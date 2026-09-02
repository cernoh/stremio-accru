use std::path::PathBuf;
use tauri::{AppHandle, Manager};

/// Portable if ACCRU_PORTABLE=1 or a writable `portable_config` dir is sibling to exe/resource.
pub fn is_portable(app: &AppHandle) -> bool {
    if std::env::var("ACCRU_PORTABLE")
        .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
        .unwrap_or(false)
    {
        return true;
    }
    // Check exe dir for portable_config or portable_data marker
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            if dir.join("portable_config").exists() || dir.join("portable_data").exists() {
                return true;
            }
        }
    }
    // Tauri resource dir check
    if let Ok(res) = app.path().resource_dir() {
        if res.join("portable_config").exists() {
            // bundled resource exists — but not proof of portable mode; only sibling-writable counts
        }
    }
    false
}

pub fn data_dir(_app: &AppHandle, portable: bool) -> anyhow::Result<PathBuf> {
    if portable {
        if let Ok(exe) = std::env::current_exe() {
            if let Some(dir) = exe.parent() {
                return Ok(dir.join("portable_data"));
            }
        }
    }
    let base = dirs::data_dir().ok_or_else(|| anyhow::anyhow!("no data dir"))?;
    Ok(base.join("stremio-accru"))
}
