use anyhow::Result;
use parking_lot::Mutex;
use serde_json::Value;
use std::sync::Arc;
use tauri::{AppHandle, Emitter};

use super::{
    backend::{Anime4KPreset, AudioPreset, PlayerBackend, VisualProfile},
    state::PlayerState,
    LoadOpts,
};

// Mirrors stremio-shell-ng's mpv init predominantly; linux-shell's GLArea path
// is the Wayland/render-context alternative (see comments inside `create_mpv`).
// Feature `desktop-player` enables the real libmpv2 wiring; otherwise we keep
// the mock (tests, CI without system mpv, mobile builds).
pub struct DesktopPlayer {
    state: Arc<PlayerState>,
    app: AppHandle,
    #[cfg(feature = "desktop-player")]
    mpv: Mutex<Option<Arc<libmpv2::Mpv>>>,
    shader_preset: Mutex<Anime4KPreset>,
    visual_profile: Mutex<VisualProfile>,
    audio_preset: Mutex<AudioPreset>,
}

impl DesktopPlayer {
    pub fn new(app: AppHandle, state: Arc<PlayerState>) -> Self {
        Self {
            state,
            app,
            #[cfg(feature = "desktop-player")]
            mpv: Mutex::new(None),
            shader_preset: Mutex::new(Anime4KPreset::Off),
            visual_profile: Mutex::new(VisualProfile::Kai),
            audio_preset: Mutex::new(AudioPreset::Off),
        }
    }

    #[cfg(not(feature = "desktop-player"))]
    fn ensure_mpv(&self) -> Result<()> {
        // Mock path — see shell-ng's create_mpv comment but no native handle.
        Ok(())
    }

    #[cfg(feature = "desktop-player")]
    fn ensure_mpv(&self) -> Result<()> {
        if self.mpv.lock().is_some() {
            return Ok(());
        }
        let wid = resolve_wid(&self.app);
        let mpv = create_mpv(wid)?;
        // Mirror shell-ng: disable deprecated events immediately after create
        let _ = mpv.disable_deprecated_events();
        let mpv = Arc::new(mpv);

        // Spawn event thread like shell-ng's create_event_thread
        let event_client = mpv
            .create_client(None)
            .map_err(|e| anyhow::anyhow!("mpv create_client: {e}"))?;
        let app = self.app.clone();
        let state = self.state.clone();
        std::thread::spawn(move || event_loop(event_client, app, state));

        // Windows-only display/HDR thread mirroring shell-ng's create_display_output_thread
        #[cfg(windows)]
        {
            let mpv2 = Arc::clone(&mpv);
            // `HWND` already consumed via `wid`; polling HDR still useful to
            // switch target-colorspace-hint via mpv properties.
            let app2 = self.app.clone();
            let _ = app2;
            // Re-resolve HWND for polling if possible; fallback no-op.
            if let Some(wid) = resolve_wid_raw(&self.app) {
                std::thread::spawn(move || windows_display_thread(mpv2, wid));
            }
        }

        *self.mpv.lock() = Some(mpv);
        Ok(())
    }

    #[cfg(feature = "desktop-player")]
    fn with_mpv<F, T>(&self, f: F) -> Result<T>
    where
        F: FnOnce(&libmpv2::Mpv) -> Result<T>,
    {
        let guard = self.mpv.lock();
        let mpv = guard
            .as_ref()
            .ok_or_else(|| anyhow::anyhow!("mpv not initialized"))?;
        f(mpv)
    }
}

// ── helpers: wid resolution & mpv creation (mirrors shell-ng) ─────────────

#[cfg(feature = "desktop-player")]
fn resolve_wid(app: &AppHandle) -> Option<i64> {
    resolve_wid_raw(app).map(|v| v as i64)
}

#[cfg(feature = "desktop-player")]
fn resolve_wid_raw(app: &AppHandle) -> Option<isize> {
    use raw_window_handle::{HasWindowHandle, RawWindowHandle};
    use tauri::Manager;
    let win = app.get_webview_window("main")?;
    let handle = win.window_handle().ok()?.as_raw();
    match handle {
        RawWindowHandle::Win32(h) => Some(h.hwnd.get() as isize),
        RawWindowHandle::Xlib(h) => Some(h.window as isize),
        RawWindowHandle::Xcb(h) => Some(h.window.get() as isize),
        // Wayland: `wid` is X11/Win32 only — skip, let mpv use its own
        // Wayland surface / render-context path (like linux-shell's GLArea).
        RawWindowHandle::Wayland(_) => None,
        RawWindowHandle::AppKit(_) => None,
        RawWindowHandle::UiKit(_) => None,
        _ => None,
    }
}

