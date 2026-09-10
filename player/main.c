/*
 * main.c — Stremio Accru Linux player host.
 *
 * One native window: libmpv renders into a GtkGLArea underlay that fills
 * the window; a transparent WebKitGTK webview (inside a GtkGraphicsOffload
 * overlay) sits above it and shows the Stremio web UI. The web UI drives
 * playback through the shell transport (player/ipc.c, player/preload.js):
 * every mpv-command / mpv-set-prop / mpv-observe-prop is executed against
 * libmpv and property changes / end-of-file events are pushed back.
 *
 * The streaming server, portable_config seeding and the HTTPS cert stay
 * owned by scripts/stremio-linux.sh; this host spawns the launcher with
 * --no-browser as a process-group child and terminates it on exit. The web
 * UI is loaded through the server proxy route
 * (http://127.0.0.1:11470/proxy/d=<webui>) so page and shell share an
 * origin, matching the official Linux shell's startup URL.
 *
 * Issue #58. Scope: docs/linux-scope.md (#36).
 */
#include <arpa/inet.h>
#include <errno.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ipc.h"
#include "json.h"
#include "player_mpv.h"
#include "video.h"
#include "webview.h"

#define APP_NAME "Stremio Accru"
#define SHELL_VERSION "0.1.0"
#define SERVER_PORT 11470
#define PROXY_PORT 8485
#define DEFAULT_WEBUI "https://web.stremio.com/"

typedef struct {
  gchar *data_dir;
  gchar *launcher;
  gchar *webui;
  gchar *nav_target;
  gboolean no_server;
  gboolean devtools;
  gboolean check;
} Options;

typedef struct {
  Options opt;
  GtkWidget *window;
  GtkWidget *webview;
  GtkWidget *gfx;
  SaPlayerMpv *mpv;
  GPid launcher_pid;
  gboolean launcher_spawned;
  GPid proxy_pid;
  gboolean proxy_spawned;
  gboolean app_ready;
  gboolean fullscreen;
  GQueue *out_queue; /* gchar* payloads waiting for app-ready */
  guint poll_source;
  guint server_wait_ticks;
  gboolean quitting;
} App;

static App g_app;

