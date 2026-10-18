# Shared code of the Redragon keyboards with an Evision CH555-family MCU (VS11K28A, VS11K33A): included at the top
# of the keyboard's rules.mk.
#
# The CH555 LED controller drives the matrix columns (its COM pins), so its interrupt (led_dma.c) reads the key rows
# and matrix.c takes them from there (matrix_pins.custom_lite in keyboard.json).
VPATH += keyboards/redragon/common
SRC += led_dma.c led_fx.c matrix.c
# The LED interrupt handler (platforms/mcs51/platform.mk)
MCS51_ISR_HEADERS += led_dma.h
# Lighting settings saved in the data flash (flash_store.c)
SRC += flash_store.c
CH555_DATA_FLASH_ENABLE = yes

# USB serial number, optional: qmk compile ... -e SERIAL_NUMBER=<serial>. Without it, the serial number is the
# chip ID (SERIAL_NUMBER_USE_HARDWARE_ID in tmk_core/protocol/ch555/ch555.mk). Letters, digits and . _ : - only
# (the value passes through the shell), at most 126 characters.
ifneq ($(strip $(SERIAL_NUMBER)),)
    OPT_DEFS += -DSERIAL_NUMBER=\"$(strip $(SERIAL_NUMBER))\"
endif
