use std::collections::HashMap;

use parking_lot::Mutex;
use serde_json::Value;
use tauri::{AppHandle, Emitter};

#[derive(Default)]
pub struct PlayerState {
    pub properties: Mutex<HashMap<String, Value>>,
    pub observed: Mutex<Vec<String>>,
    pub current_url: Mutex<Option<String>>,
}

impl PlayerState {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn set(&self, key: &str, val: Value) {
        self.properties.lock().insert(key.to_string(), val);
    }

    pub fn get(&self, key: &str) -> Option<Value> {
        self.properties.lock().get(key).cloned()
    }

    pub fn emit_property(&self, app: &AppHandle, key: &str, val: Value) {
        self.set(key, val.clone());
        let _ = app.emit("player:property-changed", serde_json::json!({ "key": key, "value": val }));
        let _ = app.emit("property-changed", serde_json::json!({ "name": key, "value": val }));
    }

    pub fn emit_time_pos(&self, app: &AppHandle, pos: f64) {
        self.emit_property(app, "time-pos", Value::from(pos));
    }

    pub fn emit_playback_ended(&self, app: &AppHandle, reason: &str) {
        let _ = app.emit("player:playback-ended", serde_json::json!({ "reason": reason }));
        let _ = app.emit("playback-ended", reason.to_string());
    }
}
