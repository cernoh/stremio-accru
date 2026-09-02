pub mod runtime;

use std::sync::Arc;

use parking_lot::RwLock;
use serde_json::Value;
use tauri::{AppHandle, Emitter, State};

use self::runtime::CoreRuntime;

pub struct CoreState {
    runtime: CoreRuntime,
}

impl CoreState {
    pub fn new() -> Self {
        Self {
            runtime: CoreRuntime::new(),
        }
    }
}

pub fn init_core(_app: AppHandle) -> Arc<RwLock<CoreState>> {
    Arc::new(RwLock::new(CoreState::new()))
}

#[tauri::command]
pub async fn dispatch_action(
    app: AppHandle,
    state: State<'_, Arc<RwLock<CoreState>>>,
    action: Value,
) -> Result<Value, String> {
    let result = {
        let guard = state.read();
        guard.runtime.dispatch(action.clone())
    };
    // Emit NewState / CoreEvent to frontend (Elm loop)
    let _ = app.emit("core:event", result.clone());
    if result.get("type").and_then(Value::as_str) == Some("NewState") {
        let _ = app.emit("core:new-state", result.clone());
    }
    tracing::info!(target: "core", "dispatch_action ok");
    Ok(result)
}

#[tauri::command]
pub async fn get_state(state: State<'_, Arc<RwLock<CoreState>>>) -> Result<Value, String> {
    let guard = state.read();
    Ok(guard.runtime.get_state())
}
