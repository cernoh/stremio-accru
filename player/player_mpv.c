/*
 * player_mpv.c — see player_mpv.h.
 *
 * Property groups and formats mirror the shell contract the web UI uses
 * (Stremio/stremio-linux-shell src/app/video/config.rs).
 */
#include "player_mpv.h"

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "json.h"

static GQuark sa_error_quark(void);

typedef struct {
  gchar *name;
  gint format; /* MPV_FORMAT_DOUBLE / FLAG / STRING */
} Prop;

static const Prop kFloatProps[] = {
    {"time-pos", MPV_FORMAT_DOUBLE},          {"duration", MPV_FORMAT_DOUBLE},
    {"volume", MPV_FORMAT_DOUBLE},            {"speed", MPV_FORMAT_DOUBLE},
    {"sub-pos", MPV_FORMAT_DOUBLE},           {"sub-scale", MPV_FORMAT_DOUBLE},
    {"sub-delay", MPV_FORMAT_DOUBLE},         {"cache-buffering-state", MPV_FORMAT_DOUBLE},
    {"demuxer-cache-time", MPV_FORMAT_DOUBLE},{"panscan", MPV_FORMAT_DOUBLE},
};
static const Prop kBoolProps[] = {
    {"pause", MPV_FORMAT_FLAG},               {"buffering", MPV_FORMAT_FLAG},
    {"seeking", MPV_FORMAT_FLAG},             {"osc", MPV_FORMAT_FLAG},
    {"input-default-bindings", MPV_FORMAT_FLAG},
    {"input-vo-keyboard", MPV_FORMAT_FLAG},
    {"eof-reached", MPV_FORMAT_FLAG},         {"paused-for-cache", MPV_FORMAT_FLAG},
    {"keepaspect", MPV_FORMAT_FLAG},
};
static const Prop kStringProps[] = {
    {"path", MPV_FORMAT_STRING},              {"mpv-version", MPV_FORMAT_STRING},
    {"ffmpeg-version", MPV_FORMAT_STRING},    {"hwdec", MPV_FORMAT_STRING},
    {"track-list", MPV_FORMAT_STRING},        {"sub-color", MPV_FORMAT_STRING},
    {"sub-back-color", MPV_FORMAT_STRING},    {"sub-border-color", MPV_FORMAT_STRING},
    {"sid", MPV_FORMAT_STRING},               {"aid", MPV_FORMAT_STRING},
    {"vid", MPV_FORMAT_STRING},               {"mute", MPV_FORMAT_STRING},
    {"metadata", MPV_FORMAT_STRING},          {"video-params", MPV_FORMAT_STRING},
    {"sub-ass-override", MPV_FORMAT_STRING},
};

static const Prop *find_prop(const gchar *name) {
  for (guint i = 0; i < G_N_ELEMENTS(kFloatProps); i++)
    if (strcmp(kFloatProps[i].name, name) == 0) return &kFloatProps[i];
  for (guint i = 0; i < G_N_ELEMENTS(kBoolProps); i++)
    if (strcmp(kBoolProps[i].name, name) == 0) return &kBoolProps[i];
  for (guint i = 0; i < G_N_ELEMENTS(kStringProps); i++)
    if (strcmp(kStringProps[i].name, name) == 0) return &kStringProps[i];
  return NULL;
}

struct SaPlayerMpv {
  mpv_handle *mpv;
  gchar *config_dir;
  gboolean running;
  GThread *thread;
  GMutex lock;
  SaMpvPropertyFn on_property;
  void *on_property_ud;
  SaMpvEndfileFn on_endfile;
  void *on_endfile_ud;
  mpv_render_context *render_ctx;
  gpointer draw_widget;
};

static GQuark sa_error_quark(void) {
  return g_quark_from_static_string("sa-player");
}

static void log_mpv_error(const gchar *what, int rc) {
  g_warning("player: %s: %s", what, mpv_error_string(rc));
}

static void set_option_str(SaPlayerMpv *s, const gchar *name, const gchar *v,
                           GError **err) {
  int rc = mpv_set_option_string(s->mpv, name, v);
  if (rc < 0 && err && !*err)
    *err = g_error_new(sa_error_quark(), rc, "mpv option %s=%s: %s", name, v,
                       mpv_error_string(rc));
}

