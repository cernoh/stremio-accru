use serde_json::Value;

// stremio-core Runtime wired in PR #2 (issue #2).
// Env impl: fetch via reqwest, storage via tauri-plugin-store/portable file, exec via tokio.
#[tauri::command]
pub async fn dispatch_action(action: Value) -> Result<Value, String> {
    tracing::info!("core.dispatch_action {action}");
    Err("stremio-core not yet wired — see issue #2".into())
}

#[tauri::command]
pub async fn get_state() -> Result<Value, String> {
    Err("stremio-core not yet wired — see issue #2".into())
}
