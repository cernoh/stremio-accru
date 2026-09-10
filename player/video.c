/*
 * video.c — see video.h.
 *
 * Wiring mirrors the embedded player behavior of the official Linux shell:
 * a GtkGLArea that owns one mpv render context (OpenGL, plus the Wayland
 * display parameter when running on Wayland so hwdec interop works), with
 * mpv's update callback driving redraws. Rendering happens on the "render"
 * signal with the GL context current.
 */
#include "video.h"

#include <dlfcn.h>
#include <epoxy/gl.h>

typedef struct {
  SaPlayerMpv *mpv;
  mpv_render_context *rc;
  gboolean rc_error;
} VideoData;

static void video_data_free(gpointer p) {
  VideoData *d = p;
  if (d->rc) mpv_render_context_free(d->rc);
  g_free(d);
}

static void *gl_get_proc_address(void *fn_ctx, const char *name) {
  (void)fn_ctx;
  /* GTK's GLArea context here is EGL-backed (GLVND dispatch, possibly a
   * GLES context), and GLVND loads its libraries locally — plain
   * RTLD_DEFAULT lookup misses the GL entry points. dlopen the client
   * libraries explicitly (their dirs are on this binary's RUNPATH via
   * buildInputs) and resolve from GLES first (full core exports), then
   * desktop GL, then EGL (extension procs), then anything already global.
   * Load-once, keep the handles. */
  static void *h_gles = NULL;
  static void *h_gl = NULL;
  static void *h_egl = NULL;
  if (!h_gl) {
    h_gles = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);
    h_gl = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    h_egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!h_gles && !h_gl && !h_egl)
      g_warning("video: no GL client library loadable (%s)", dlerror());
  }
  void *p = h_gles ? dlsym(h_gles, name) : NULL;
  if (!p && h_gl) p = dlsym(h_gl, name);
  if (!p && h_egl) p = dlsym(h_egl, name);
  if (!p) p = dlsym(RTLD_DEFAULT, name);
  return p;
}

static gboolean video_render(GtkGLArea *area, GdkGLContext *context,
                             gpointer user_data) {
  VideoData *d = user_data;
  (void)context;
  if (!d->rc) return TRUE;

  int fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
  double scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
  int width = gtk_widget_get_width(GTK_WIDGET(area)) * scale;
  int height = gtk_widget_get_height(GTK_WIDGET(area)) * scale;
  if (width <= 0 || height <= 0) return TRUE;

  mpv_opengl_fbo mfbo = {.fbo = fbo, .w = width, .h = height};
  int flip_y = 1;
  mpv_render_param params[] = {
      {MPV_RENDER_PARAM_OPENGL_FBO, &mfbo},
      {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
      {MPV_RENDER_PARAM_INVALID, NULL},
  };
  int rc = mpv_render_context_render(d->rc, params);
  if (rc < 0 && !d->rc_error) {
    g_warning("video: mpv render failed: %s", mpv_error_string(rc));
    d->rc_error = TRUE;
  }
  return TRUE;
}

static void video_realize(GtkGLArea *area, gpointer user_data) {
  VideoData *d = user_data;
  gtk_gl_area_make_current(area);
  if (gtk_gl_area_get_error(area)) {
    g_warning("video: GLArea GL context error: %s",
              gtk_gl_area_get_error(area)->message);
    return;
  }
  GdkGLContext *glctx = gtk_gl_area_get_context(area);
  if (!glctx) {
    g_warning("video: no GL context at realize");
    return;
  }

  mpv_render_param params[4];
  guint n = 0;
  params[n++] = (mpv_render_param){
      .type = MPV_RENDER_PARAM_API_TYPE,
      .data = (void *)(uintptr_t)MPV_RENDER_API_TYPE_OPENGL,
  };
  mpv_opengl_init_params gl_init = {
      .get_proc_address = gl_get_proc_address,
  };
  params[n++] = (mpv_render_param){
      .type = MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,
      .data = &gl_init,
  };
  params[n++] = (mpv_render_param){.type = MPV_RENDER_PARAM_INVALID, .data = NULL};

  mpv_render_context *rc = NULL;
  int rc_err = mpv_render_context_create(&rc, sa_mpv_handle(d->mpv), params);
  if (rc_err < 0) {
    g_warning("video: mpv_render_context_create failed: %s",
              mpv_error_string(rc_err));
    return;
  }
  d->rc = rc;
  d->rc_error = FALSE;
  sa_mpv_attach_render(d->mpv, rc, area);
  g_message("video: mpv render context ready");
}

static void video_unrealize(GtkGLArea *area, gpointer user_data) {
  VideoData *d = user_data;
  gtk_gl_area_make_current(area);
  if (d->rc) {
    mpv_render_context_free(d->rc);
    d->rc = NULL;
  }
}

GtkWidget *sa_video_new(SaPlayerMpv *mpv) {
  GtkWidget *area = gtk_gl_area_new();
  gtk_gl_area_set_auto_render(GTK_GL_AREA(area), TRUE);
  gtk_widget_set_hexpand(area, TRUE);
  gtk_widget_set_vexpand(area, TRUE);

  VideoData *d = g_new0(VideoData, 1);
  d->mpv = mpv;
  g_object_set_data_full(G_OBJECT(area), "sa-video-data", d, video_data_free);

  g_signal_connect(area, "realize", G_CALLBACK(video_realize), d);
  g_signal_connect(area, "unrealize", G_CALLBACK(video_unrealize), d);
  g_signal_connect(area, "render", G_CALLBACK(video_render), d);
  return area;
}
