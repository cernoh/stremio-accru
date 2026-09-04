#!/usr/bin/env -S deno run --allow-run --allow-net --allow-read --allow-env --allow-write
// main.ts — Stremio Accru desktop entrypoint (deno desktop, issue #43).
//
// Model: Deno.serve() a local boot/status page (the desktop webview opens
// here), supervise the streaming server (stremio-linux.sh as a child), then
// navigate the adopted window to the web UI once the server is up. Closing
// the window stops a server this app started; a pre-existing server is left
// alone. Window geometry persists to $DATA_DIR/window.json.
//
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const PROG = "stremio-accru";
const SERVER_URL = "http://127.0.0.1:11470/";
const WEBUI_URLS = [
  "https://web.stremio.com/",
  "https://stremio.zarg.me/",
  "https://zaarrg.github.io/stremio-web-shell-fixes/",
];
const DEFAULT_SIZE = { width: 1280, height: 800 };

const HERE = dirname(fileURLToPath(import.meta.url));
// The wrapper passes the store path; HMR stages main.ts under /tmp, so
// probe the checkout layout too (cwd = scripts/desktop, or its sibling).
function findLauncher(): string {
  const cands = [
    Deno.env.get("STREMIO_LAUNCHER") ?? "",
    join(Deno.cwd(), "..", "stremio-linux.sh"),
    join(HERE, "..", "stremio-linux.sh"),
  ];
  for (const c of cands) {
    if (!c) continue;
    try {
      Deno.statSync(c);
      return c;
    } catch {
      // try next
    }
  }
  die(`stremio-linux.sh not found (set STREMIO_LAUNCHER): ${cands.join(", ")}`);
}
const LAUNCHER = findLauncher();
function homeDir(): string {
  return Deno.env.get("HOME") ?? die("HOME is not set");
}
function dataDirDefault(): string {
  const xdg = Deno.env.get("XDG_DATA_HOME");
  return xdg
    ? `${xdg}/stremio-accru`
    : join(homeDir(), ".local/share/stremio-accru");
}
interface DesktopWindow {
  navigate(url: string): void;
  getSize(): [number, number];
  getPosition(): [number, number];
  addEventListener(type: string, cb: () => void): void;
  openDevtools(opts?: { deno?: boolean; renderer?: boolean }): void;
}
const desktop = Deno as unknown as {
  BrowserWindow?: new (opts: Record<string, unknown>) => DesktopWindow;
};

function die(msg: string): never {
  console.error(`${PROG}: error: ${msg}`);
  Deno.exit(1);
}

function usage(): void {
  console.log(`Usage: nix run .#app [-- <options>]

Standalone Stremio Community app for MangoWM (#43): deno desktop window
+ supervised streaming server.

Options:
  --webui-url=URL   use URL first (default: ${WEBUI_URLS[0]})
  --data-dir=DIR    data dir (default: ${dataDirDefault()})
  --no-server       use the running server, fail if it is down
  --devtools        open the DevTools window (CEF backend only)
  --check           verify seed/server and exit (no window nav)
  --help            show this help

MangoWM rule example (match by appid, confirm yours in the client list):
  windowrule=isfloating:1,width:1280,height:800,appid:stremio-accru`);
}

interface Opts {
  dataDir: string;
  webuiExtra: string;
  noServer: boolean;
  devtools: boolean;
  check: boolean;
}

function parseArgs(args: string[]): Opts {
  const o: Opts = {
    dataDir: Deno.env.get("STREMIO_DATA_DIR") ?? dataDirDefault(),
    webuiExtra: "",
    noServer: false,
    devtools: false,
    check: false,
  };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--") continue;
    // Internal desktop-runtime handoff (compiled .so path); not a user flag.
    if (a === "--runtime") {
      i++;
      continue;
    }
    if (a.startsWith("--runtime=") || a.startsWith("--runtime ")) continue;
    if (a.endsWith(".so")) continue;
    else if (a.startsWith("--webui-url=")) {
      o.webuiExtra = a.slice("--webui-url=".length);
    } else if (a === "--no-server") o.noServer = true;
    else if (a === "--devtools") o.devtools = true;
    else if (a === "--check") o.check = true;
    else if (a === "--help") {
      usage();
      Deno.exit(0);
    } else die(`unknown option: ${a} (see --help)`);
  }
  return o;
}

async function probe(url: string, timeoutMs: number): Promise<boolean> {
  try {
    const res = await fetch(url, {
      method: "HEAD",
      redirect: "manual",
      signal: AbortSignal.timeout(timeoutMs),
    });
    await res.body?.cancel().catch(() => {});
    return res.status < 500;
  } catch {
    return false;
  }
}

async function serverUp(): Promise<boolean> {
  return await probe(SERVER_URL, 3000);
}

async function pickWebui(o: Opts): Promise<string> {
  const env = Deno.env.get("STREMIO_WEBUI_URL");
  const urls = [
    ...(env ? [env] : []),
    ...(o.webuiExtra ? [o.webuiExtra] : []),
    ...WEBUI_URLS,
  ];
  for (const u of urls) {
    if (await probe(u, 5000)) return u;
  }
  return urls[0];
}

