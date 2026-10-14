# Bootloaders of MCS-51 (CH555) keyboards; set BOOTLOADER in rules.mk or "bootloader" in keyboard.json.
#
# ch55xbl:      the WCH factory bootloader (USB 4348:55e0) at 0xF400. The image starts at 0x0000.
#               bootloader_jump() calls it.
# custom:       platforms/mcs51/bootloaders/custom.c, for a keyboard that supplies bootloader_jump().

ifeq ($(strip $(BOOTLOADER)), custom)
    OPT_DEFS += -DBOOTLOADER_CUSTOM
    BOOTLOADER_TYPE = custom
endif

ifeq ($(strip $(BOOTLOADER)), ch55xbl)
    OPT_DEFS += -DBOOTLOADER_CH55XBL
    BOOTLOADER_TYPE = ch55xbl
endif

ifeq ($(strip $(BOOTLOADER_TYPE)),)
    ifneq ($(strip $(BOOTLOADER)),)
        $(call CATASTROPHIC_ERROR,Invalid BOOTLOADER,Invalid bootloader specified. Please set an appropriate bootloader in your rules.mk or info.json.)
    else
        $(call CATASTROPHIC_ERROR,Invalid BOOTLOADER,No bootloader specified. Please set an appropriate bootloader in your rules.mk or info.json.)
    endif
endif
