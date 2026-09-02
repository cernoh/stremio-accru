use anyhow::Result;
use serde_json::Value;

use super::state::PlayerState;
use tauri::AppHandle;

/// HDR detection and passthrough (Kai profile-manager.lua logic ported).
/// Real: checks video-params primaries/gamma/colormatrix, sets
/// target-colorspace-hint etc. Mock: toggles props.
pub fn is_hdr(params: &Value) -> bool {
    let prim = params
        .get("primaries")
        .and_then(Value::as_str)
        .unwrap_or("");
    let gamma = params.get("gamma").and_then(Value::as_str).unwrap_or("");
    prim == "bt.2020"
        || gamma.contains("smpte2084")
        || gamma.contains("pq")
        || gamma.contains("hlg")
}

pub fn apply_hdr_passthrough(state: &PlayerState, app: &AppHandle) -> Result<()> {
    state.set("target-colorspace-hint", Value::Bool(true));
    state.set("hdr-compute-peak", Value::Bool(true));
    state.set("target-contrast", Value::String("inf".into()));
    state.emit_property(app, "target-colorspace-hint", Value::Bool(true));
    Ok(())
}

pub fn apply_tonemapping(state: &PlayerState, app: &AppHandle) -> Result<()> {
    state.set("tone-mapping", Value::String("bt.2446a".into()));
    state.set("gamut-mapping", Value::String("perceptual".into()));
    state.set("hdr-peak-percentile", Value::from(99.8));
    state.emit_property(app, "tone-mapping", Value::String("bt.2446a".into()));
    Ok(())
}
