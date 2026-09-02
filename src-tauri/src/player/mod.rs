pub mod backend;
pub mod desktop;
pub mod hdr;
pub mod mobile;
pub mod skip;
pub mod state;
pub mod svp;
pub mod thumbnails;
pub mod tracks;

use std::sync::Arc;

use parking_lot::Mutex;
use serde::Deserialize;
use serde_json::Value;
use tauri::{AppHandle, State};

use self::{
    backend::{Anime4KPreset, AudioPreset, PlayerBackend, VisualProfile},
    desktop::DesktopPlayer,
    mobile::MobilePlayer,
    state::PlayerState,
};

#[derive(Debug, Deserialize)]
pub struct LoadOpts {
    pub url: String,
}

pub struct PlayerBackendState {
    backend: Box<dyn PlayerBackend>,
    #[allow(dead_code)]
    state: Arc<PlayerState>,
}

impl PlayerBackendState {
    pub fn new(app: AppHandle, player_state: Arc<PlayerState>) -> Self {
        let backend: Box<dyn PlayerBackend> =
            if cfg!(target_os = "android") || cfg!(target_os = "ios") {
                Box::new(MobilePlayer::new(app, player_state.clone()))
            } else {
                Box::new(DesktopPlayer::new(app, player_state.clone()))
            };
        Self {
            backend,
            state: player_state,
        }
    }
}

pub fn init_state(_app: &AppHandle) -> Arc<PlayerState> {
    Arc::new(PlayerState::new())
}

// ─── Tauri commands ───────────────────────────────────────────────────────

#[tauri::command]
pub async fn load(
    _app: AppHandle,
    backend: State<'_, Mutex<PlayerBackendState>>,
    url: String,
    opts: Option<LoadOpts>,
) -> Result<(), String> {
    let load_opts = opts.unwrap_or(LoadOpts { url: url.clone() });
    let guard = backend.lock();
    guard
        .backend
        .load(&url, load_opts)
        .map_err(|e| e.to_string())?;
    tracing::info!("player.load ok url={url}");
    Ok(())
}

#[tauri::command]
pub async fn set_property(
    backend: State<'_, Mutex<PlayerBackendState>>,
    key: String,
    value: Value,
) -> Result<(), String> {
    let guard = backend.lock();
    guard
        .backend
        .set_property(&key, value)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn observe(
    backend: State<'_, Mutex<PlayerBackendState>>,
    key: String,
) -> Result<(), String> {
    let guard = backend.lock();
    guard.backend.observe(&key).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn command(
    backend: State<'_, Mutex<PlayerBackendState>>,
    cmd: String,
    args: Vec<String>,
) -> Result<(), String> {
    let guard = backend.lock();
    let refs: Vec<&str> = args.iter().map(String::as_str).collect();
    guard
        .backend
        .command(&cmd, &refs)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn set_shader_preset(
    backend: State<'_, Mutex<PlayerBackendState>>,
    preset: String,
) -> Result<(), String> {
    let p = match preset.as_str() {
        "Optimized" | "optimized" => Anime4KPreset::Optimized,
        "Fast" | "fast" => Anime4KPreset::Fast,
        "HQ" | "hq" => Anime4KPreset::HQ,
        _ => Anime4KPreset::Off,
    };
    let guard = backend.lock();
    guard
        .backend
        .set_shader_preset(p)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn set_visual_profile(
    backend: State<'_, Mutex<PlayerBackendState>>,
    profile: String,
) -> Result<(), String> {
    let p = match profile.as_str() {
        "Vivid" | "vivid" => VisualProfile::Vivid,
        "Original" | "original" => VisualProfile::Original,
        _ => VisualProfile::Kai,
    };
    let guard = backend.lock();
    guard
        .backend
        .set_visual_profile(p)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn set_audio_preset(
    backend: State<'_, Mutex<PlayerBackendState>>,
    preset: String,
) -> Result<(), String> {
    let p = match preset.as_str() {
        "Night" | "night" => AudioPreset::Night,
        "Voice" | "voice" => AudioPreset::Voice,
        _ => AudioPreset::Off,
    };
    let guard = backend.lock();
    guard.backend.set_audio_preset(p).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn get_current_url(state: State<'_, Arc<PlayerState>>) -> Result<Option<String>, String> {
    Ok(state.current_url.lock().clone())
}

// ─── New preset/automation commands ─────────────────────────────────────

#[tauri::command]
pub async fn set_hdr(
    app: AppHandle,
    state: State<'_, Arc<PlayerState>>,
    enabled: bool,
) -> Result<(), String> {
    if enabled {
        hdr::apply_hdr_passthrough(&state, &app).map_err(|e| e.to_string())?;
    } else {
        hdr::apply_tonemapping(&state, &app).map_err(|e| e.to_string())?;
    }
    Ok(())
}

#[tauri::command]
pub async fn set_svp(
    app: AppHandle,
    state: State<'_, Arc<PlayerState>>,
    enabled: bool,
) -> Result<(), String> {
    svp::set_svp(&state, &app, enabled).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn request_skip(
    app: AppHandle,
    time_pos: f64,
    duration: f64,
) -> Result<Option<Value>, String> {
    Ok(skip::maybe_emit_skip(&app, time_pos, duration))
}

#[tauri::command]
pub async fn select_tracks(
    tracks: Vec<Value>,
    audio_lang: Option<String>,
    subs_lang: Option<String>,
) -> Result<Option<Value>, String> {
    let prefs = tracks::TrackPrefs {
        audio_lang: audio_lang.unwrap_or_else(|| "en".into()),
        subs_lang: subs_lang.unwrap_or_else(|| "en".into()),
        ..Default::default()
    };
    Ok(tracks::select_best(&tracks, &prefs, "audio"))
}

#[tauri::command]
pub async fn request_thumbnail(
    app: AppHandle,
    time_pos: f64,
) -> Result<(), String> {
    thumbnails::request_thumbnail(&app, time_pos).map_err(|e| e.to_string())
}
