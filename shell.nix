# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
let
  local-pico-sdk = pkgs.pico-sdk.override {
     withSubmodules = true;
  };
in
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      gcc-arm-embedded
      local-pico-sdk
      picotool
      cmake python3   # build requirements for pico-sdk

      openscad-unstable
      openscad-lsp
      prusa-slicer
      ghostscript

      kicad
      gerbv
      python3
      python312Packages.kicad
      zip
      pcb2gcode
      pstoedit
    ];
  shellHook = ''
   export PICO_SDK_PATH=${local-pico-sdk}/lib/pico-sdk
  '';
}
