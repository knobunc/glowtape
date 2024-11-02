# Little driver makefile to make the complicated bootloader less of a hassle.

PROJECT=glowtape

TOOLCHAIN_PREFIX=arm-none-eabi-
BOARD=adafruit_feather_rp2040

build/$(PROJECT).uf2: glowtape.cc

glowtape.cc : font-message.c font-6x9.c font-timetext.c font-large.c

flash : build/$(PROJECT).uf2
	picotool load $<
	picotool reboot

build/$(PROJECT).uf2 build/$(PROJECT).elf: build FORCE
	$(MAKE) -C build
	$(TOOLCHAIN_PREFIX)size build/$(PROJECT).elf
	$(TOOLCHAIN_PREFIX)nm --print-size --size-sort --radix=d build/$(PROJECT).elf | awk '{printf("%5d %s\n", $$2, $$4);}' | sort -nr | head -20

disasm: build/$(PROJECT).elf
	$(TOOLCHAIN_PREFIX)objdump -C -S build/$(PROJECT).elf

# This requires http://github.com/hzeller/bdfont.data to be installed
# For distribution, we don't depend on %.chars, so that a git cloned project
# does not need bdfont-data-gen installed and compiles with provided files.
font-%.c: %.chars fonts/%.bdf
	bdfont-data-gen -s fonts/$*.bdf $* -C $*.chars

build:
	cmake -B build -DCMAKE_VERBOSE_MAKEFILE=OFF -DPICO_BOARD=$(BOARD)

clean:
	rm -rf build

FORCE:
