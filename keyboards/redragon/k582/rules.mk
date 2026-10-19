include keyboards/redragon/common/redragon_ch555.mk

# Fsys 32 MHz, as the stock firmware V0107. The LED frame, and with it the key sampling period, is 270336 clocks:
# 8.4 ms at 32 MHz.
F_CPU = 32000000

# No EEPROM: QMK's settings (eeconfig) are kept in RAM until the keyboard loses power.
EEPROM_DRIVER = transient

# Disabled for flash space, as on the K580. Remove a line to use the feature again.
# - Grave Escape (QK_GESC), Magic keycodes, Space Cadet and send_string() (macros, QK_MAKE)
GRAVE_ESC_ENABLE = no
MAGIC_ENABLE = no
SPACE_CADET_ENABLE = no
SEND_STRING_ENABLE = no
