# Little driver makefile to make the complicated bootloader less of a hassle.

PROJECT=glowtape

TOOLCHAIN_PREFIX=arm-none-eabi-
BOARD=adafruit_feather_rp2040

build/$(PROJECT).uf2: glowtape.cc

glowtape.cc : bdfont-support.h \
              font-message.c font-6x9.c font-timetext.c font-large.c

flash : build/$(PROJECT).uf2
	picotool load $<
	picotool reboot

build/$(PROJECT).uf2 build/$(PROJECT).elf: build FORCE
	$(MAKE) -C build
	$(TOOLCHAIN_PREFIX)size build/$(PROJECT).elf
	$(TOOLCHAIN_PREFIX)nm --print-size --size-sort --radix=d build/$(PROJECT).elf | awk '{printf("%5d %s\n", $$2, $$4);}' | sort -nr | head -20

disasm: build/$(PROJECT).elf
	$(TOOLCHAIN_PREFIX)objdump -C -S build/$(PROJECT).elf

# -- the following should be done in cmake
# This requires http://github.com/hzeller/bdfont.data to be installed
# (provided already from the shell.nix)
font-%.c: %.chars fonts/%.bdf
	bdfont-data-gen fonts/$*.bdf $* -C $*.chars

bdfont-support.h:
	bdfont-data-gen -s

build:
	cmake -B build -DCMAKE_VERBOSE_MAKEFILE=OFF -DPICO_BOARD=$(BOARD)

clean:
	rm -rf build bdfont-support.{h,c} font-*.{h,c}

FORCE:
