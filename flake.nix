{
  description = "Konfyt, Digital Keyboard Workstation for Linux";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; config.allowUnfree = true; };
      in
      rec {
        packages.default = pkgs.stdenv.mkDerivation {
          name = "konfyt";
          src = ./.;

          nativeBuildInputs = with pkgs.qt5; [ qmake wrapQtAppsHook ];
          buildInputs = with pkgs; [
            qt5.full
            liblscp
            linuxsampler
            pkg-config
            carla
            fluidsynth
            jack2
          ];

          installPhase = ''
            mkdir -p $out/bin
            cp konfyt $out/bin
            wrapProgram $out/bin/konfyt --set PATH ${pkgs.lib.makeBinPath [ pkgs.linuxsampler ]}
          '';
        };
      });
}

