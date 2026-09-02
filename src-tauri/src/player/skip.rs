use serde_json::{json, Value};
use tauri::{AppHandle, Emitter};

/// Skip Opening (IntroDB > chapters > silence/blackframe) — Kai notify_skip.lua port.
/// Mock: detects intro in first 90s and outro last 90s, emits toast.
pub fn detect_intro(time_pos: f64, duration: f64) -> Option<(f64, f64)> {
    if time_pos < 90.0 && duration > 1200.0 {
        // Mock intro 0-90s for episodes >20min
        Some((0.0, 90.0))
    } else if duration - time_pos < 90.0 && duration > 1200.0 {
        Some((duration - 90.0, duration))
    } else {
        None
    }
}

pub fn emit_skip_toast(app: &AppHandle, start: f64, end: f64, label: &str) -> anyhow::Result<()> {
    let _ = app.emit(
        "player:skip-toast",
        json!({ "start": start, "end": end, "label": label, "action": "skip" }),
    );
    // legacy
    let _ = app.emit("skip-toast", json!({ "start": start, "end": end }));
    Ok(())
}

pub fn maybe_emit_skip(app: &AppHandle, time_pos: f64, duration: f64) -> Option<Value> {
    if let Some((s, e)) = detect_intro(time_pos, duration) {
        let label = if s == 0.0 { "Skip Intro" } else { "Skip Outro" };
        let _ = emit_skip_toast(app, s, e, label);
        Some(json!({ "start": s, "end": e, "label": label }))
    } else {
        None
    }
}
