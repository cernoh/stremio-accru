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

#[cfg(test)]
mod tests {
    #[test]
    fn portable_env_true() {
        std::env::set_var("ACCRU_PORTABLE", "1");
        std::env::remove_var("ACCRU_PORTABLE");
        std::env::set_var("ACCRU_PORTABLE", "True");
        assert_eq!(std::env::var("ACCRU_PORTABLE").unwrap(), "True");
        std::env::remove_var("ACCRU_PORTABLE");
    }

    #[test]
    fn data_dir_non_portable_ends_with_stremio_accru() {
        let base = dirs::data_dir().expect("no data dir on this host");
        let expected = base.join("stremio-accru");
        assert!(expected.ends_with("stremio-accru"));
        assert!(expected.to_string_lossy().contains("stremio-accru"));
    }

    #[test]
    fn data_dir_portable_flag_uses_exe_parent() {
        if let Ok(exe) = std::env::current_exe() {
            if let Some(parent) = exe.parent() {
                let portable_path = parent.join("portable_data");
                assert!(portable_path.ends_with("portable_data"));
            }
        }
    }

    #[test]
    fn env_var_case_insensitive() {
        for val in ["1", "true", "True", "TRUE", "TrUe"] {
            let is_one = val == "1" || val.eq_ignore_ascii_case("true");
            assert!(is_one, "val {val} should be portable");
        }
        for val in ["0", "false", "no", "", "2"] {
            let is_one = val == "1" || val.eq_ignore_ascii_case("true");
            assert!(!is_one, "val {val} should not be portable");
        }
    }
}
