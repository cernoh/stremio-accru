use tauri::AppHandle;

pub mod portable;

/// Returns true if running in portable mode (portable_config sibling or ACCRU_PORTABLE=1).
#[tauri::command]
pub fn get_portable_mode(app: AppHandle) -> bool {
    portable::is_portable(&app)
}

pub async fn init(app: &AppHandle) -> anyhow::Result<()> {
    let portable = portable::is_portable(app);
    tracing::info!("config.init portable={portable}");
    let data_dir = portable::data_dir(app, portable)?;
    std::fs::create_dir_all(&data_dir)?;
    // First-run extraction of bundled resources handled in issue #10.
    Ok(())
}
