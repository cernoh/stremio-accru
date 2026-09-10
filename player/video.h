/*
 * video.h — mpv video surface (GtkGLArea underlay).
 *
 * The GLArea fills the window below the transparent webview. libmpv renders
 * into the GLArea's framebuffer via the mpv render API (OpenGL). The render
 * context is created when the widget's GL context first exists (realize).
 */
#ifndef SA_VIDEO_H
#define SA_VIDEO_H

#include <gtk/gtk.h>

#include "player_mpv.h"

/* Creates the underlay widget. `mpv` must outlive the widget. */
GtkWidget *sa_video_new(SaPlayerMpv *mpv);

#endif /* SA_VIDEO_H */