static void shutdown_now(void);
static void stop_launcher(App *app);
static gboolean port_open(int port);
static gchar *resolve_proxy_script(void);
static void die(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void die(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  gchar *msg = g_strdup_vprintf(fmt, ap);
  va_end(ap);
  g_printerr("%s: error: %s\n", APP_NAME, msg);
  g_free(msg);
  stop_launcher(&g_app);
  exit(1);
}

static void usage(FILE *out) {
  fprintf(out,
          "Usage: stremio-accru [options]\n"
          "\n"
          "Linux desktop player: mpv renders below the transparent web UI.\n"
          "\n"
          "Options:\n"
          "  --webui-url=URL   web UI to load (default: %s)\n"
          "  --data-dir=DIR    data dir (default: $XDG_DATA_HOME/stremio-accru)\n"
          "  --no-server       require a running streaming server\n"
          "  --devtools        open WebKit developer extras\n"
          "  --check           verify launcher/server pieces and exit\n"
          "  --help            show this help\n",
          DEFAULT_WEBUI);
}

static gchar *data_dir_default(void) {
  const gchar *xdg = g_getenv("XDG_DATA_HOME");
  if (xdg && *xdg) return g_build_filename(xdg, "stremio-accru", NULL);
  return g_build_filename(g_get_home_dir(), ".local", "share", "stremio-accru",
                          NULL);
}

static gchar *resolve_launcher(void) {
  const gchar *env = g_getenv("STREMIO_LAUNCHER");
  if (env && *env) {
    if (g_file_test(env, G_FILE_TEST_IS_EXECUTABLE)) return g_strdup(env);
  }
  gchar *exe = g_file_read_link("/proc/self/exe", NULL);
  if (exe) {
    gchar *dir = g_path_get_dirname(exe);
    gchar *cand = g_build_filename(dir, "stremio-linux.sh", NULL);
    g_free(dir);
    g_free(exe);
    if (g_file_test(cand, G_FILE_TEST_IS_EXECUTABLE)) return cand;
    g_free(cand);
  }
  gchar *cand = g_build_filename(g_get_current_dir(), "scripts",
                                 "stremio-linux.sh", NULL);
  if (g_file_test(cand, G_FILE_TEST_IS_EXECUTABLE)) return cand;
  g_free(cand);
  return g_find_program_in_path("stremio-linux.sh");
}

static Options parse_args(int argc, char **argv) {
  Options o = {0};
  const gchar *env_webui = g_getenv("STREMIO_WEBUI_URL");
  o.webui = (env_webui && *env_webui) ? g_strdup(env_webui) : NULL;
  const gchar *env_data = g_getenv("STREMIO_DATA_DIR");
  o.data_dir = (env_data && *env_data) ? g_strdup(env_data) : NULL;

  for (int i = 1; i < argc; i++) {
    const gchar *a = argv[i];
    if (g_str_has_prefix(a, "--webui-url=")) {
      g_free(o.webui);
      o.webui = g_strdup(a + strlen("--webui-url="));
    } else if (g_str_has_prefix(a, "--data-dir=")) {
      g_free(o.data_dir);
      o.data_dir = g_strdup(a + strlen("--data-dir="));
    } else if (strcmp(a, "--no-server") == 0) {
      o.no_server = TRUE;
    } else if (strcmp(a, "--devtools") == 0) {
      o.devtools = TRUE;
    } else if (strcmp(a, "--check") == 0) {
      o.check = TRUE;
    } else if (strcmp(a, "--help") == 0) {
      usage(stdout);
      exit(0);
    } else {
      die("unknown option: %s (see --help)", a);
    }
  }
  if (!o.data_dir) o.data_dir = data_dir_default();
  o.data_dir = g_canonicalize_filename(o.data_dir, NULL);
  if (!o.webui) o.webui = g_strdup(DEFAULT_WEBUI);
  o.launcher = resolve_launcher();
  return o;
}

/* ---------------- streaming server / launcher ---------------- */

static void child_setup(gpointer data) {
  (void)data;
  setpgid(0, 0); /* own process group so we can kill the whole tree */
}

static void spawn_launcher(App *app) {
  gchar *dir_arg = g_strdup_printf("--data-dir=%s", app->opt.data_dir);
  gchar *argv[] = {(gchar *)app->opt.launcher, (gchar *)"--no-browser",
                   dir_arg, NULL};
  gchar **envp = g_environ_setenv(g_get_environ(), "SERVER_IPC_KEY", "LINUX",
                                  TRUE);
  GError *err = NULL;
  if (g_spawn_async(NULL, argv, envp, G_SPAWN_DO_NOT_REAP_CHILD,
                    child_setup, NULL, &app->launcher_pid, &err)) {
    app->launcher_spawned = TRUE;
    g_message("%s: streaming server starting (pid %d)", APP_NAME,
              (int)app->launcher_pid);
  } else {
    g_printerr("%s: launcher failed: %s\n", APP_NAME,
               err ? err->message : "unknown");
    g_clear_error(&err);
  }
  g_strfreev(envp);
  g_free(dir_arg);
}
static gboolean server_port_open(void) { return port_open(SERVER_PORT); }

static void spawn_proxy(App *app) {
  gchar *script = resolve_proxy_script();
  if (!script) {
    g_warning("proxy.js not found — the UI will load directly (server calls "
              "may be blocked)");
    return;
  }
  gchar *port_s = g_strdup_printf("%d", PROXY_PORT);
  gchar *argv[] = {g_find_program_in_path("node"), script, port_s,
                   (gchar *)app->opt.webui, NULL};
  if (!argv[0]) {
    g_warning("node not found — UI proxy disabled");
    g_free(script);
    g_free(port_s);
    return;
  }
  GError *err = NULL;
  if (g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, child_setup,
                    NULL, &app->proxy_pid, &err)) {
    app->proxy_spawned = TRUE;
    g_message("%s: UI proxy starting (pid %d)", APP_NAME,
              (int)app->proxy_pid);
  } else {
    g_printerr("%s: UI proxy failed: %s\n", APP_NAME,
               err ? err->message : "unknown");
    g_clear_error(&err);
  }
  g_free(script);
  g_free(port_s);
}

