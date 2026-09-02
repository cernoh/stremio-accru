pub mod config;
pub mod core;
pub mod player;

use parking_lot::Mutex;
use tauri::Manager;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tracing_subscriber::fmt::init();

    tauri::Builder::default()
        .plugin(tauri_plugin_store::Builder::default().build())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .setup(|app| {
            let handle = app.handle().clone();
            // Player state + backend (desktop vs mobile)
            let player_state = player::init_state(&handle);
            let backend = player::PlayerBackendState::new(handle.clone(), player_state.clone());
            app.manage(player_state);
            app.manage(Mutex::new(backend));
            // Core runtime (mock Elm, real stremio-core in future)
            let core_state = core::init_core(handle.clone());
            app.manage(core_state);
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
            player::set_shader_preset,
            player::set_visual_profile,
            player::set_audio_preset,
            player::get_current_url,
            player::set_hdr,
            player::set_svp,
            player::request_skip,
            player::select_tracks,
            player::request_thumbnail,
            core::dispatch_action,
            core::get_state,
            config::get_portable_mode,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri app");
}
