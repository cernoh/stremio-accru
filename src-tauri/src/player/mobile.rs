use anyhow::Result;
use serde_json::Value;
use std::sync::Arc;
use tauri::{AppHandle, Emitter};

use super::{
    backend::{Anime4KPreset, AudioPreset, PlayerBackend, VisualProfile},
    state::PlayerState,
    LoadOpts,
};

pub struct MobilePlayer {
    state: Arc<PlayerState>,
    app: AppHandle,
}

impl MobilePlayer {
    pub fn new(app: AppHandle, state: Arc<PlayerState>) -> Self {
        Self { state, app }
    }
}

#[async_trait::async_trait]
impl PlayerBackend for MobilePlayer {
    fn load(&self, url: &str, _opts: LoadOpts) -> Result<()> {
        tracing::info!(target: "player", "MobilePlayer::load url={url} (fallback: ExoPlayer/AVPlayer if libmpv missing)");
        *self.state.current_url.lock() = Some(url.to_string());
        self.state.set("path", Value::String(url.to_string()));
        self.state.set("pause", Value::Bool(false));
        let app = self.app.clone();
        let state = self.state.clone();
        tauri::async_runtime::spawn(async move {
            tokio::time::sleep(std::time::Duration::from_millis(50)).await;
            state.emit_property(&app, "idle-active", Value::Bool(false));
            for i in 1..=3 {
                tokio::time::sleep(std::time::Duration::from_millis(500)).await;
                state.emit_time_pos(&app, i as f64 * 0.5);
            }
        });
        let _ = self.app.emit("player:load", url.to_string());
        Ok(())
    }

    fn set_property(&self, key: &str, val: Value) -> Result<()> {
        // On mobile, HDR/shader/SVP keys are gated in UI; log and no-op if unsupported
        let gated = [
            "glsl-shaders",
            "target-peak",
            "tone-mapping",
            "hdr-compute-peak",
        ];
        if gated.contains(&key) {
            tracing::warn!(target: "player", "MobilePlayer gated key {key} ignored (desktop-only)");
            return Ok(());
        }
        tracing::info!(target: "player", "MobilePlayer::set_property {key}={val}");
        self.state.set(key, val.clone());
        self.state.emit_property(&self.app, key, val);
        Ok(())
    }

    fn observe(&self, key: &str) -> Result<()> {
        tracing::info!(target: "player", "MobilePlayer::observe {key}");
        self.state.observed.lock().push(key.to_string());
        if let Some(v) = self.state.get(key) {
            self.state.emit_property(&self.app, key, v);
        }
        Ok(())
    }

    fn command(&self, cmd: &str, args: &[&str]) -> Result<()> {
        tracing::info!(target: "player", "MobilePlayer::command {cmd} {args:?}");
        if cmd == "seek" {
            if let Some(pos) = args.first().and_then(|s| s.parse::<f64>().ok()) {
                self.state.emit_time_pos(&self.app, pos);
            }
        }
        let _ = self.app.emit(
            "player:command",
            serde_json::json!({ "cmd": cmd, "args": args }),
        );
        Ok(())
    }

    fn set_shader_preset(&self, preset: Anime4KPreset) -> Result<()> {
        if preset != Anime4KPreset::Off {
            tracing::warn!(target: "player", "MobilePlayer Anime4K {preset:?} gated — desktop only");
        }
        Ok(())
    }

    fn set_visual_profile(&self, profile: VisualProfile) -> Result<()> {
        // Keep visual profiles on mobile (OLED)
        tracing::info!(target: "player", "MobilePlayer::set_visual_profile {profile:?}");
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
        tracing::info!(target: "player", "MobilePlayer::set_audio_preset {preset:?}");
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
