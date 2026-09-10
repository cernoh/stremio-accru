/*
 * webview.c — see webview.h.
 *
 * Script injection mirrors the preload behavior of the official Linux
 * shell: one user script at document start in the top frame defines
 * window.ipc / window.qt / window.chrome.webview so the Stremio web UI
 * enters shell mode and talks the Qt-WebChannel wire contract.
 */
#include "webview.h"

#include <webkit/webkit.h>

#include "json.h"

/* Generated from preload.js at build time (see gen_preload.sh). */
#include "preload.inc"

typedef struct {
  void (*handler)(void *ud, const gchar *msg);
  void *ud;
} IpcHandler;

static IpcHandler *get_handler(GtkWidget *wv) {
  return g_object_get_data(G_OBJECT(wv), "sa-ipc-handler");
}

static void on_script_message_received(WebKitUserContentManager *manager,
                                       JSCValue *value, gpointer user_data) {
  GtkWidget *wv = user_data;
  (void)manager;
  IpcHandler *h = get_handler(wv);
  if (!h || !h->handler) return;

  gchar *str = jsc_value_to_string(value);
  if (str) {
    h->handler(h->ud, str);
    g_free(str);
  }
}

static void on_decide_policy(WebKitWebView *web_view,
                             WebKitPolicyDecision *decision,
                             WebKitPolicyDecisionType type,
                             gpointer user_data) {
  GtkWidget *wv = user_data;
  (void)web_view;
  (void)wv;

  if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
    webkit_policy_decision_ignore(decision);
    WebKitNavigationAction *action =
        webkit_navigation_policy_decision_get_navigation_action(
            WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    WebKitURIRequest *request = webkit_navigation_action_get_request(action);
    const gchar *uri = webkit_uri_request_get_uri(request);
    if (uri && (g_str_has_prefix(uri, "http://") ||
                g_str_has_prefix(uri, "https://"))) {
      /* Open in the system browser (xdg-utils is a launcher dependency). */
      gchar *cmd = g_strdup_printf("xdg-open %s", g_shell_quote(uri));
      g_spawn_command_line_async(cmd, NULL);
      g_free(cmd);
    }
    return;
  }

  if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
    WebKitNavigationAction *action =
        webkit_navigation_policy_decision_get_navigation_action(
            WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    WebKitURIRequest *request = webkit_navigation_action_get_request(action);
    const gchar *uri = webkit_uri_request_get_uri(request);
    if (!uri || !(g_str_has_prefix(uri, "http://") ||
                  g_str_has_prefix(uri, "https://") ||
                  g_str_has_prefix(uri, "about:") ||
                  g_str_has_prefix(uri, "data:"))) {
      /* Block custom-scheme main-frame navigation (stremio://, magnet:). */
      webkit_policy_decision_ignore(decision);
    }
  }
}

static const gchar *load_event_name(WebKitLoadEvent e) {
  switch (e) {
    case WEBKIT_LOAD_STARTED: return "started";
    case WEBKIT_LOAD_REDIRECTED: return "redirected";
    case WEBKIT_LOAD_COMMITTED: return "committed";
    case WEBKIT_LOAD_FINISHED: return "finished";
  }
  return "?";
}

static void on_load_changed(WebKitWebView *wv, WebKitLoadEvent event,
                            gpointer data) {
  (void)wv;
  (void)data;
  g_message("webview: load %s", load_event_name(event));
}

static gboolean on_load_failed(WebKitWebView *wv, WebKitLoadEvent event,
                               gchar *failing_uri, GError *error,
                               gpointer data) {
  (void)wv;
  (void)event;
  (void)data;
  g_warning("webview: load failed %s: %s", failing_uri,
            error ? error->message : "?");
  return FALSE;
}

GtkWidget *sa_webview_new(gboolean devtools) {
  GtkWidget *wv = webkit_web_view_new();

  GdkRGBA transparent = {0, 0, 0, 0};
  webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(wv), &transparent);

  WebKitSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(wv));
  /* The streaming server's HTTPS leg (127.0.0.1:12470) uses the loopback
   * cert from api.strem.io, which is not in the system trust store;
   * desktop shells must accept it. Only loopback content is loaded. */
  WebKitNetworkSession *session =
      webkit_web_view_get_network_session(WEBKIT_WEB_VIEW(wv));
  webkit_network_session_set_tls_errors_policy(
      session, WEBKIT_TLS_ERRORS_POLICY_IGNORE);
  webkit_settings_set_enable_media(settings, FALSE);
  webkit_settings_set_enable_media_capabilities(settings, FALSE);
  webkit_settings_set_enable_media_stream(settings, FALSE);
  webkit_settings_set_enable_webaudio(settings, FALSE);
  webkit_settings_set_enable_developer_extras(settings, devtools);
  webkit_settings_set_enable_write_console_messages_to_stdout(
      settings, devtools || g_getenv("STREMIO_DEBUG_LOADFILE") ||
                     g_getenv("STREMIO_DEBUG_HIDE_UI") != NULL);

  g_signal_connect(wv, "load-changed", G_CALLBACK(on_load_changed), NULL);
  g_signal_connect(wv, "load-failed", G_CALLBACK(on_load_failed), NULL);
  /* Console output reaches stderr via the settings flag above. */

  /* Document-start transport shim (preload.inc is generated from
   * preload.js at build time). */
  WebKitUserContentManager *manager =
      webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(wv));
  WebKitUserScript *script = webkit_user_script_new(
      SA_PRELOAD_JS, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
      WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL);
  webkit_user_content_manager_add_script(manager, script);
  webkit_user_script_unref(script);

  /* Connect before registering the handler name (recommended ordering). */
  g_signal_connect(manager, "script-message-received::ipc",
                   G_CALLBACK(on_script_message_received), wv);
  webkit_user_content_manager_register_script_message_handler(manager, "ipc",
                                                              NULL);

  g_signal_connect(wv, "decide-policy", G_CALLBACK(on_decide_policy), wv);
  return wv;
}

void sa_webview_load_uri(GtkWidget *wv, const gchar *uri) {
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(wv), uri);
}

void sa_webview_send(GtkWidget *wv, const gchar *message) {
  /* __postMessage("<message>") with JSON string escaping. */
  SaJson *lit = sa_json_new_string(message);
  gchar *escaped = sa_json_dump(lit);
  sa_json_free(lit);
  gchar *script = g_strdup_printf("__postMessage(%s)", escaped);
  g_free(escaped);
  webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(wv), script, -1, NULL,
                                      NULL, NULL, NULL, NULL);
  g_free(script);
}

void sa_webview_set_ipc_handler(GtkWidget *wv,
                                void (*handler)(void *ud, const gchar *msg),
                                void *ud) {
  IpcHandler *h = g_new0(IpcHandler, 1);
  h->handler = handler;
  h->ud = ud;
  g_object_set_data_full(G_OBJECT(wv), "sa-ipc-handler", h, g_free);
}
