# Bootloaders of MCS-51 keyboards; set BOOTLOADER in rules.mk or "bootloader" in keyboard.json.
#
# Current options for the CH555 (MCU = ch555):
#     ch55xbl      WCH factory bootloader (USB 4348:55e0) in ROM at 0xF400; the image starts at 0x0000
#     evision-oem  Evision OEM bootloader (USB 0c45:7687) at 0x0000-0x27FF; the image starts at 0x2800
#
# BOOTLOADER = custom: the keyboard supplies bootloader_jump() (bootloaders/custom.c).

ifeq ($(strip $(BOOTLOADER)), custom)
    OPT_DEFS += -DBOOTLOADER_CUSTOM
    BOOTLOADER_TYPE = custom
endif

ifeq ($(strip $(BOOTLOADER)), ch55xbl)
    ifneq ($(strip $(MCU)), ch555)
        $(call CATASTROPHIC_ERROR,Invalid BOOTLOADER,Bootloader ch55xbl needs MCU = ch555.)
    endif
    OPT_DEFS += -DBOOTLOADER_CH55XBL
    BOOTLOADER_TYPE = ch55xbl
endif

ifeq ($(strip $(BOOTLOADER)), evision-oem)
    ifneq ($(strip $(MCU)), ch555)
        $(call CATASTROPHIC_ERROR,Invalid BOOTLOADER,Bootloader evision-oem needs MCU = ch555.)
    endif
    OPT_DEFS += -DBOOTLOADER_EVISION_OEM
    BOOTLOADER_TYPE = evision_oem
    MCS51_CODE_LOC = 0x3027
    # The app's last address is 0xEFBF: the bootloader erases and writes 0x2800-0xEFBF.
    CODE_SIZE = 0xefc0
    FIRMWARE_FORMAT = bin
endif

ifeq ($(strip $(BOOTLOADER_TYPE)),)
    ifneq ($(strip $(BOOTLOADER)),)
        $(call CATASTROPHIC_ERROR,Invalid BOOTLOADER,Invalid bootloader specified. Please set an appropriate bootloader in your rules.mk or info.json.)
    else
        $(call CATASTROPHIC_ERROR,Invalid BOOTLOADER,No bootloader specified. Please set an appropriate bootloader in your rules.mk or info.json.)
    endif
endif