#[cfg(feature = "desktop-player")]
fn with_gpu_next_fallback(vo: String) -> String {
    // Exact copy of shell-ng/src/stremio_app/stremio_player/player.rs::with_gpu_next_fallback
    let mut outputs = vo
        .split(',')
        .filter(|o| !o.is_empty())
        .map(String::from)
        .collect::<Vec<String>>();
    let has_next = outputs.iter().any(|o| o == "gpu-next");
    let has_gpu = outputs.iter().any(|o| o == "gpu");
    if outputs.is_empty() {
        outputs.push("gpu-next".to_string());
        outputs.push("gpu".to_string());
    } else if has_next && !has_gpu {
        outputs.push("gpu".to_string());
    } else if has_gpu && !has_next {
        outputs.push("gpu-next".to_string());
    }
    format!("{},", outputs.join(","))
}

#[cfg(feature = "desktop-player")]
fn create_mpv(wid: Option<i64>) -> Result<libmpv2::Mpv> {
    libmpv2::Mpv::with_initializer(|init| {
        // `wid` embedding — Windows/X11 direct render (shell-ng). Absent on
        // Wayland/macOS where linux-shell's GLArea/render_context would be used.
        if let Some(wid) = wid {
            init.set_property("wid", wid)?;
        }
        // Shared baseline — verbatim from shell-ng's create_mpv initializer
        init.set_property("title", "Stremio")?;
        init.set_property("audio-client-name", "Stremio")?;
        init.set_property("terminal", "yes")?;
        #[cfg(debug_assertions)]
        init.set_property("msg-level", "all=no,cplayer=debug")?;
        #[cfg(not(debug_assertions))]
        init.set_property("msg-level", "all=no")?;
        init.set_property("quiet", "yes")?;
        init.set_property("hwdec", "auto")?;
        // same reconnect list as shell-ng: 408,429,500,502,503,504
        init.set_property(
            "stream-lavf-o",
            "reconnect=1,reconnect_streamed=1,reconnect_on_network_error=1,reconnect_on_http_error=%23%408,429,500,502,503,504,reconnect_delay_max=15",
        )?;
        init.set_property("vo", with_gpu_next_fallback("gpu-next,gpu,".to_string()))?;

        // Windows d3d11 opts — only relevant on Windows; no-ops elsewhere.
        #[cfg(windows)]
        {
            let mut opts: Vec<(&str, &str)> = vec![
                ("gpu-context", "d3d11"),
                ("d3d11-output-format", "auto"),
                ("d3d11-output-csp", "auto"),
                ("target-colorspace-hint", "auto"),
                ("target-colorspace-hint-mode", "target"),
                ("tone-mapping", "bt.2390"),
                ("dither-depth", "auto"),
            ];
            // shell-ng gates UMA check via gpu_video_processing::unified_memory_architecture()
            // We approximate: if env ACCRU_UMA=1 or low-mem hint, use fast profile.
            let uma = std::env::var("ACCRU_UMA").map(|v| v == "1").unwrap_or(false);
            if uma {
                opts.push(("profile", "fast"));
            } else {
                opts.extend([
                    ("deband", "yes"),
                    ("scale", "spline36"),
                    ("cscale", "spline36"),
                ]);
            }
            for (k, v) in opts {
                if let Err(e) = init.set_property(k, v) {
                    eprintln!("mpv: cannot set {k}={v}: {e:?}");
                }
            }
        }
        #[cfg(not(windows))]
        {
            // Linux/macOS: lean defaults; portable_config/mpv.conf provides the rest
            let opts = [
                ("gpu-context", "auto"),
                ("hwdec", "auto"),
                ("profile", "high-quality"),
            ];
            for (k, v) in opts {
                let _ = init.set_property(k, v);
            }
        }
        Ok(())
    })
    .map_err(|e| anyhow::anyhow!("mpv with_initializer failed: {e}"))
}

