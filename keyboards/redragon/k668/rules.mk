# Keyboard and extra key reports share EP1; the SH68F90 protocol needs it.
KEYBOARD_SHARED_EP = yes

# The PWM0 interrupt handler (led_pwm.c) drives the matrix rows, which are also the LED columns, and reads the keys;
# matrix.c takes them from there (matrix_pins.custom_lite in keyboard.json).
SRC += led_pwm.c matrix.c
# Lighting effects
SRC += keyboards/redragon/common/led_fx.c
# The PWM0 interrupt handler's declaration, for SDCC's vector table
MCS51_ISR_HEADERS += led_pwm.h

# USB serial number, optional: qmk compile ... -e SERIAL_NUMBER=<serial>. Without it, the serial number is the
# chip ID (SERIAL_NUMBER_USE_HARDWARE_ID). Letters, digits and . _ : - only (the value passes through the shell),
# at most 126 characters.
ifneq ($(strip $(SERIAL_NUMBER)),)
    OPT_DEFS += -DSERIAL_NUMBER=\"$(strip $(SERIAL_NUMBER))\"
endif
