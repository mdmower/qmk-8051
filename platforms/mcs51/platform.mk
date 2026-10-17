# Hey Emacs, this is a -*- makefile -*-
##############################################################################
# MCS-51 (8051) with SDCC.
#
# $(CC) is sdcc_cc.py, a GCC-style driver for SDCC: QMK's compile and link rules (builddefs/common_rules.mk) call
# it as they call GCC. It resolves weak symbols at the link (clang lists them) and stops at constructs that SDCC
# compiles to wrong code (clang-query); the QMK core is edited where SDCC does not take its C. The link writes
# $(TARGET).ihx and converts it to $(TARGET).elf, from which the .hex and .bin rules make the firmware. Needs
# clang, clang-query and llvm-nm besides SDCC.

# Compiler settings
#
CC = python3 $(PLATFORM_COMMON_DIR)/sdcc_cc.py
OBJCOPY = objcopy
OBJDUMP = objdump
SIZE = size
NM = nm
AR = sdar
HEX = $(OBJCOPY) -O $(FORMAT)
BIN = $(OBJCOPY) -O binary
EEP =

MCUFLAGS = -mmcs51 --model-large
MCUFLAGS += --iram-size $(IRAM_SIZE) --xram-size $(XRAM_SIZE) --xram-loc $(XRAM_LOC) --code-size $(CODE_SIZE)
ifneq ($(strip $(MCS51_CODE_LOC)),)
    MCUFLAGS += --code-loc $(MCS51_CODE_LOC)
endif

# warning  85: unreferenced function argument
# warning 115: unsupported pragma
# warning 190: empty translation unit
CFLAGS += --std-sdcc11
CFLAGS += --disable-warning 85 --disable-warning 115 --disable-warning 190
CFLAGS += --opt-code-speed
CFLAGS += -DF_CPU=$(F_CPU)
# Definitions that replace GCC built-ins in QMK headers, included before everything else
CFLAGS += -include $(PLATFORM_COMMON_DIR)/sdcc_compat.h

# SDCC puts an interrupt handler into its vector table only if main.c sees its declaration (`__interrupt(n)`).
# The platform, the protocol and the keyboard (rules.mk) add the headers that declare their handlers.
MCS51_ISR_HEADERS += $(PLATFORM_COMMON_DIR)/_timer.h
$(INTERMEDIATE_OUTPUT)/quantum/main.o: FILE_SPECIFIC_CFLAGS += $(foreach h,$(MCS51_ISR_HEADERS),-include $(h))

# NVM_DRIVER = custom (rules.mk): QMK settings from quantum/nvm/eeprom, VIA and dynamic keymap storage from nvm/,
# all on the EEPROM driver. The upstream VIA and dynamic keymap files of quantum/nvm/eeprom use void-pointer
# arithmetic, which SDCC compiles wrong (sdcc_check.query); nvm/ keeps their EEPROM layout.
ifeq ($(strip $(NVM_DRIVER)), custom)
    VPATH += $(QUANTUM_DIR)/nvm/eeprom
    ifeq ($(strip $(VIA_ENABLE)), yes)
        SRC += $(PLATFORM_COMMON_DIR)/nvm/nvm_via.c
    endif
    ifeq ($(strip $(DYNAMIC_KEYMAP_ENABLE)), yes)
        SRC += $(PLATFORM_COMMON_DIR)/nvm/nvm_dynamic_keymap.c
    endif
endif

# SDCC keeps values that do not fit in registers in spill locations in direct RAM (0x08-0x7F), which a K580
# build without VIA fills to one byte. In VIA builds these files are compiled without global CSE, induction,
# invariant and loop-reversal optimization, which removes most of their spill locations.
ifeq ($(strip $(VIA_ENABLE)), yes)
    MCS51_FEWER_SPILLS_SRC += quantum/via.c quantum/dynamic_keymap.c quantum/action_layer.c
    MCS51_FEWER_SPILLS_SRC += quantum/send_string/send_string.c quantum/keyboard.c quantum/quantum.c
    MCS51_FEWER_SPILLS_SRC += $(PLATFORM_COMMON_DIR)/nvm/nvm_via.c
endif
$(foreach f,$(MCS51_FEWER_SPILLS_SRC),$(eval $(INTERMEDIATE_OUTPUT)/$(f:.c=.o): FILE_SPECIFIC_CFLAGS += --nogcse --noinduction --noinvariant --noloopreverse))

bin: $(BUILD_DIR)/$(TARGET).bin

# Evision OEM bootloader: the flash image based at 0x2800 (bootloader.mk)
ifeq ($(strip $(BOOTLOADER)), evision-oem)
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@$(SILENT) || printf "$(MSG_BIN) $@" | $(AWK_CMD)
	$(eval CMD=python3 $(TOP_DIR)/util/evision_oem_image.py sdcc $(BUILD_DIR)/$(TARGET).ihx $(INTERMEDIATE_OUTPUT)/quantum/main.asm $@ --code-loc $(MCS51_CODE_LOC) --code-end $(CODE_SIZE))
	@$(BUILD_CMD)
endif

# Code end and internal RAM, from the linker's memory map
check-size: build
	@$(SILENT) || grep -E "ROM/EPROM/FLASH|EXTERNAL RAM|Stack starts" $(BUILD_DIR)/$(TARGET).mem
