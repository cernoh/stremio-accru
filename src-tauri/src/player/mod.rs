use serde::Deserialize;
use serde_json::Value;
use tauri::State;

#[derive(Debug, Deserialize)]
pub struct LoadOpts {
    pub url: String,
}

// Abstraction — desktop uses libmpv2, mobile uses fallback.
// See docs/PLAN.md §2.4. Full trait + impl in PR #1 (issue #1).
#[async_trait::async_trait]
pub trait PlayerBackend: Send + Sync {
    fn load(&self, url: &str, opts: LoadOpts) -> anyhow::Result<()>;
    fn set_property(&self, key: &str, val: Value) -> anyhow::Result<()>;
    fn observe(&self, key: &str) -> anyhow::Result<()>;
    fn command(&self, cmd: &str, args: &[&str]) -> anyhow::Result<()>;
}

#[tauri::command]
pub async fn load(url: String, opts: Option<LoadOpts>) -> Result<(), String> {
    tracing::info!("player.load url={url} opts={opts:?}");
    Err("player not yet implemented — see issue #1".into())
}

#[tauri::command]
pub async fn set_property(key: String, value: Value) -> Result<(), String> {
    tracing::info!("player.set_property {key}={value}");
    Err("player not yet implemented — see issue #1".into())
}

#[tauri::command]
pub async fn observe(key: String) -> Result<(), String> {
    tracing::info!("player.observe {key}");
    Err("player not yet implemented — see issue #1".into())
}

#[tauri::command]
pub async fn command(cmd: String, args: Vec<String>) -> Result<(), String> {
    tracing::info!("player.command {cmd} {args:?}");
    Err("player not yet implemented — see issue #1".into())
}
