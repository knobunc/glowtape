Firmware glowtape
=================

Makre sure you have all dependencies (on nix: get a nix-shell, otherwise get
pico SDK and compiler toolchain).

Then simply:

```
make
```

... and once Feather is connected with bootsel enabled

```
make flash
```

After the flash, the RP2040 RTC lost all time information, so you have to
set it again with

```
../set-time.sh
```