static void stop_launcher(App *app) {
  if (app->proxy_spawned) {
    kill(-app->proxy_pid, SIGTERM);
    g_usleep(100 * 1000);
    kill(-app->proxy_pid, SIGKILL);
    int st;
    while (waitpid(app->proxy_pid, &st, WNOHANG) == 0 && errno != ECHILD)
      g_usleep(50 * 1000);
    app->proxy_spawned = FALSE;
  }
  if (!app->launcher_spawned) return;
  kill(-app->launcher_pid, SIGTERM);
  g_usleep(250 * 1000);
  kill(-app->launcher_pid, SIGKILL);
  int status;
  while (waitpid(app->launcher_pid, &status, WNOHANG) == 0) {
    if (errno == ECHILD) break;
    g_usleep(50 * 1000);
  }
  app->launcher_spawned = FALSE;
}

static gchar *resolve_proxy_script(void) {
  const gchar *env = g_getenv("STREMIO_PROXY_JS");
  if (env && *env && g_file_test(env, G_FILE_TEST_IS_REGULAR))
    return g_strdup(env);
  gchar *exe = g_file_read_link("/proc/self/exe", NULL);
  if (exe) {
    gchar *dir = g_path_get_dirname(exe);
    g_free(exe);
    gchar *cands[2] = {
        g_build_filename(dir, "..", "share", "stremio-accru", "proxy.js",
                         NULL),
        g_build_filename(dir, "proxy.js", NULL),
    };
    for (int i = 0; i < 2; i++) {
      if (g_file_test(cands[i], G_FILE_TEST_IS_REGULAR)) {
        gchar *c = g_canonicalize_filename(cands[i], NULL);
        g_free(cands[0]);
        g_free(cands[1]);
        g_free(dir);
        return c;
      }
    }
    /* cands lives on the stack: free the strings, never g_strfreev. */
    g_free(cands[0]);
    g_free(cands[1]);
    g_free(dir);
  }
  gchar *cwd = g_build_filename(g_get_current_dir(), "player", "proxy.js",
                                NULL);
  if (g_file_test(cwd, G_FILE_TEST_IS_REGULAR)) return cwd;
  g_free(cwd);
  return NULL;
}

static gboolean port_open(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return FALSE;
  struct sockaddr_in sa = {0};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int rc = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
  close(fd);
  return rc == 0;
}

/* ---------------- window geometry ---------------- */

static void load_geometry(int *w, int *h) {
  gchar *path = g_build_filename(g_app.opt.data_dir, "window.json", NULL);
  gchar *text = NULL;
  if (g_file_get_contents(path, &text, NULL, NULL)) {
    SaJson *j = sa_json_parse(text, -1);
    if (j && j->type == SA_J_OBJECT) {
      const SaJson *v;
      if ((v = sa_json_object_get(j, "width")) && v->type == SA_J_NUMBER)
        *w = (int)v->u.num;
      if ((v = sa_json_object_get(j, "height")) && v->type == SA_J_NUMBER)
        *h = (int)v->u.num;
    }
    sa_json_free(j);
    g_free(text);
  }
  g_free(path);
}

