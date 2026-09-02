fn main() {
    // Tauri codegen always
    tauri_build::build();

    // ── MPV vendoring (Windows-only) — mirrors stremio-shell-ng/build.rs ─────
    // shell-ng extracts libmpv-2_x64.zip / libmpv-2_arm64.zip at build time and
    // pushes /LIBPATH:.\mpv-x64 (so rustc links mpv.lib). Accru reuses the same
    // layout but scopes it to Windows MSVC only:
    //   • Windows MSVC: extract libmpv-2.dll + mpv.lib, add LIBPATH, ship DLL
    //     via bundle.resources (see tauri.conf.json).
    //   • Linux/macOS: rely on system libmpv via libmpv2-sys pkg-config
    //     (`cargo:rustc-link-lib=mpv` → libmpv.so/dylib from flake.nix pkgs.mpv
    //     or libmpv-dev on CI). No zip/.lib step.
    // Place archives at:
    //   src-tauri/libmpv-2_x64.zip  (+ libmpv-2_arm64.zip for aarch64)
    // which expand to src-tauri/mpv-x64/{mpv.lib,mpv.def,mpv.exp} etc.
    // Fetch helper: https://github.com/Stremio/stremio-shell-ng/releases (assets)
    // or build from https://mpv.io/ + libmpv-2.dll.
    let target = std::env::var("TARGET").unwrap_or_default();
    let is_windows_msvc = target.contains("windows") && target.contains("msvc");

    if !is_windows_msvc {
        return;
    }

    // Copy of shell-ng's arch-branch — keep values identical for drop-in compat.
    let (arch, archive_name, libpath_flag) = match target.as_str() {
        "x86_64-pc-windows-msvc" => ("x64", "libmpv-2_x64.zip", "/LIBPATH:.\\mpv-x64"),
        "aarch64-pc-windows-msvc" => ("arm64", "libmpv-2_arm64.zip", "/LIBPATH:.\\mpv-arm64"),
        other => {
            println!("cargo:warning=stremio-accru: unsupported Windows target {other} — skipping vendored mpv");
            return;
        }
    };

    // Only activate when `desktop-player` (hence `zip-extract`) is enabled;
    // otherwise the `zip-extract` crate is not available and we would fail.
    let zip_extract_enabled = std::env::var("CARGO_FEATURE_DESKTOP_PLAYER").is_ok()
        || std::env::var("CARGO_FEATURE_ZIP_EXTRACT").is_ok();

    if !zip_extract_enabled {
        println!("cargo:warning=stremio-accru: desktop-player feature not enabled — skipping vendored mpv extraction for {arch}");
        return;
    }

    println!("cargo:rustc-env=ARCH={arch}");
    println!("cargo:rustc-link-arg={libpath_flag}");
    println!("cargo:rerun-if-changed={archive_name}");
    println!("cargo:rerun-if-changed=mpv-{arch}/mpv.lib");

    let manifest_dir = std::path::PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let archive_path = manifest_dir.join(archive_name);

    if !archive_path.exists() {
        println!(
            "cargo:warning=stremio-accru: {archive_name} not found at {} — build will try system libmpv. \
             To mirror shell-ng exactly, place the zip (from Stremio/stremio-shell-ng release assets) \
             at src-tauri/{archive_name} so it extracts to src-tauri/mpv-{arch}/ .",
            archive_path.display()
        );
        return;
    }

    // Extract near Cargo.toml (same as shell-ng: target_dir = ".")
    let archive_bytes = match std::fs::read(&archive_path) {
        Ok(b) => b,
        Err(e) => {
            println!(
                "cargo:warning=stremio-accru: failed to read {}: {e}",
                archive_path.display()
            );
            return;
        }
    };

    // zip-extract only available with feature; guarded above.
    #[cfg(feature = "zip-extract")]
    {
        let cursor = std::io::Cursor::new(archive_bytes);
        if let Err(e) = zip_extract::extract(cursor, &manifest_dir, true) {
            println!("cargo:warning=stremio-accru: zip extract failed for {archive_name}: {e}");
        } else {
            println!(
                "cargo:warning=stremio-accru: extracted {archive_name} → {}",
                manifest_dir.display()
            );
        }
    }
    #[cfg(not(feature = "zip-extract"))]
    {
        let _ = archive_bytes;
    }
}
