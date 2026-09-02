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

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn detect_intro_at_start() {
        let res = detect_intro(10.0, 1500.0);
        assert_eq!(res, Some((0.0, 90.0)));
    }

    #[test]
    fn detect_outro_near_end() {
        let res = detect_intro(1450.0, 1500.0);
        assert_eq!(res, Some((1410.0, 1500.0)));
    }

    #[test]
    fn no_detect_mid_episode() {
        assert_eq!(detect_intro(500.0, 1500.0), None);
    }

    #[test]
    fn no_detect_short_duration() {
        // duration < 1200 => no intro skip
        assert_eq!(detect_intro(10.0, 600.0), None);
        assert_eq!(detect_intro(590.0, 600.0), None);
    }

    #[test]
    fn boundary_90s() {
        // exactly at 90s should not trigger intro (needs <90)
        assert_eq!(detect_intro(90.0, 1500.0), None);
        // duration - pos == 90 should not trigger outro (needs <90)
        assert_eq!(detect_intro(1410.0, 1500.0), None);
        // 89.9 triggers
        assert_eq!(detect_intro(89.9, 1500.0), Some((0.0, 90.0)));
        assert_eq!(detect_intro(1410.1, 1500.0), Some((1410.0, 1500.0)));
    }

    #[test]
    fn json_skip_payload_shape() {
        let v = json!({ "start": 0.0, "end": 90.0, "label": "Skip Intro" });
        assert_eq!(v["label"], "Skip Intro");
        assert_eq!(v["start"], 0.0);
    }
}