#[cfg(all(feature = "desktop-player", windows))]
fn windows_display_thread(mpv: Arc<libmpv2::Mpv>, _wid: isize) {
    use std::time::Duration;
    // Minimal stub of shell-ng's create_display_output_thread: poll
    // every 500ms and keep target-colorspace-hint in sync if HDR active.
    // Full HDR detection requires winapi DisplayConfig (see shell-ng
    // monitor_hdr_active). For accru we keep the property loop but skip
    // native winapi HDR probe unless the `windows` feature pulls winapi.
    loop {
        // Touch a property so mpv stays configured; real HDR probe would
        // call mpv.set_property("target-colorspace-hint", ...) here.
        let _ = mpv.get_property::<String>("vo");
        std::thread::sleep(Duration::from_millis(500));
    }
}

#[cfg(feature = "desktop-player")]
fn event_loop(client: libmpv2::Mpv, app: AppHandle, state: Arc<PlayerState>) {
    use libmpv2::events::{Event, PropertyData};
    loop {
        let ev = match client.wait_event(0.1) {
            Some(Ok(ev)) => ev,
            Some(Err(e)) => {
                eprintln!("mpv event error: {e:?}");
                continue;
            }
            None => continue,
        };
        match ev {
            Event::PropertyChange { name, change, .. } => {
                let val = match change {
                    PropertyData::Str(s) => {
                        // Try parse as JSON, else string
                        serde_json::from_str::<Value>(s).unwrap_or(Value::String(s.to_string()))
                    }
                    PropertyData::Flag(v) => Value::Bool(v),
                    PropertyData::Int64(v) => Value::from(v),
                    PropertyData::Double(v) => {
                        serde_json::Number::from_f64(v).map(Value::Number).unwrap_or(Value::Null)
                    }
                    _ => continue,
                };
                state.emit_property(&app, name, val);
            }
            Event::EndFile(reason) => {
                let r = format!("{reason:?}").to_lowercase();
                // Map libmpv2 reason to shell-ng's strings: eof/stop/etc
                let mapped = if r.contains("eof") {
                    "eof"
                } else if r.contains("stop") {
                    "stop"
                } else if r.contains("quit") {
                    "quit"
                } else {
                    "other"
                };
                state.emit_playback_ended(&app, mapped);
            }
            Event::Shutdown => break,
            Event::StartFile | Event::FileLoaded | Event::PlaybackRestart => {
                // Could forward video-ready like shell-ng's VideoReadyState;
                // for now emit generic.
                let _ = app.emit("player:event", format!("{ev:?}"));
            }
            _ => {}
        }
    }
}

// ── PlayerBackend impls ─────────────────────────────────────────────────

