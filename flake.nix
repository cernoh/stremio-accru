{
  description = "Stremio Accru — platform-agnostic Stremio-Kai (Tauri + stremio-core + MPV) dev shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    rust-overlay = {
      url = "github:oxalica/rust-overlay";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, flake-utils, rust-overlay }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        overlays = [ (import rust-overlay) ];
        pkgs = import nixpkgs { inherit system overlays; };

        rustToolchain = pkgs.rust-bin.stable.latest.default.override {
          targets = [ "wasm32-unknown-unknown" ];
        };

        tauriDeps = with pkgs; [
          pkg-config
          gobject-introspection
          openssl
          glib
          gtk3
          webkitgtk_4_1
          libsoup_3
          cairo
          gdk-pixbuf
          librsvg
          pango
          atk
          harfbuzz
          dbus
          zlib
          mpv
          # libmpv headers for libmpv2-sys pkg-config (mirrors stremio-linux-shell mpv-devel)
        ];

        darwinDeps = with pkgs; lib.optionals pkgs.stdenv.hostPlatform.isDarwin [
          darwin.apple_sdk
          libiconv
        ];

        commonRuntimeInputs = with pkgs; [
          rustToolchain
          cargo-tauri
          deno
          nodejs_22
          pkg-config
          openssl
        ] ++ tauriDeps ++ darwinDeps;

        # `nix run` — local dev build (cargo tauri dev) with auto npm install
        devRunner = pkgs.writeShellApplication {
          name = "stremio-accru-dev";
          runtimeInputs = commonRuntimeInputs;
          text = ''
            export PKG_CONFIG_PATH="${pkgs.lib.makeSearchPathOutput "dev" "lib/pkgconfig" tauriDeps}:${pkgs.lib.makeSearchPathOutput "dev" "share/pkgconfig" tauriDeps}:${pkgs.mpv}/lib/pkgconfig:''${PKG_CONFIG_PATH:-}"
            export WEBKIT_DISABLE_DMABUF_RENDERER=1
            if [ ! -d node_modules ]; then
              echo "→ node_modules missing — running npm install..."
              npm install
            fi
            exec cargo tauri dev "$@"
          '';
        };
        # `nix run .#build` — local production bundle (cargo tauri build)
        buildRunner = pkgs.writeShellApplication {
          name = "stremio-accru-build";
          runtimeInputs = commonRuntimeInputs;
          text = ''
            export PKG_CONFIG_PATH="${pkgs.lib.makeSearchPathOutput "dev" "lib/pkgconfig" tauriDeps}:${pkgs.lib.makeSearchPathOutput "dev" "share/pkgconfig" tauriDeps}:${pkgs.mpv}/lib/pkgconfig:''${PKG_CONFIG_PATH:-}"
            export WEBKIT_DISABLE_DMABUF_RENDERER=1
            if [ ! -d node_modules ]; then
              echo "→ node_modules missing — running npm install..."
              npm install
            fi
            exec cargo tauri build "$@"
          '';
        };
        # `nix run .#frontend` — just the vite frontend (deno task dev)
        frontendRunner = pkgs.writeShellApplication {
          name = "stremio-accru-frontend";
          runtimeInputs = with pkgs; [ deno nodejs_22 ];
          text = ''
            if [ ! -d node_modules ]; then
              echo "→ node_modules missing — running npm install..."
              npm install
            fi
            exec deno task dev "$@"
          '';
        };

      in {
        devShells.default = pkgs.mkShell {
          name = "stremio-accru";
          nativeBuildInputs = with pkgs; [
            rustToolchain
            rust-analyzer
            cargo-tauri
            deno
            nodejs_22
            pkg-config
            openssl
            cargo-watch
          ] ++ tauriDeps ++ darwinDeps;

          shellHook = ''
            echo "Stremio Accru dev shell — $(rustc --version), deno $(deno --version | head -n1), node $(node --version), npm $(npm --version)"
            echo "  nix develop              # enter shell"
            echo "  nix run                  # tauri dev (cargo tauri dev)"
            echo "  nix run .#build          # tauri build (cargo tauri build)"
            echo "  nix run .#frontend       # frontend only (deno task dev)"
            echo "  deno task dev            # frontend dev (vite via deno)"
            echo "  deno task build          # frontend build"
            echo "  deno task tauri dev      # tauri desktop"
            echo "  cargo check --manifest-path src-tauri/Cargo.toml"
            export PKG_CONFIG_PATH="${pkgs.openssl.dev}/lib/pkgconfig:${pkgs.webkitgtk_4_1.dev}/lib/pkgconfig:${pkgs.mpv}/lib/pkgconfig:''${PKG_CONFIG_PATH:-}"
          '';

          WEBKIT_DISABLE_DMABUF_RENDERER = "1";
        };

        packages = {
          default = devRunner;
          dev = devRunner;
          build = buildRunner;
          frontend = frontendRunner;
        };

        apps = {
          default = flake-utils.lib.mkApp { drv = devRunner; };
          dev = flake-utils.lib.mkApp { drv = devRunner; };
          build = flake-utils.lib.mkApp { drv = buildRunner; };
          frontend = flake-utils.lib.mkApp { drv = frontendRunner; };
        };

        formatter = pkgs.nixpkgs-fmt;
      });
}
