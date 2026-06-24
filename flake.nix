{
  description = "FT-1210 native module DJ deck";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config
            clang-tools
            SDL2
            libopenmpt
            zlib
          ];
        };
      });

      packages = forAllSystems (pkgs: {
        default = pkgs.stdenv.mkDerivation {
          pname = "ft1210";
          version = "0.1.0";
          src = pkgs.lib.cleanSourceWith {
            src = self;
            filter = path: type:
              let base = baseNameOf path;
              in !(base == "build" || base == "result" || base == ".git");
          };
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs; [ SDL2 libopenmpt zlib ];
        };
      });
    };
}