#[cfg(not(feature = "desktop-player"))]
#[async_trait::async_trait]
impl PlayerBackend for DesktopPlayer {
    fn load(&self, url: &str, _opts: LoadOpts) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::load (mock) url={url}");
        *self.state.current_url.lock() = Some(url.to_string());
        self.state.set("path", Value::String(url.to_string()));
        self.state.set("pause", Value::Bool(false));
        let app = self.app.clone();
        let state = self.state.clone();
        tauri::async_runtime::spawn(async move {
            tokio::time::sleep(std::time::Duration::from_millis(50)).await;
            state.emit_property(&app, "idle-active", Value::Bool(false));
            state.emit_property(&app, "core-idle", Value::Bool(false));
            for i in 1..=3 {
                tokio::time::sleep(std::time::Duration::from_millis(500)).await;
                state.emit_time_pos(&app, i as f64 * 0.5);
            }
        });
        let _ = self.app.emit("player:load", url.to_string());
        Ok(())
    }

    fn set_property(&self, key: &str, val: Value) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::set_property (mock) {key}={val}");
        match key {
            "glsl-shaders" | "target-peak" | "tone-mapping" | "hdr-peak-percentile" => {
                self.state.set(key, val.clone());
                self.state.emit_property(&self.app, key, val);
                return Ok(());
            }
            _ => {}
        }
        self.state.set(key, val.clone());
        self.state.emit_property(&self.app, key, val);
        Ok(())
    }

    fn observe(&self, key: &str) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::observe (mock) {key}");
        self.state.observed.lock().push(key.to_string());
        if let Some(v) = self.state.get(key) {
            self.state.emit_property(&self.app, key, v);
        }
        Ok(())
    }

    fn command(&self, cmd: &str, args: &[&str]) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::command (mock) {cmd} {args:?}");
        match cmd {
            "seek" => {
                if let Some(pos) = args.first().and_then(|s| s.parse::<f64>().ok()) {
                    self.state.emit_time_pos(&self.app, pos);
                }
            }
            "stop" => self.state.emit_playback_ended(&self.app, "stop"),
            _ => {}
        }
        let _ = self.app.emit(
            "player:command",
            serde_json::json!({ "cmd": cmd, "args": args }),
        );
        Ok(())
    }

    fn set_shader_preset(&self, preset: Anime4KPreset) -> Result<()> {
        *self.shader_preset.lock() = preset;
        tracing::info!(target: "player", "DesktopPlayer::set_shader_preset (mock) {preset:?}");
        let list = match preset {
            Anime4KPreset::Optimized => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Restore_CNN_M.glsl",
            Anime4KPreset::Fast => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Upscale_CNN_x2_S.glsl",
            Anime4KPreset::HQ => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Restore_CNN_M.glsl:~~/shaders/Anime4K_Upscale_CNN_x2_M.glsl",
            Anime4KPreset::Off => "",
        };
        self.state.set("glsl-shaders", Value::String(list.to_string()));
        self.state.emit_property(&self.app, "glsl-shaders", Value::String(list.to_string()));
        Ok(())
    }

    fn set_visual_profile(&self, profile: VisualProfile) -> Result<()> {
        *self.visual_profile.lock() = profile;
        tracing::info!(target: "player", "DesktopPlayer::set_visual_profile (mock) {profile:?}");
        let (contrast, brightness, saturation, gamma) = match profile {
            VisualProfile::Kai => (2, -6, 2, 2),
            VisualProfile::Vivid => (5, -4, 15, -2),
            VisualProfile::Original => (0, 0, 0, 0),
        };
        self.state.set("contrast", Value::from(contrast));
        self.state.set("brightness", Value::from(brightness));
        self.state.set("saturation", Value::from(saturation));
        self.state.set("gamma", Value::from(gamma));
        self.state.emit_property(&self.app, "visual-profile", Value::String(format!("{profile:?}")));
        Ok(())
    }

    fn set_audio_preset(&self, preset: AudioPreset) -> Result<()> {
        *self.audio_preset.lock() = preset;
        tracing::info!(target: "player", "DesktopPlayer::set_audio_preset (mock) {preset:?}");
        let af = match preset {
            AudioPreset::Off => "",
            AudioPreset::Night => "lavfi=[highpass=f=120,lowpass=f=10000,equalizer=f=2000:width_type=o:width=2:g=6]",
            AudioPreset::Voice => "lavfi=[highpass=f=80,equalizer=f=2000:g=8,equalizer=f=4000:g=6]",
        };
        self.state.set("af", Value::String(af.to_string()));
        self.state.emit_property(&self.app, "af", Value::String(af.to_string()));
        Ok(())
    }
}

