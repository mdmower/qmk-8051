# Bootloaders of MCS-51 (CH555) keyboards; set BOOTLOADER in rules.mk or "bootloader" in keyboard.json.
#
# ch55xbl:      the WCH factory bootloader (USB 4348:55e0) at 0xF400. The image starts at 0x0000.
#               bootloader_jump() calls it.
# evision-oem:  the Evision OEM bootloader (USB 0c45:7687) of the Redragon K580 and other boards with an Evision
#               VS11K33A. It occupies 0x0000-0x27FF and starts the app at 0x2800 while its boot marker
#               (AA 55 33 CC at 0x27C0) is present; it forwards every interrupt to a table at 0x3000. The app is
#               linked at 0x3027, and util/evision_oem_image.py makes the image based at 0x2800 (the LJMP at
#               0x2800, the interrupt table and a check of the Esc key, row 0 on P4.0 and column 0 on P0.0).
#               bootloader_jump() erases the boot marker and resets.
# custom:       platforms/mcs51/bootloaders/custom.c, for a keyboard that supplies bootloader_jump().

ifeq ($(strip $(BOOTLOADER)), custom)
    OPT_DEFS += -DBOOTLOADER_CUSTOM
    BOOTLOADER_TYPE = custom
endif

ifeq ($(strip $(BOOTLOADER)), ch55xbl)
    OPT_DEFS += -DBOOTLOADER_CH55XBL
    BOOTLOADER_TYPE = ch55xbl
endif

ifeq ($(strip $(BOOTLOADER)), evision-oem)
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
