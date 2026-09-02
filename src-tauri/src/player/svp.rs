use anyhow::Result;
use serde_json::Value;

use super::state::PlayerState;
use tauri::AppHandle;

/// SVP interpolation (desktop-only, gated). Real: loads svp_*.vpy via
/// VapourSynth, sets vd-queue etc. Mock: toggles flag and emits.
pub fn is_svp_available() -> bool {
    // Real check: VapourSynth + SVP installed, else false
    false
}

pub fn set_svp(state: &PlayerState, app: &AppHandle, enabled: bool) -> Result<()> {
    if enabled && !is_svp_available() {
        tracing::warn!(target: "player", "SVP requested but VapourSynth not available — no-op (desktop-only)");
        state.set("svp-enabled", Value::Bool(false));
        state.emit_property(app, "svp-enabled", Value::Bool(false));
        return Ok(());
    }
    state.set("svp-enabled", Value::Bool(enabled));
    state.set(
        "hwdec",
        Value::String(if enabled {
            "auto-copy".into()
        } else {
            "auto".into()
        }),
    );
    state.set("hr-seek-framedrop", Value::Bool(!enabled));
    state.emit_property(app, "svp-enabled", Value::Bool(enabled));
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn svp_not_available_by_default() {
        // Mock: always false until VapourSynth/SVP installed
        assert!(!is_svp_available());
    }
}
