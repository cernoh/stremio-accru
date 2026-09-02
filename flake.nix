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
          mpv
        ];

        darwinDeps = with pkgs; lib.optionals pkgs.stdenv.hostPlatform.isDarwin [
          darwin.apple_sdk
          libiconv
        ];

      in {
        devShells.default = pkgs.mkShell {
          name = "stremio-accru";
          nativeBuildInputs = with pkgs; [
            rustToolchain
            rust-analyzer
            cargo-tauri
            nodejs_22
            pkg-config
            openssl
            cargo-watch
          ] ++ tauriDeps ++ darwinDeps;

          shellHook = ''
            echo "Stremio Accru dev shell — $(rustc --version), node $(node --version), npm $(npm --version)"
            echo "  nix develop              # enter shell"
            echo "  npm install && npm run tauri dev   # desktop"
            echo "  cargo check --manifest-path src-tauri/Cargo.toml"
            export PKG_CONFIG_PATH="${pkgs.openssl.dev}/lib/pkgconfig:${pkgs.webkitgtk_4_1.dev}/lib/pkgconfig:''${PKG_CONFIG_PATH:-}"
          '';

          WEBKIT_DISABLE_DMABUF_RENDERER = "1";
        };

        formatter = pkgs.nixpkgs-fmt;
      });
}
