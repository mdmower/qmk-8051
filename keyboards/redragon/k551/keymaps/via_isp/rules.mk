VIA_ENABLE = yes
# QMK's EEPROM (the settings, the keymap and the macros) in code flash; the keyboard's default is transient
EEPROM_DRIVER = vendor
# Image linked at 0x0000 for the WCH bootloader, which replaces the OEM bootloader: about 10 KB more code space
BOOTLOADER = wch-isp
# Features the keyboard's rules.mk disables for flash space
GRAVE_ESC_ENABLE = yes
MAGIC_ENABLE = yes
SPACE_CADET_ENABLE = yes
# Tap keys: their spill locations do not fit in direct RAM otherwise
MCS51_FEWER_SPILLS_SRC += quantum/action_tapping.c
