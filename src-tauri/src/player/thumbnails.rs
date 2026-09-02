use serde_json::json;
use tauri::{AppHandle, Emitter};

/// Hover-seek thumbnails (thumbfast.lua equiv). Mock: emits placeholder.
pub fn request_thumbnail(app: &AppHandle, time_pos: f64) -> anyhow::Result<()> {
    // Real: ffmpeg/mpv thumbnail generation, height via thumbfast.conf
    let thumb = json!({
        "time": time_pos,
        "url": format!("thumb://{:.1}", time_pos),
        "height": 160
    });
    let _ = app.emit("player:thumbnail", thumb.clone());
    let _ = app.emit("thumbnail", thumb);
    Ok(())
}

pub fn get_thumb_height() -> u32 {
    160
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn thumb_height_constant() {
        assert_eq!(get_thumb_height(), 160);
    }

    #[test]
    fn thumb_url_format() {
        // mirrors request_thumbnail URL pattern thumb://{pos:.1}
        let pos = 12.345;
        let expected = format!("thumb://{:.1}", pos);
        assert_eq!(expected, "thumb://12.3");
        assert_eq!(format!("thumb://{:.1}", 0.0), "thumb://0.0");
    }
}