static void save_geometry(void) {
  int width = gtk_widget_get_width(g_app.window);
  int height = gtk_widget_get_height(g_app.window);
  if (width <= 1) width = 1280;
  if (height <= 1) height = 800;
  SaJson *j = sa_json_new_object();
  sa_json_object_set(j, "width", sa_json_new_number(width));
  sa_json_object_set(j, "height", sa_json_new_number(height));
  gchar *dump = sa_json_dump(j);
  sa_json_free(j);
  g_mkdir_with_parents(g_app.opt.data_dir, 0755);
  gchar *path = g_build_filename(g_app.opt.data_dir, "window.json", NULL);
  g_file_set_contents(path, dump, -1, NULL);
  g_free(path);
  g_free(dump);
}

/* ---------------- transport ---------------- */

static void send_event(const gchar *name, const SaJson *payload) {
  gchar *msg = sa_ipc_event_message(name, payload);
  if (g_app.app_ready) {
    sa_webview_send(g_app.webview, msg);
  } else {
    if (g_queue_get_length(g_app.out_queue) < 512)
      g_queue_push_tail(g_app.out_queue, msg);
    else {
      g_free(msg);
      g_warning("transport: dropping outbound %s (queue full)", name);
    }
  }
}

static void send_visibility(void) {
  SaJson *payload = sa_json_new_object();
  sa_json_object_set(payload, "visible", sa_json_new_bool(TRUE));
  sa_json_object_set(payload, "visibility", sa_json_new_number(1));
  sa_json_object_set(payload, "isFullscreen",
                     sa_json_new_bool(g_app.fullscreen));
  send_event("win-visibility-changed", payload);
  sa_json_free(payload);
}

static void flush_queue(void) {
  gchar *msg;
  while ((msg = g_queue_pop_head(g_app.out_queue)) != NULL) {
    sa_webview_send(g_app.webview, msg);
    g_free(msg);
  }
}

static void on_mpv_property(void *ud, const gchar *name, SaJson *value) {
  (void)ud;
  SaJson *payload = sa_json_new_object();
  sa_json_object_set(payload, "name", sa_json_new_string(name));
  sa_json_object_set(payload, "data", sa_json_clone(value));
  send_event("mpv-prop-change", payload);
  sa_json_free(payload);
}

static void on_mpv_endfile(void *ud, const gchar *reason) {
  (void)ud;
  SaJson *payload = sa_json_new_object();
  sa_json_object_set(payload, "reason", sa_json_new_string(reason));
  sa_json_object_set(payload, "error", sa_json_new_null());
  send_event("mpv-event-ended", payload);
  sa_json_free(payload);
  g_message("player: playback ended (%s)", reason);
}

static const char *kCommandAllowlist[] = {"loadfile", "sub-add", "keypress",
                                          "stop", "script-message-to", "cycle",
                                          NULL};

static gboolean command_allowed(const gchar *cmd) {
  for (int i = 0; kCommandAllowlist[i]; i++)
    if (strcmp(kCommandAllowlist[i], cmd) == 0) return TRUE;
  return FALSE;
}

