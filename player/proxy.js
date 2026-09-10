#!/usr/bin/env node
// proxy.js — local http origin for the Stremio web UI.
//
// The web UI's current build references the streaming server with
// http://127.0.0.1:11470 URLs; from an https page WebKitGTK refuses those
// as mixed content (no loopback exemption like Chromium). Serving the UI
// from a local http origin keeps the page on http, so those calls are
// plain cross-origin fetches (the server runs with NO_CORS=1).
//
// This server reverse-proxies web.stremio.com byte-for-byte: the upstream
// index uses relative asset URLs, so no HTML rewriting is needed — every
// path is forwarded upstream as-is.
//
// Usage: node proxy.js <port> [upstream]
"use strict";

const http = require("http");
const https = require("https");

const port = parseInt(process.argv[2] || "8485", 10);
const upstream = process.argv[3] || "https://web.stremio.com/";
const upstreamUrl = new URL(upstream);

const server = http.createServer((req, res) => {
  const path = req.url === "/" ? "/" : req.url;
  const target = upstreamUrl.origin + path;
  const preq = https.request(
    target,
    {
      method: req.method,
      headers: {
        ...req.headers,
        host: upstreamUrl.host,
        // Keep the connection local; upstream does not need cookies from us.
      },
    },
    (pres) => {
      const headers = { ...pres.headers };
      // hop-by-hop and origin-bound headers that must not leak to the page
      delete headers["strict-transport-security"];
      delete headers["content-security-policy"];
      delete headers["content-security-policy-report-only"];
      delete headers["set-cookie"];
      res.writeHead(pres.statusCode || 502, headers);
      pres.pipe(res);
    }
  );
  preq.on("error", (err) => {
    console.error(`proxy: upstream ${target}: ${err.message}`);
    if (!res.headersSent) {
      res.writeHead(502, { "content-type": "text/plain" });
      res.end("proxy upstream error");
    } else {
      res.destroy();
    }
  });
  req.pipe(preq);
});

server.on("clientError", (_err, socket) => socket.destroy());
server.listen(port, "127.0.0.1", () => {
  console.log(`proxy: listening on http://127.0.0.1:${port} -> ${upstream}`);
});
