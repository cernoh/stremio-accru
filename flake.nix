{
  description = "stremio-accru Linux shell (nix + direnv, see #37)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forEachSystem = nixpkgs.lib.genAttrs systems;
      pkgsFor = system: import nixpkgs { inherit system; };
    in
    {
      devShells = forEachSystem (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              # Launcher runtime (#38): server.js + media + fetch
              nodejs
              mpv
              ffmpeg
              curl
              jq
              xdg-utils
              p7zip
              # Portable core build deps (mpv client, updater verify)
              cmake
              ninja
              pkg-config
              openssl
              nlohmann_json
              # Future GTK host (#36 proposal): WebKitGTK webview
              webkitgtk_6_0
            ];

            shellHook = ''
              command -v node >/dev/null && command -v mpv >/dev/null \
                || echo "stremio-accru shell: warning, node/mpv missing from PATH"
            '';
          };
        });
    };
}
