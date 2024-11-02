{ pkgs ? import <nixpkgs> {} }:
let
  local-pico-sdk = pkgs.pico-sdk.override {
     withSubmodules = true;
  };

  # gcode-cli is perfect to send a line to tty and awaiting OK
  gcode-cli = pkgs.stdenv.mkDerivation rec {
    name = "gcode-cli";
    src = pkgs.fetchFromGitHub {
      owner = "hzeller";
      repo = "gcode-cli";
      rev = "v0.9";
      hash = "sha256-L9hUleslnTd5LWm2ZgkgkiKq/UTQP3CuaorAkiKXoPk=";
    };
    buildPhase = "make";
    installPhase = "mkdir -p $out/bin; install gcode-cli $out/bin";
  };

  # Generating compressed fonts from BDF for use in embedded device.
  bdfont-data = pkgs.stdenv.mkDerivation rec {
    name = "bdfont-data";
    src = pkgs.fetchFromGitHub {
      owner = "hzeller";
      repo = "bdfont.data";
      rev = "v1.0";
      hash = "sha256-1QoCnX0L+GH8ufMRI4c9N6q0Jh2u3vDZn+YqnWMQe5M=";
    };
    postPatch = "patchShebangs src/make-inc.sh";
    buildPhase = "make -C src";
    installPhase = "mkdir -p $out/bin; install src/bdfont-data-gen $out/bin";
  };
in
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      # rp2040 toolchain
      gcc-arm-embedded
      local-pico-sdk
      picotool
      cmake python3   # build requirements for pico-sdk

      # Tools needed
      bdfont-data   # Generated embedded fonts
      gcode-cli     # Tool to sync time from host to glowtape

      # Generating casing and sync-tape
      openscad-unstable
      openscad-lsp
      prusa-slicer
      ghostscript

      # EDA tooling.
      kicad
      gerbv
      python312Packages.kicad
      zip
    ];
  shellHook = ''
   export PICO_SDK_PATH=${local-pico-sdk}/lib/pico-sdk
  '';
}
