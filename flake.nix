{
  description = "stremio-accru Linux app (nix run .#app; shell #37, launcher #38, deno app #43)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forEachSystem = nixpkgs.lib.genAttrs systems;
      pkgsFor = system: import nixpkgs { inherit system; };
      # Launcher runtime (#38): server.js + media + fetch. Shared by the
      # dev shell and the nix run app so both stay on the same toolset.
      launcherDeps = pkgs: with pkgs; [
        nodejs
        mpv
        ffmpeg
        curl
        jq
        xdg-utils
        p7zip
      ];
      launcherApp = pkgs: {
        type = "app";
        program = nixpkgs.lib.getExe (pkgs.writeShellApplication {
          name = "stremio-accru-linux";
          runtimeInputs = launcherDeps pkgs;
          text = ''exec "${./scripts/stremio-linux.sh}" "$@"'';
        });
      };
      # Native libs for the downloaded laufey backends (webview webkit2gtk
      # 4.1 ABI + CEF NSS/ALSA/CUPS set): empirically closed, ldd-verified.
      webkitLibs = pkgs: with pkgs; [
        webkitgtk_4_1
        gtk3
        glib.out
        pango
        cairo
        gdk-pixbuf
        atk
        at-spi2-atk
        at-spi2-core
        libsoup_3
        glib-networking
        libsecret
        harfbuzz
        freetype
        fontconfig
        libxkbcommon
        wayland
        libx11
        libxcomposite
        libxdamage
        libxext
        libxfixes
        libxrandr
        libxrender
        libXtst
        mesa
        dbus
        libdrm
        libxshmfence
        libgbm
        libxcb
        libXau
        libXdmcp
        expat
        libffi
        pcre2
        zlib
        libpng
        libjpeg_turbo
        libtiff
        libwebp
        libxml2
        libxslt
        sqlite
        libpsl
        libtasn1
        brotli
        libidn2
        libunistring
        krb5
        keyutils
        icu
        nss
        nspr
        cups.lib
        alsa-lib
        libXi
        udev
        gcc.cc.lib
      ];
      # Standalone app (#43): official deno desktop (webview backend) +
      # launcher toolset for the supervised server.
      appDeps = pkgs: launcherDeps pkgs ++ (with pkgs; [ deno webkitgtk_6_0 ]);
      denoPkg = pkgs: pkgs.writeShellApplication {
        name = "stremio-accru";
        runtimeInputs = (appDeps pkgs) ++ (webkitLibs pkgs) ++ (with pkgs; [ patchelf file ]);
        text = ''
          export LD_LIBRARY_PATH=${nixpkgs.lib.makeLibraryPath (webkitLibs pkgs)}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
          export STREMIO_LAUNCHER="${./scripts}/stremio-linux.sh"
          SRC="${./scripts}"
          BUNDLE="''${XDG_CACHE_HOME:-$HOME/.cache}/stremio-accru/bundle"
          OUT="$BUNDLE/stremio-accru"
          BIN="$OUT/stremio-accru"
          if [ ! -x "$BIN" ] || [ "$(cat "$BUNDLE/.src" 2>/dev/null)" != "$SRC" ]; then
            rm -rf "$BUNDLE"
            mkdir -p "$BUNDLE"
            ( cd "$SRC/desktop" && exec deno desktop \
              --allow-run --allow-net --allow-read --allow-env --allow-write \
              --output "$OUT" ./main.ts )
            INTERP=$(cat ${pkgs.stdenv.cc}/nix-support/dynamic-linker)
            LIBS=${nixpkgs.lib.makeLibraryPath (webkitLibs pkgs)}
            find "$OUT" -type f | while read -r f; do
              if file -b "$f" | grep -q ELF; then
                patchelf --set-interpreter "$INTERP" "$f" 2>/dev/null || true
                patchelf --set-rpath "$LIBS" "$f" || true
              fi
            done
            printf '%s' "$SRC" >"$BUNDLE/.src"
          fi
          exec "$BIN" "$@"
        '';
      };
      denoApp = pkgs: {
        type = "app";
        program = nixpkgs.lib.getExe (denoPkg pkgs);
      };
    in
    {
      devShells = forEachSystem (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            packages = (appDeps pkgs) ++ (webkitLibs pkgs) ++ (with pkgs; [
              # Portable core build deps (mpv client, updater verify)
              cmake
              ninja
              pkg-config
              openssl
              nlohmann_json
              # Future GTK host (#36 proposal): WebKitGTK webview
              webkitgtk_6_0
            ]);

            shellHook = ''
              export LD_LIBRARY_PATH=${nixpkgs.lib.makeLibraryPath (webkitLibs pkgs)}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
              command -v node >/dev/null && command -v mpv >/dev/null \
                || echo "stremio-accru shell: warning, node/mpv missing from PATH"
            '';
          };
        });
      apps = forEachSystem (system:
        let
          pkgs = pkgsFor system;
        in
        {
          linux = launcherApp pkgs;
          app = denoApp pkgs;
          default = denoApp pkgs;
        });
      packages = forEachSystem (system:
        let
          pkgs = pkgsFor system;
        in
        {
          stremio-accru = denoPkg pkgs;
          default = denoPkg pkgs;
        });
    };
}
