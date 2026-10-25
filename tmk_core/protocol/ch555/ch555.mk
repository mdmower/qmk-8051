CH555_DIR = protocol/ch555

SRC += $(CH555_DIR)/usb_device.c
SRC += ch555_usb_stack.c
SRC += usb_descriptor.c
SRC += $(CH555_DIR)/usb_util.c

# Search Path
VPATH += $(TMK_PATH)/$(CH555_DIR)
# LUFA's descriptor definitions, as for ChibiOS
VPATH += $(TMK_PATH)/protocol/chibios/lufa_utils

# The USB interrupt handler (platform.mk)
MCS51_ISR_HEADERS += $(TMK_PATH)/$(CH555_DIR)/usb_device.h

# CH555 library compile-time options and predefined tokens
CH555_OPTS += -DFIXED_CONTROL_ENDPOINT_SIZE=64
CH555_OPTS += -DFIXED_NUM_CONFIGURATIONS=1

# USB serial number from the chip ID (get_hardware_id()) when SERIAL_NUMBER is not set;
# SERIAL_NUMBER_USE_HARDWARE_ID = no in a keyboard's rules.mk leaves the device without a serial number.
# usb_descriptor.c gives the device descriptor a serial number index with SERIAL_NUMBER_USE_HARDWARE_ID == TRUE.
SERIAL_NUMBER_USE_HARDWARE_ID ?= yes
ifeq ($(strip $(SERIAL_NUMBER_USE_HARDWARE_ID)), yes)
    CH555_OPTS += -DSERIAL_NUMBER_USE_HARDWARE_ID=TRUE
endif

OPT_DEFS += -DARCH=ARCH_$(ARCH)
OPT_DEFS += $(CH555_OPTS)

# This indicates using CH555 stack
OPT_DEFS += -DPROTOCOL_CH555
