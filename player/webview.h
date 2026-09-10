/*
 * webview.h — WebKitGTK webview overlay.
 *
 * The webview sits above the mpv video underlay with a fully transparent
 * background; HTML5 media is disabled so every playback goes through mpv.
 * A document-start user script installs the shell transport shim
 * (window.qt / window.ipc / chrome.webview aliases) exactly like the
 * official Linux shell preload, and native <-> page messages flow over the
 * WebKit script-message channel ("ipc").
 */
#ifndef SA_WEBVIEW_H
#define SA_WEBVIEW_H

#include <gtk/gtk.h>

/* Creates the configured WebKitWebView widget. The caller wraps it in a
 * GtkGraphicsOffload overlay child. */
GtkWidget *sa_webview_new(gboolean devtools);
void sa_webview_load_uri(GtkWidget *wv, const gchar *uri);

/* Delivers a native message to the page (calls __postMessage). The message
 * is the raw JSON transport payload. Safe to call before the page is ready;
 * delivery is dropped silently then (the UI re-requests state on boot). */
void sa_webview_send(GtkWidget *wv, const gchar *message);

/* Registers the handler for page -> native messages (raw string payload). */
void sa_webview_set_ipc_handler(GtkWidget *wv,
                                void (*handler)(void *ud, const gchar *msg),
                                void *ud);

#endif /* SA_WEBVIEW_H */