static void dispatch_event(const gchar *name, const SaJson *data) {
  if (strcmp(name, "app-ready") == 0) {
    g_message("transport: app ready");
    g_app.app_ready = TRUE;
    flush_queue();
    send_visibility();
    /* Manual QA hooks (undocumented env, keep the normal path clean):
     *   STREMIO_DEBUG_LOADFILE=<url> starts playback through the normal
     *     mpv path once the UI signals ready;
     *   STREMIO_DEBUG_HIDE_UI=1 hides the webview so the mpv underlay is
     *     visible for screenshots. */
    const gchar *dbg = g_getenv("STREMIO_DEBUG_LOADFILE");
    if (dbg && *dbg) {
      const gchar *load[] = {"loadfile", dbg, NULL};
      sa_mpv_commandv(g_app.mpv, load);
    }
    if (g_getenv("STREMIO_DEBUG_HIDE_UI")) gtk_widget_set_visible(g_app.gfx, FALSE);
  } else if (strcmp(name, "quit") == 0) {
    g_message("transport: quit requested by UI");
    shutdown_now();
  } else if (strcmp(name, "win-set-visibility") == 0) {
    const SaJson *fs = data ? sa_json_object_get(data, "fullscreen") : NULL;
    gboolean state = fs && fs->type == SA_J_BOOL && fs->u.b;
    if (state != g_app.fullscreen) {
      g_app.fullscreen = state;
      if (state)
        gtk_window_fullscreen(GTK_WINDOW(g_app.window));
      else
        gtk_window_unfullscreen(GTK_WINDOW(g_app.window));
    }
    send_visibility();
  } else if (strcmp(name, "mpv-command") == 0) {
    if (!data || data->type != SA_J_ARRAY || data->u.arr->len == 0) return;
    const SaJson *cmd = g_ptr_array_index(data->u.arr, 0);
    if (cmd->type != SA_J_STRING) return;
    if (!command_allowed(cmd->u.str)) {
      g_warning("transport: blocked mpv-command %s", cmd->u.str);
      return;
    }
    GPtrArray *argv = g_ptr_array_new();
    for (guint i = 0; i < data->u.arr->len; i++) {
      const SaJson *a = g_ptr_array_index(data->u.arr, i);
      if (a->type == SA_J_STRING)
        g_ptr_array_add(argv, (gchar *)a->u.str);
      else {
        gchar *tmp = sa_json_dump(a); /* rare non-string arg */
        g_ptr_array_add(argv, tmp);
      }
    }
    g_ptr_array_add(argv, NULL);
    sa_mpv_commandv(g_app.mpv, (const gchar *const *)argv->pdata);
    for (guint i = 0; i < argv->len - 1; i++) {
      const SaJson *a = i < data->u.arr->len
                            ? g_ptr_array_index(data->u.arr, i)
                            : NULL;
      if (!a || a->type != SA_J_STRING) g_free(g_ptr_array_index(argv, i));
    }
    g_ptr_array_free(argv, TRUE);
  } else if (strcmp(name, "mpv-set-prop") == 0) {
    if (!data || data->type != SA_J_ARRAY || data->u.arr->len < 2) return;
    const SaJson *pn = g_ptr_array_index(data->u.arr, 0);
    const SaJson *pv = g_ptr_array_index(data->u.arr, 1);
    if (pn->type != SA_J_STRING) return;
    sa_mpv_set_property_value(g_app.mpv, pn->u.str, pv);
  } else if (strcmp(name, "mpv-observe-prop") == 0) {
    if (!data || data->type != SA_J_STRING) return;
    sa_mpv_observe(g_app.mpv, data->u.str);
  } else if (strcmp(name, "media.status") == 0 ||
             strcmp(name, "media.metadata") == 0) {
    /* No MPRIS in v1; accepted and ignored. */
  } else if (strcmp(name, "discord-connect") == 0 ||
             strcmp(name, "discord-disconnect") == 0) {
    SaJson *p = sa_json_new_object();
    sa_json_object_set(p, "connected", sa_json_new_bool(FALSE));
    send_event("discord-status", p);
    sa_json_free(p);
  } else {
    g_message("transport: unhandled event %s", name);
  }
}

static void on_ipc_message(void *ud, const gchar *raw) {
  (void)ud;
  SaInbound *in = sa_ipc_parse(raw);
  if (!in) {
    g_message("transport: ignoring malformed message");
    return;
  }
  if (in->type == 3) {
    /* Page asks for the transport descriptor. Gate outgoing messages again:
     * the page re-initializes after every navigation. */
    gchar *init = sa_ipc_init_message(SHELL_VERSION);
    g_app.app_ready = FALSE;
    sa_webview_send(g_app.webview, init);
    g_free(init);
  } else if (in->type == 6 && in->name) {
    dispatch_event(in->name, in->data);
  }
  sa_ipc_inbound_free(in);
}

/* ---------------- navigation ---------------- */