async function waitForServer(timeoutMs: number): Promise<boolean> {
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    if (await serverUp()) return true;
    await new Promise((r) => setTimeout(r, 500));
  }
  return await serverUp();
}

interface WinGeom {
  width?: number;
  height?: number;
  x?: number;
  y?: number;
}

async function loadGeom(dataDir: string): Promise<WinGeom> {
  try {
    return JSON.parse(await Deno.readTextFile(join(dataDir, "window.json")));
  } catch {
    return {};
  }
}

function bootPage(): string {
  return `<!doctype html>
<html><head><meta charset="utf-8"><title>Stremio Accru</title>
<style>body{background:#0d1117;color:#e6edf3;font:16px system-ui;display:grid;place-items:center;height:100vh;margin:0}.box{text-align:center}.spin{font-size:28px;animation:s 1.2s linear infinite;display:inline-block}@keyframes s{to{transform:rotate(360deg)}}</style>
</head><body><div class="box"><div class="spin">◌</div><p id="t">Starting streaming server…</p></div>
<script>
const t = document.getElementById("t");
async function poll() {
  try {
    const r = await fetch("/api/status");
    const s = await r.json();
    if (s.serverUp && s.webui) { t.textContent = "Opening " + s.webui; location.replace(s.webui); return; }
  } catch { /* retry */ }
  setTimeout(poll, 800);
}
poll();
</script></body></html>`;
}

async function runCheck(o: Opts): Promise<void> {
  console.log(`deno: ${Deno.version.deno}`);
  const cmd = new Deno.Command("bash", {
    args: [LAUNCHER, "--check", `--data-dir=${o.dataDir}`],
    stdin: "inherit",
    stdout: "inherit",
    stderr: "inherit",
  });
  const st = await cmd.output();
  if (await serverUp()) console.log("server: up");
  else console.log("server: down (the app will start it)");
  if (!st.success) {
    console.log("check: FAILED");
    Deno.exit(1);
  }
  console.log("check: ok");
}

async function main(): Promise<void> {
  const o = parseArgs(Deno.args);

  // serve first: the desktop webview opens here; plain `deno run` binds :8484.
  const serveOpts = Deno.env.get("DENO_SERVE_ADDRESS") ? {} : { port: 8484 };
  const state = { serverUp: false, webui: WEBUI_URLS[0] };
  const server = Deno.serve(serveOpts, (req) => {
    const url = new URL(req.url);
    if (url.pathname === "/api/status") return Response.json(state);
    return new Response(bootPage(), {
      headers: { "content-type": "text/html" },
    });
  });

  if (o.check) {
    await runCheck(o);
    Deno.exit(0);
  }

  // adopt the startup window (desktop runtime only); restore saved geometry.
  const saved = await loadGeom(o.dataDir);
  const win = desktop.BrowserWindow
    ? new desktop.BrowserWindow({
      title: "Stremio Accru",
      width: saved.width ?? DEFAULT_SIZE.width,
      height: saved.height ?? DEFAULT_SIZE.height,
      ...(saved.x !== undefined ? { x: saved.x } : {}),
      ...(saved.y !== undefined ? { y: saved.y } : {}),
    })
    : undefined;
  const saveGeom = () => {
    if (!win) return;
    const [width, height] = win.getSize();
    const [x, y] = win.getPosition();
    Deno.writeTextFile(
      join(o.dataDir, "window.json"),
      JSON.stringify({ width, height, x, y }),
    )
      .catch(() => {});
  };
  win?.addEventListener("resize", saveGeom);
  win?.addEventListener("move", saveGeom);

  let serverChild: Deno.ChildProcess | undefined;
  const stopServer = () => {
    try {
      serverChild?.kill("SIGTERM");
    } catch {
      // already gone
    }
  };
  const quit = () => {
    stopServer();
    Deno.exit(0);
  };
  win?.addEventListener("close", quit);
  Deno.addSignalListener("SIGINT", quit);
  Deno.addSignalListener("SIGTERM", quit);

  if (!(await serverUp())) {
    if (o.noServer) die(`server down at ${SERVER_URL} (--no-server)`);
    console.log(`${PROG}: starting streaming server...`);
    serverChild = new Deno.Command("bash", {
      args: [LAUNCHER, "--no-browser", `--data-dir=${o.dataDir}`],
      stdin: "null",
      stdout: "inherit",
      stderr: "inherit",
    }).spawn();
    if (!(await waitForServer(45000))) {
      stopServer();
      die("streaming server did not come up");
    }
  }

  state.webui = await pickWebui(o);
  state.serverUp = true;
  console.log(`${PROG}: opening ${state.webui}`);
  if (win) {
    win.navigate(state.webui);
    if (o.devtools) win.openDevtools();
  } else {console.log(
      `${PROG}: no desktop window (plain run); open ${state.webui}`,
    );}
  await server.finished;
  stopServer();
}

await main();
