{
  description = "stremio-accru Linux app (nix run .#app; shell #37, launcher #38, native player #58)";

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
      # Native player (#58): GTK4 + WebKitGTK 6 host with a libmpv video
      # underlay (player/). Headers/libs for building the host and libs for
      # running it (stdenv adds rpath to buildInputs automatically).
      playerBuildInputs = pkgs: with pkgs; [
        gtk4
        webkitgtk_6_0
        libepoxy
        mpv
        libGL
        libglvnd
        mesa
      ];
      playerLibs = pkgs: with pkgs; [
        gtk4
        webkitgtk_6_0
        libepoxy
        mpv
        libGL
        libglvnd
        mesa
        glib-networking
      ];
      playerBin = pkgs: pkgs.stdenv.mkDerivation {
        pname = "stremio-accru-player";
        version = "0.1.0";
        src = ./player;
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildInputs = playerBuildInputs pkgs;
        buildPhase = ''
          bash gen_preload.sh
          cc -std=c11 -O2 -Wall -o stremio-accru \
            main.c ipc.c json.c player_mpv.c video.c webview.c \
            $(pkg-config --cflags --libs gtk4 webkitgtk-6.0 mpv epoxy)
        '';
        installPhase = ''
          mkdir -p $out/bin $out/share/stremio-accru
          cp stremio-accru $out/bin/
          cp proxy.js $out/share/stremio-accru/
        '';
      };
      # Wrapper: MangoWM runtime env (WEBKIT_DISABLE_DMABUF_RENDERER,
      # GIO_EXTRA_MODULES) plus the launcher toolset PATH for the supervised
      # server and xdg-open. STREMIO_LAUNCHER points at the repo launcher.
      playerPkg = pkgs: pkgs.writeShellApplication {
        name = "stremio-accru";
        runtimeInputs = launcherDeps pkgs ++ playerLibs pkgs;
        text = ''
          export WEBKIT_DISABLE_DMABUF_RENDERER=1
          export GIO_EXTRA_MODULES=${pkgs.glib-networking}/lib/gio/modules''${GIO_EXTRA_MODULES:+:$GIO_EXTRA_MODULES}
          export STREMIO_LAUNCHER="${./scripts}/stremio-linux.sh"
          exec "${playerBin pkgs}/bin/stremio-accru" "$@"
        '';
      };
      playerApp = pkgs: {
        type = "app";
        program = nixpkgs.lib.getExe (playerPkg pkgs);
      };
    in
    {
      devShells = forEachSystem (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            packages = launcherDeps pkgs ++ playerBuildInputs pkgs
              ++ (with pkgs; [ pkg-config glib-networking ]);

            shellHook = ''
              export LD_LIBRARY_PATH=${nixpkgs.lib.makeLibraryPath (playerLibs pkgs)}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
              export WEBKIT_DISABLE_DMABUF_RENDERER=1
              export GIO_EXTRA_MODULES=${pkgs.glib-networking}/lib/gio/modules''${GIO_EXTRA_MODULES:+:$GIO_EXTRA_MODULES}
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
          app = playerApp pkgs;
          default = playerApp pkgs;
        });
      packages = forEachSystem (system:
        let
          pkgs = pkgsFor system;
        in
        {
          stremio-accru = playerPkg pkgs;
          default = playerPkg pkgs;
        });
    };
}