#[cfg(feature = "desktop-player")]
#[async_trait::async_trait]
impl PlayerBackend for DesktopPlayer {
    fn load(&self, url: &str, _opts: LoadOpts) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::load url={url}");
        *self.state.current_url.lock() = Some(url.to_string());
        self.state.set("path", Value::String(url.to_string()));
        self.state.set("pause", Value::Bool(false));
        // Mirror shell-ng's message thread: loadfile via mpv command
        self.with_mpv(|mpv| {
            mpv.command("loadfile", &[url, "replace"])
                .map_err(|e| anyhow::anyhow!("mpv loadfile: {e}"))?;
            Ok(())
        })?;
        let _ = self.app.emit("player:load", url.to_string());
        Ok(())
    }

    fn set_property(&self, key: &str, val: Value) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::set_property {key}={val}");
        let res: Result<()> = self.with_mpv(|mpv| {
            match &val {
                Value::Bool(b) => mpv.set_property(key, *b).map_err(|e| anyhow::anyhow!("{e}"))?,
                Value::Number(n) => {
                    if let Some(f) = n.as_f64() {
                        mpv.set_property(key, f).map_err(|e| anyhow::anyhow!("{e}"))?;
                    } else if let Some(i) = n.as_i64() {
                        mpv.set_property(key, i).map_err(|e| anyhow::anyhow!("{e}"))?;
                    }
                }
                Value::String(s) => mpv.set_property(key, s.as_str()).map_err(|e| anyhow::anyhow!("{e}"))?,
                _ => {
                    // Complex JSON -> serialize as string (e.g. video-params)
                    let s = val.to_string();
                    mpv.set_property(key, s.as_str()).map_err(|e| anyhow::anyhow!("{e}"))?;
                }
            }
            Ok(())
        });
        // Keep state in sync + emit regardless of mpv success (mirrors linux-shell)
        self.state.set(key, val.clone());
        self.state.emit_property(&self.app, key, val);
        res
    }

    fn observe(&self, key: &str) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::observe {key}");
        self.state.observed.lock().push(key.to_string());
        let fmt = match key {
            // mirror linux-shell's BOOL/FLOAT/STRING partitioning heuristically
            "pause" | "idle-active" | "core-idle" | "pause-for-cache" | "eof-reached" => {
                libmpv2::Format::Flag
            }
            "time-pos" | "duration" | "volume" | "speed" | "contrast" | "brightness"
            | "saturation" | "gamma" => libmpv2::Format::Double,
            _ => libmpv2::Format::String,
        };
        self.with_mpv(|mpv| {
            mpv.observe_property(key, fmt, 0)
                .map_err(|e| anyhow::anyhow!("observe {key}: {e}"))?;
            Ok(())
        })?;
        if let Some(v) = self.state.get(key) {
            self.state.emit_property(&self.app, key, v);
        }
        Ok(())
    }

    fn command(&self, cmd: &str, args: &[&str]) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::command {cmd} {args:?}");
        self.with_mpv(|mpv| {
            mpv.command(cmd, args)
                .map_err(|e| anyhow::anyhow!("mpv command {cmd}: {e}"))?;
            Ok(())
        })?;
        // Also update local state for seek/stop like mock
        match cmd {
            "seek" => {
                if let Some(pos) = args.first().and_then(|s| s.parse::<f64>().ok()) {
                    self.state.emit_time_pos(&self.app, pos);
                }
            }
            "stop" => self.state.emit_playback_ended(&self.app, "stop"),
            _ => {}
        }
        let _ = self.app.emit(
            "player:command",
            serde_json::json!({ "cmd": cmd, "args": args }),
        );
        Ok(())
    }

    fn set_shader_preset(&self, preset: Anime4KPreset) -> Result<()> {
        *self.shader_preset.lock() = preset;
        tracing::info!(target: "player", "DesktopPlayer::set_shader_preset {preset:?}");
        let list = match preset {
            Anime4KPreset::Optimized => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Restore_CNN_M.glsl",
            Anime4KPreset::Fast => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Upscale_CNN_x2_S.glsl",
            Anime4KPreset::HQ => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Restore_CNN_M.glsl:~~/shaders/Anime4K_Upscale_CNN_x2_M.glsl",
            Anime4KPreset::Off => "",
        };
        self.set_property("glsl-shaders", Value::String(list.to_string()))?;
        Ok(())
    }

    fn set_visual_profile(&self, profile: VisualProfile) -> Result<()> {
        *self.visual_profile.lock() = profile;
        tracing::info!(target: "player", "DesktopPlayer::set_visual_profile {profile:?}");
        let (contrast, brightness, saturation, gamma) = match profile {
            VisualProfile::Kai => (2, -6, 2, 2),
            VisualProfile::Vivid => (5, -4, 15, -2),
            VisualProfile::Original => (0, 0, 0, 0),
        };
        self.set_property("contrast", Value::from(contrast))?;
        self.set_property("brightness", Value::from(brightness))?;
        self.set_property("saturation", Value::from(saturation))?;
        self.set_property("gamma", Value::from(gamma))?;
        self.state.emit_property(
            &self.app,
            "visual-profile",
            Value::String(format!("{profile:?}")),
        );
        Ok(())
    }

    fn set_audio_preset(&self, preset: AudioPreset) -> Result<()> {
        *self.audio_preset.lock() = preset;
        tracing::info!(target: "player", "DesktopPlayer::set_audio_preset {preset:?}");
        let af = match preset {
            AudioPreset::Off => "",
            AudioPreset::Night => "lavfi=[highpass=f=120,lowpass=f=10000,equalizer=f=2000:width_type=o:width=2:g=6]",
            AudioPreset::Voice => "lavfi=[highpass=f=80,equalizer=f=2000:g=8,equalizer=f=4000:g=6]",
        };
        self.set_property("af", Value::String(af.to_string()))?;
        Ok(())
    }
}
