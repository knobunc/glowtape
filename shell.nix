# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
let
  # Needs to be synced past
  # https://github.com/NixOS/nixpkgs/pull/321786
  local-pico-sdk = pkgs.pico-sdk.override {
     withSubmodules = true;
  };
in
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      gcc-arm-embedded
      local-pico-sdk
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

    ];
  shellHook = ''
   export PICO_SDK_PATH=${local-pico-sdk}/lib/pico-sdk
  '';
}