static gboolean navigate_now(gpointer data) {
  (void)data;
  g_app.poll_source = 0;
  g_message("%s: opening %s", APP_NAME, g_app.opt.nav_target);
  sa_webview_load_uri(g_app.webview, g_app.opt.nav_target);
  return G_SOURCE_REMOVE;
}

static gboolean poll_startup(gpointer data) {
  (void)data;
  gboolean need_server = g_app.launcher_spawned || !g_app.opt.no_server;
  gboolean srv = !need_server || port_open(SERVER_PORT);
  gboolean prx = !g_app.proxy_spawned || port_open(PROXY_PORT);
  if (srv && prx) {
    g_message("server: up");
    if (g_app.poll_source) g_source_remove(g_app.poll_source);
    g_app.poll_source = g_idle_add(navigate_now, NULL);
    return G_SOURCE_REMOVE;
  }
  if (++g_app.server_wait_ticks > 45 * 4) /* 45 s at 250 ms */
    die("streaming server did not come up (%s)", g_app.opt.data_dir);
  return G_SOURCE_CONTINUE;
}

/* ---------------- lifecycle ---------------- */

static void shutdown_now(void) {
  if (g_app.quitting) return;
  g_app.quitting = TRUE;
  if (g_app.poll_source) g_source_remove(g_app.poll_source);
  save_geometry();
  stop_launcher(&g_app);
  if (g_app.mpv) {
    sa_mpv_free(g_app.mpv);
    g_app.mpv = NULL;
  }
  if (g_main_loop_is_running(NULL)) g_main_loop_quit(NULL);
}

static gboolean on_window_close(GtkWidget *window, gpointer data) {
  (void)window;
  (void)data;
  shutdown_now();
  return GDK_EVENT_PROPAGATE;
}

static void on_signal(int sig) {
  (void)sig;
  shutdown_now();
}

static gboolean run_check(const Options *o) {
  if (!o->launcher) die("stremio-linux.sh not found (set STREMIO_LAUNCHER)");
  gchar *dir_arg = g_strdup_printf("--data-dir=%s", o->data_dir);
  gchar *argv[] = {(gchar *)o->launcher, (gchar *)"--check", dir_arg, NULL};
  gchar *stdout_buf = NULL, *stderr_buf = NULL;
  gint status = 0;
  gboolean ok = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                             &stdout_buf, &stderr_buf, &status, NULL);
  if (stdout_buf) {
    g_print("%s", stdout_buf);
    g_free(stdout_buf);
  }
  if (stderr_buf) {
    g_printerr("%s", stderr_buf);
    g_free(stderr_buf);
  }
  g_free(dir_arg);
  if (!ok) return FALSE;
  g_message("server port %d: %s", SERVER_PORT,
            server_port_open() ? "up" : "down (the app will start it)");
  return g_spawn_check_wait_status(status, NULL);
}

static void wait_for_seed_or_timeout(guint seconds) {
  /* The launcher seeds portable_config before starting the server; mpv needs
   * config-dir present at initialize. This returns as soon as the config
   * exists (usually immediate on warm data dirs). */
  gchar *pc_dir =
      g_build_filename(g_app.opt.data_dir, "portable_config", NULL);
  gchar *mpv_conf = g_build_filename(pc_dir, "mpv.conf", NULL);
  for (guint i = 0; i < seconds * 10; i++) {
    if (g_file_test(mpv_conf, G_FILE_TEST_EXISTS)) break;
    g_usleep(100 * 1000);
  }
  g_free(mpv_conf);
  g_free(pc_dir);
}

