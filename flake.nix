{
  description = "stremio-accru Linux launcher (nix run .#linux; shell #37, launcher #38)";

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
    in
    {
      devShells = forEachSystem (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            packages = launcherDeps pkgs ++ (with pkgs; [
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
          default = launcherApp pkgs;
        });
    };
}
