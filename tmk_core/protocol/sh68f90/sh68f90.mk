SH68F90_DIR = protocol/sh68f90

SRC += $(SH68F90_DIR)/sh68f90.c
SRC += $(SH68F90_DIR)/usb_device.c
SRC += usb_descriptor.c

VPATH += $(TMK_PATH)/$(SH68F90_DIR)
# LUFA's descriptor definitions, as for ChibiOS
VPATH += $(TMK_PATH)/protocol/chibios/lufa_utils

# The USB interrupt handler's declaration, for SDCC's vector table
MCS51_ISR_HEADERS += $(TMK_PATH)/$(SH68F90_DIR)/usb_device.h

# EP0 packets of 8 bytes; one configuration
OPT_DEFS += -DFIXED_CONTROL_ENDPOINT_SIZE=8
OPT_DEFS += -DFIXED_NUM_CONFIGURATIONS=1

OPT_DEFS += -DPROTOCOL_SH68F90
