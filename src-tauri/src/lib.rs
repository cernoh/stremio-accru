pub mod config;
pub mod core;
pub mod player;

use tauri::Manager;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tracing_subscriber::fmt::init();

    tauri::Builder::default()
        .plugin(tauri_plugin_store::Builder::default().build())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .setup(|app| {
            let handle = app.handle().clone();
            tauri::async_runtime::spawn(async move {
                if let Err(e) = config::init(&handle).await {
                    tracing::error!("config init failed: {e:#}");
                }
            });
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            player::load,
            player::set_property,
            player::observe,
            player::command,
            core::dispatch_action,
            core::get_state,
            config::get_portable_mode,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri app");
}