SaPlayerMpv *sa_mpv_new(const gchar *config_dir, GError **err) {
  SaPlayerMpv *s = g_new0(SaPlayerMpv, 1);
  s->config_dir = g_strdup(config_dir);
  s->mpv = mpv_create();
  if (!s->mpv) {
    g_set_error(err, sa_error_quark(), 0, "mpv_create failed");
    sa_mpv_free(s);
    return NULL;
  }
  g_mutex_init(&s->lock);

  set_option_str(s, "config", "yes", err);
  set_option_str(s, "load-scripts", "yes", err);
  if (config_dir) set_option_str(s, "config-dir", config_dir, err);
  /* Embedded rendering: no window of our own; frames go to the render API. */
  set_option_str(s, "vo", "libmpv", err);
  set_option_str(s, "terminal", "yes", err);
  set_option_str(s, "msg-level", "all=warn", err);
  set_option_str(s, "audio-client-name", "stremio-accru", err);

  if (mpv_initialize(s->mpv) < 0) {
    g_set_error(err, sa_error_quark(), 0, "mpv_initialize failed");
    sa_mpv_free(s);
    return NULL;
  }

  /* Streaming-friendly demuxer options (mirror the values the upstream
   * Windows shell sets; mpv 0.41 accepts these). */
  mpv_set_property_string(s->mpv, "demuxer-lavf-probesize", "524288");
  mpv_set_property_string(s->mpv, "demuxer-lavf-analyzeduration", "0.5");
  mpv_set_property_string(s->mpv, "audio-fallback-to-null", "yes");
  mpv_set_property_string(s->mpv, "vd-lavc-threads", "0");
  return s;
}

void sa_mpv_free(SaPlayerMpv *s) {
  if (!s) return;
  if (s->thread && s->thread != g_thread_self()) {
    s->running = FALSE;
    g_thread_join(s->thread);
  }
  if (s->mpv) mpv_terminate_destroy(s->mpv);
  g_free(s->config_dir);
  g_mutex_clear(&s->lock);
  g_free(s);
}

void sa_mpv_set_property_cb(SaPlayerMpv *s, SaMpvPropertyFn fn, void *ud) {
  s->on_property = fn;
  s->on_property_ud = ud;
}
void sa_mpv_set_endfile_cb(SaPlayerMpv *s, SaMpvEndfileFn fn, void *ud) {
  s->on_endfile = fn;
  s->on_endfile_ud = ud;
}

mpv_handle *sa_mpv_handle(SaPlayerMpv *s) { return s->mpv; }

/* ---------------- event marshalling ---------------- */

typedef struct {
  SaPlayerMpv *s;
  gchar *name;
  SaJson *value; /* NULL when the property changed to an unusable form */
  gchar *reason; /* end-file reason; non-NULL for end-file events */
} PendingEvent;

static gboolean deliver_event(gpointer data) {
  PendingEvent *ev = data;
  if (ev->reason) {
    if (ev->s->on_endfile)
      ev->s->on_endfile(ev->s->on_endfile_ud, ev->reason);
  } else if (ev->name) {
    if (ev->s->on_property && ev->value)
      ev->s->on_property(ev->s->on_property_ud, ev->name, ev->value);
    else
      sa_json_free(ev->value);
  }
  g_free(ev->name);
  g_free(ev->reason);
  g_free(ev);
  return G_SOURCE_REMOVE;
}

static SaJson *string_prop_to_json(const gchar *raw) {
  /* mpv emits JSON for structured string properties (track-list,
   * metadata, video-params). Embed parsed JSON when possible. */
  SaJson *parsed = sa_json_parse(raw, -1);
  if (parsed &&
      (parsed->type == SA_J_ARRAY || parsed->type == SA_J_OBJECT ||
       parsed->type == SA_J_STRING || parsed->type == SA_J_NUMBER ||
       parsed->type == SA_J_BOOL))
    return parsed;
  sa_json_free(parsed);
  return sa_json_new_string(raw ? raw : "");
}

static SaJson *value_from_prop(const mpv_event_property *prop) {
  switch (prop->format) {
    case MPV_FORMAT_DOUBLE:
      return prop->data ? sa_json_new_number(*(double *)prop->data) : NULL;
    case MPV_FORMAT_INT64:
      return prop->data ? sa_json_new_number((double)*(gint64 *)prop->data)
                        : NULL;
    case MPV_FORMAT_FLAG:
      return prop->data ? sa_json_new_bool(*(int *)prop->data != 0) : NULL;
    case MPV_FORMAT_STRING: {
      if (!prop->data) return NULL;
      const char *s = *(char **)prop->data;
      return string_prop_to_json(s ? s : "");
    }
    default:
      return NULL;
  }
}

static const gchar *endfile_reason(mpv_end_file_reason reason) {
  switch ((int)reason) {
    case MPV_END_FILE_REASON_EOF:
      return "eof";
    case MPV_END_FILE_REASON_STOP:
      return "stop";
    case MPV_END_FILE_REASON_QUIT:
      return "quit";
    case MPV_END_FILE_REASON_ERROR:
      return "error";
    case MPV_END_FILE_REASON_REDIRECT:
      return "redirect";
    default:
      return "other";
  }
}