int main(int argc, char **argv) {
  /* mpv refuses to start unless the numeric locale is C (it prints the
   * remediation itself). Keep message localization untouched. */
  setlocale(LC_NUMERIC, "C");
  setvbuf(stdout, NULL, _IONBF, 0);
  g_app.opt = parse_args(argc, argv);

  if (g_app.opt.check) return run_check(&g_app.opt) ? 0 : 1;

  gtk_init();
  g_app.out_queue = g_queue_new();

  /* Dark backdrop behind the transparent webview / below the video layer. */
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_string(css, "window { background: #0b0d12; }");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);

  g_mkdir_with_parents(g_app.opt.data_dir, 0755);
  g_message("%s: data dir %s", APP_NAME, g_app.opt.data_dir);

  /* Server + seed. */
  if (g_app.opt.no_server) {
    if (!port_open(SERVER_PORT))
      die("server down at port %d (--no-server)", SERVER_PORT);
  } else {
    if (!g_app.opt.launcher)
      die("stremio-linux.sh not found (set STREMIO_LAUNCHER)");
    spawn_launcher(&g_app);
    wait_for_seed_or_timeout(30);
  }
  /* Local http origin for the UI (mixed-content workaround). */
  spawn_proxy(&g_app);

  /* mpv first: the video underlay needs the handle at realize time. */
  {
    /* GTK/GLib may restore the user locale during gtk_init; mpv requires
     * a C numeric locale or mpv_create() fails. */
    setlocale(LC_NUMERIC, "C");
    gchar *pc_dir =
        g_build_filename(g_app.opt.data_dir, "portable_config", NULL);
    gchar *mpv_conf = g_build_filename(pc_dir, "mpv.conf", NULL);
    gboolean have = g_file_test(mpv_conf, G_FILE_TEST_EXISTS);
    g_free(mpv_conf);
    GError *err = NULL;
    g_app.mpv = sa_mpv_new(have ? pc_dir : NULL, &err);
    g_free(pc_dir);
    if (!g_app.mpv) die("mpv init failed: %s", err ? err->message : "?");
    g_clear_error(&err);
  }
  sa_mpv_set_property_cb(g_app.mpv, on_mpv_property, NULL);
  sa_mpv_set_endfile_cb(g_app.mpv, on_mpv_endfile, NULL);
  sa_mpv_start(g_app.mpv);

  /* Window: video underlay below a transparent webview overlay. */
  int w = 1280, h = 800;
  load_geometry(&w, &h);
  g_app.window = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(g_app.window), APP_NAME);
  gtk_window_set_default_size(GTK_WINDOW(g_app.window), w, h);
  g_signal_connect(g_app.window, "close-request",
                   G_CALLBACK(on_window_close), NULL);

  GtkWidget *overlay = gtk_overlay_new();
  GtkWidget *video = sa_video_new(g_app.mpv);
  gtk_overlay_set_child(GTK_OVERLAY(overlay), video);

  g_app.webview = sa_webview_new(g_app.opt.devtools);
  sa_webview_set_ipc_handler(g_app.webview, on_ipc_message, NULL);
  g_app.gfx = gtk_graphics_offload_new(g_app.webview);
  gtk_widget_set_hexpand(g_app.gfx, TRUE);
  gtk_widget_set_vexpand(g_app.gfx, TRUE);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), g_app.gfx);

  gtk_window_set_child(GTK_WINDOW(g_app.window), overlay);
  gtk_window_present(GTK_WINDOW(g_app.window));

  /* Navigate once the server answers. The UI loads from the local http
   * proxy (see proxy.js); without it the page falls back to the direct
   * https UI where WebKit blocks its http loopback server calls. */
  if (g_app.proxy_spawned)
    g_app.opt.nav_target =
        g_strdup_printf("http://127.0.0.1:%d/", PROXY_PORT);
  else
    g_app.opt.nav_target = g_strdup(g_app.opt.webui);
  g_app.poll_source =
      g_app.proxy_spawned ? g_timeout_add(250, poll_startup, NULL)
                          : (port_open(SERVER_PORT) || g_app.opt.no_server
                                 ? g_idle_add(navigate_now, NULL)
                                 : g_timeout_add(250, poll_startup, NULL));

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  stop_launcher(&g_app);
  if (g_app.mpv) sa_mpv_free(g_app.mpv);
  return 0;
}
