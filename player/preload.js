/*
 * preload.js — document-start shell transport shim.
 *
 * Installed by the player host before any page script runs. It presents
 * the transport objects the Stremio web UI expects from a desktop shell
 * (the official Linux shell exposes the same shape):
 *
 *   window.ipc.postMessage(msg)         -> native (WebKit "ipc" channel)
 *   window.qt.webChannelTransport       -> what the web UI's shell glue
 *                                          binds its channel to
 *   window.chrome.webview               -> WebView2-style alias
 *   window.stremio_server_ipc_key       -> matches SERVER_IPC_KEY the host
 *                                          passes to the streaming server
 *
 * Native -> page messages arrive through __postMessage and are forwarded to
 * qt.webChannelTransport.onmessage (which the page's channel code assigns).
 */
(() => {
  if (window.__stremioAccruPreload) return;
  window.__stremioAccruPreload = true;

  const listeners = [];

  window.__postMessage = (data) => {
    for (const listener of listeners.slice()) listener({ data });
  };

  const postMessage = (data) => {
    try {
      window.webkit.messageHandlers.ipc.postMessage(data);
    } catch (e) {
      console.error("stremio-accru: ipc send failed", e);
    }
  };

  const addListener = (name, listener) => {
    if (name !== "message") throw new Error("unsupported event: " + name);
    listeners.push(listener);
  };
  const removeListener = (name, listener) => {
    if (name !== "message") throw new Error("unsupported event: " + name);
    const i = listeners.indexOf(listener);
    if (i >= 0) listeners.splice(i, 1);
  };

  const ipc = {
    postMessage,
    addEventListener: addListener,
    removeEventListener: removeListener,
  };

  window.ipc = ipc;
  window.qt = { webChannelTransport: { send: postMessage } };

  window.chrome = window.chrome || {};
  window.chrome.webview = {
    postMessage,
    addEventListener: addListener,
    removeEventListener: removeListener,
  };

  window.stremio_server_ipc_key = "LINUX";

  ipc.addEventListener("message", (message) => {
    const transport = window.qt.webChannelTransport;
    if (transport && typeof transport.onmessage === "function") {
      transport.onmessage(message);
    }
  });

  console.log("stremio-accru: shell transport installed");
})();