static gpointer event_thread(gpointer data) {
  SaPlayerMpv *s = data;
  while (s->running) {
    mpv_event *ev = mpv_wait_event(s->mpv, 0.05);
    if (!ev || ev->event_id == MPV_EVENT_NONE) continue;
    PendingEvent *pe = NULL;
    switch (ev->event_id) {
      case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = ev->data;
        if (!prop || !prop->name || ev->error < 0) break;
        pe = g_new0(PendingEvent, 1);
        pe->s = s;
        pe->name = g_strdup(prop->name);
        pe->value = value_from_prop(prop);
        break;
      }
      case MPV_EVENT_END_FILE: {
        mpv_event_end_file *ef = ev->data;
        pe = g_new0(PendingEvent, 1);
        pe->s = s;
        pe->reason = g_strdup(endfile_reason(
            ef ? (mpv_end_file_reason)ef->reason : MPV_END_FILE_REASON_EOF));
        break;
      }
      case MPV_EVENT_SHUTDOWN:
        s->running = FALSE;
        return NULL;
      default:
        break;
    }
    if (pe) g_idle_add(deliver_event, pe);
  }
  return NULL;
}

void sa_mpv_start(SaPlayerMpv *s) {
  if (s->running) return;
  s->running = TRUE;
  s->thread = g_thread_new("sa-mpv-events", event_thread, s);
}

/* ---------------- commands / properties / render ---------------- */

void sa_mpv_commandv(SaPlayerMpv *s, const gchar *const *argv) {
  if (!s->mpv || !argv || !argv[0]) return;
  /* mpv_command wants a NULL-terminated array of non-const strings. */
  GPtrArray *arr = g_ptr_array_new();
  for (const gchar *const *a = argv; *a; a++) g_ptr_array_add(arr, (gchar *)*a);
  g_ptr_array_add(arr, NULL);
  int rc = mpv_command(s->mpv, (const char **)arr->pdata);
  g_ptr_array_free(arr, TRUE);
  if (rc < 0) log_mpv_error(argv[0], rc);
}

void sa_mpv_observe(SaPlayerMpv *s, const gchar *name) {
  const Prop *p = find_prop(name);
  if (!p) {
    g_message("player: ignoring observe of unsupported property %s", name);
    return;
  }
  if (mpv_observe_property(s->mpv, 0, name, p->format) < 0)
    g_warning("player: observe_property %s failed", name);
}

static const gchar *json_as_string(const SaJson *v) {
  if (!v) return NULL;
  if (v->type == SA_J_STRING) return v->u.str;
  return NULL;
}

void sa_mpv_set_property_value(SaPlayerMpv *s, const gchar *name,
                               const SaJson *value) {
  const Prop *p = find_prop(name);
  if (!p || !value) {
    g_message("player: ignoring set of unsupported property %s", name);
    return;
  }
  switch (p->format) {
    case MPV_FORMAT_DOUBLE: {
      double d = 0;
      if (value->type == SA_J_NUMBER) d = value->u.num;
      else if (value->type == SA_J_STRING) d = g_ascii_strtod(value->u.str, NULL);
      else return;
      if (mpv_set_property(s->mpv, name, MPV_FORMAT_DOUBLE, &d) < 0)
        g_warning("player: set %s failed", name);
      break;
    }
    case MPV_FORMAT_FLAG: {
      int flag;
      if (value->type == SA_J_BOOL) flag = value->u.b ? 1 : 0;
      else if (value->type == SA_J_STRING) {
        const gchar *sv = value->u.str;
        if (g_strcmp0(sv, "true") == 0 || g_strcmp0(sv, "yes") == 0) flag = 1;
        else if (g_strcmp0(sv, "false") == 0 || g_strcmp0(sv, "no") == 0) flag = 0;
        else return;
      } else return;
      if (mpv_set_property(s->mpv, name, MPV_FORMAT_FLAG, &flag) < 0)
        g_warning("player: set %s failed", name);
      break;
    }
    case MPV_FORMAT_STRING: {
      const gchar *sv = json_as_string(value);
      if (value->type == SA_J_NUMBER) {
        gchar *tmp = g_strdup_printf("%.17g", value->u.num);
        sv = tmp;
      } else if (value->type == SA_J_BOOL) {
        sv = value->u.b ? "yes" : "no";
      }
      if (!sv) return;
      if (mpv_set_property_string(s->mpv, name, sv) < 0)
        g_warning("player: set %s failed", name);
      if (value->type == SA_J_NUMBER) g_free((gchar *)sv);
      break;
    }
    default:
      break;
  }
}

static gboolean render_idle(gpointer widget) {
  gtk_gl_area_queue_render(GTK_GL_AREA(widget));
  return G_SOURCE_REMOVE;
}

static void on_render_update(void *ctx) {
  g_idle_add(render_idle, ctx);
}

void sa_mpv_attach_render(SaPlayerMpv *s, mpv_render_context *ctx,
                          gpointer queue_draw_widget) {
  s->render_ctx = ctx;
  s->draw_widget = queue_draw_widget;
  mpv_render_context_set_update_callback(ctx, on_render_update,
                                         queue_draw_widget);
}
