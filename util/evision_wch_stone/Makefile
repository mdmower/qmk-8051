# Builds evision_wch_stone.bin in the directory above: the stepping stone that evision_wch_switch.py flashes through
# the Evision bootloader to start the WCH bootloader of the CH555. Needs SDCC 4.6.0 (sdcc on PATH, or SDCC=path) and
# Python 3.
SDCC ?= sdcc
SDCCFLAGS = -mmcs51 --code-loc 0x3027 --code-size 0xEFC0 --xram-loc 0x0100 --xram-size 0x0F00 -I../../lib/ch555
OUT = ../evision_wch_stone.bin

all: $(OUT)

# evision_oem_image.py adds the LJMP at 0x2800, the Esc check at 0x2803, and the interrupt table at 0x3000 in front
# of the code linked at 0x3027. evision_flash.py check then runs the flash tool's image checks.
$(OUT): .build/stone.ihx
	python3 -I ../evision_oem_image.py sdcc $< .build/stone.asm $@ --code-loc 0x3027 --code-end 0xEFC0
	python3 -I ../evision_flash.py check $@

.build/stone.ihx: stone.c ../../lib/ch555/ch555.h
	mkdir -p .build
	$(SDCC) $(SDCCFLAGS) -o .build/ stone.c

clean:
	$(RM) -r .build

.PHONY: all clean
.DELETE_ON_ERROR:
