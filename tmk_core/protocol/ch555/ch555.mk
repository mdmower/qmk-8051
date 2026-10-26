CH555_DIR = protocol/ch555

SRC += $(CH555_DIR)/usb_device.c
SRC += $(CH555_DIR)/ch555_usb_stack.c
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

OPT_DEFS += $(CH555_OPTS)

# This indicates using CH555 stack
OPT_DEFS += -DPROTOCOL_CH555
