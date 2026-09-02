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

pub struct DesktopPlayer {
    state: Arc<PlayerState>,
    app: AppHandle,
    // When feature desktop-player is enabled, this would hold `libmpv2::Mpv`
    // mpv: Mutex<Option<libmpv2::Mpv>>,
    shader_preset: Mutex<Anime4KPreset>,
    visual_profile: Mutex<VisualProfile>,
    audio_preset: Mutex<AudioPreset>,
}

impl DesktopPlayer {
    pub fn new(app: AppHandle, state: Arc<PlayerState>) -> Self {
        Self {
            state,
            app,
            shader_preset: Mutex::new(Anime4KPreset::Off),
            visual_profile: Mutex::new(VisualProfile::Kai),
            audio_preset: Mutex::new(AudioPreset::Off),
        }
    }

    fn ensure_mpv(&self) -> Result<()> {
        // In real desktop build with `desktop-player` feature:
        // - create_render_context with OpenGL/Vulkan bound to Tauri webview
        // - load `mpv.conf` baseline via `load_config`
        // - emit `property-changed` for `vo`, `gpu-api` etc.
        // Mock: just ensure state is initialized.
        Ok(())
    }
}

#[async_trait::async_trait]
impl PlayerBackend for DesktopPlayer {
    fn load(&self, url: &str, _opts: LoadOpts) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::load url={url}");
        *self.state.current_url.lock() = Some(url.to_string());
        self.state.set("path", Value::String(url.to_string()));
        self.state.set("pause", Value::Bool(false));
        // Simulate async playback start + time-pos ticking
        let app = self.app.clone();
        let state = self.state.clone();
        tauri::async_runtime::spawn(async move {
            tokio::time::sleep(std::time::Duration::from_millis(50)).await;
            state.emit_property(&app, "idle-active", Value::Bool(false));
            state.emit_property(&app, "core-idle", Value::Bool(false));
            // tick time-pos every 500ms for 3 ticks (demo)
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
        tracing::info!(target: "player", "DesktopPlayer::set_property {key}={val}");
        // Route known presets through dedicated methods
        match key {
            "glsl-shaders" => {
                // would call `mpv.set_property("glsl-shaders", ...)`
                self.state.set(key, val.clone());
                self.state.emit_property(&self.app, key, val);
                return Ok(());
            }
            "target-peak" | "tone-mapping" | "hdr-peak-percentile" => {
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
        tracing::info!(target: "player", "DesktopPlayer::observe {key}");
        self.state.observed.lock().push(key.to_string());
        // Immediately emit current value if known
        if let Some(v) = self.state.get(key) {
            self.state.emit_property(&self.app, key, v);
        }
        Ok(())
    }

    fn command(&self, cmd: &str, args: &[&str]) -> Result<()> {
        self.ensure_mpv()?;
        tracing::info!(target: "player", "DesktopPlayer::command {cmd} {args:?}");
        match cmd {
            "seek" => {
                if let Some(pos) = args.first().and_then(|s| s.parse::<f64>().ok()) {
                    self.state.emit_time_pos(&self.app, pos);
                }
            }
            "stop" => {
                self.state.emit_playback_ended(&self.app, "stop");
            }
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
        // Real: build `glsl-shaders` semicolon list from `resources/shaders/Anime4K_*`
        let list = match preset {
            Anime4KPreset::Optimized => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Restore_CNN_M.glsl",
            Anime4KPreset::Fast => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Upscale_CNN_x2_S.glsl",
            Anime4KPreset::HQ => "~~/shaders/Anime4K_Clamp_Highlights.glsl:~~/shaders/Anime4K_Restore_CNN_M.glsl:~~/shaders/Anime4K_Upscale_CNN_x2_M.glsl",
            Anime4KPreset::Off => "",
        };
        self.state
            .set("glsl-shaders", Value::String(list.to_string()));
        self.state
            .emit_property(&self.app, "glsl-shaders", Value::String(list.to_string()));
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
        self.state.set("contrast", Value::from(contrast));
        self.state.set("brightness", Value::from(brightness));
        self.state.set("saturation", Value::from(saturation));
        self.state.set("gamma", Value::from(gamma));
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
            AudioPreset::Night => {
                "lavfi=[highpass=f=120,lowpass=f=10000,equalizer=f=2000:width_type=o:width=2:g=6]"
            }
            AudioPreset::Voice => "lavfi=[highpass=f=80,equalizer=f=2000:g=8,equalizer=f=4000:g=6]",
        };
        self.state.set("af", Value::String(af.to_string()));
        self.state
            .emit_property(&self.app, "af", Value::String(af.to_string()));
        Ok(())
    }
}
