/*
 * player_mpv.h — libmpv handle for the player host.
 *
 * Owns one mpv instance configured for embedded rendering (vo=libmpv +
 * render API) with the launcher-seeded portable_config as config-dir, so
 * mpv.conf / input.conf / scripts (thumbfast) / ~~/shaders all resolve
 * there. Property observation and setting are typed against the table the
 * web UI uses (float/bool/string groups); commands are passed through raw.
 *
 * The mpv event loop runs on a dedicated thread; property changes and
 * end-of-file are marshalled to the main thread through GLib idle hooks.
 */
#ifndef SA_PLAYER_MPV_H
#define SA_PLAYER_MPV_H

#include <glib.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include "json.h"

typedef struct SaPlayerMpv SaPlayerMpv;

/* Callbacks run on the GLib main thread. `value` is a new JSON value owned
 * by the callback (free with sa_json_free). */
typedef void (*SaMpvPropertyFn)(void *ud, const gchar *name, SaJson *value);
typedef void (*SaMpvEndfileFn)(void *ud, const gchar *reason);

/* Create and initialize mpv. config_dir may be NULL (mpv defaults).
 * Returns NULL and fills err on failure. */
SaPlayerMpv *sa_mpv_new(const gchar *config_dir, GError **err);
/* Start the event thread. Call before entering the main loop. */
void sa_mpv_start(SaPlayerMpv *mpv);
void sa_mpv_free(SaPlayerMpv *mpv);

void sa_mpv_set_property_cb(SaPlayerMpv *mpv, SaMpvPropertyFn fn, void *ud);
void sa_mpv_set_endfile_cb(SaPlayerMpv *mpv, SaMpvEndfileFn fn, void *ud);

/* Install the render context produced by the GLArea once its GL context
 * exists; wires mpv's update callback to request redraws. */
void sa_mpv_attach_render(SaPlayerMpv *mpv, mpv_render_context *ctx,
                          gpointer queue_draw_widget);

/* Executes an mpv command (argv[0] = command name, NULL-terminated). */
void sa_mpv_commandv(SaPlayerMpv *mpv, const gchar *const *argv);
/* Typed set-property from a JSON value; no-op when the value does not fit
 * the property's group. */
void sa_mpv_set_property_value(SaPlayerMpv *mpv, const gchar *name,
                               const SaJson *value);
/* Typed observe. Re-observing is harmless. */
void sa_mpv_observe(SaPlayerMpv *mpv, const gchar *name);

mpv_handle *sa_mpv_handle(SaPlayerMpv *mpv);

#endif /* SA_PLAYER_MPV_H */
