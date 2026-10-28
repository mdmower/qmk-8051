# Keyboard and extra key reports share EP1; the SH68F90 protocol needs it.
KEYBOARD_SHARED_EP = yes

# USB serial number, optional: qmk compile ... -e SERIAL_NUMBER=<serial>. Without it, the serial number is the
# chip ID (SERIAL_NUMBER_USE_HARDWARE_ID). Letters, digits and . _ : - only (the value passes through the shell),
# at most 126 characters.
ifneq ($(strip $(SERIAL_NUMBER)),)
    OPT_DEFS += -DSERIAL_NUMBER=\"$(strip $(SERIAL_NUMBER))\"
endif
