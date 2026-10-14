# The CH555 LED controller drives the matrix columns (its COM pins), so its interrupt (led_dma.c) reads the
# key rows and matrix.c takes them from there (matrix_pins.custom_lite in keyboard.json).
SRC += led_dma.c led_fx.c matrix.c
OPT_DEFS += -DCH555_LED_DMA_ENABLE
# The LED interrupt handler (platforms/mcs51/platform.mk)
MCS51_ISR_HEADERS += led_dma.h
# Fsys 32 MHz, as stock 1.04 (CH555 default here: 24 MHz). The LED frame, and with it the key sampling period,
# is 304128 clocks: 9.5 ms at 32 MHz, 12.7 ms at 24 MHz.
F_CPU = 32000000
# Lighting settings saved in the data flash (flash_store.c)
SRC += flash_store.c
CH555_DATA_FLASH_ENABLE = yes

# No EEPROM: QMK's settings (eeconfig) are kept in RAM until the keyboard loses power.
EEPROM_DRIVER = transient

# USB serial number, optional: qmk compile ... -e SERIAL_NUMBER=<serial>. Without it, the serial number is the
# chip ID (SERIAL_NUMBER_USE_HARDWARE_ID in tmk_core/protocol/ch555/ch555.mk). Letters, digits and . _ : - only
# (the value passes through the shell), at most 126 characters.
ifneq ($(strip $(SERIAL_NUMBER)),)
    OPT_DEFS += -DSERIAL_NUMBER=\"$(strip $(SERIAL_NUMBER))\"
endif

# Disabled for flash space. Remove a line to use the feature again.
# - Grave Escape (QK_GESC), Magic keycodes, Space Cadet and send_string() (macros, QK_MAKE)
GRAVE_ESC_ENABLE = no
MAGIC_ENABLE = no
SPACE_CADET_ENABLE = no
SEND_STRING_ENABLE = no
