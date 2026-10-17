CH555_DIR = protocol/ch555

# Path to the CH555 library
CH555_PATH = $(LIB_PATH)/ch555

SRC += $(CH555_PATH)/gpio.c
# Data flash driver
ifeq ($(strip $(CH555_DATA_FLASH_ENABLE)), yes)
    SRC += $(CH555_PATH)/data_flash.c
endif
# QMK's EEPROM in code flash (lib/ch555/eeprom_code_flash.h): CH555_EEPROM_CODE_FLASH = yes and
# EEPROM_DRIVER = custom in rules.mk. CH555_EEPROM_SIZE bytes (a multiple of 64) in xRAM at
# CH555_EEPROM_CODE_FLASH_XRAM, saved to two slots of CH555_EEPROM_SIZE + 64 bytes at the top of the app's
# code space; CODE_SIZE then ends below them, and the link and the flash image keep code there.
ifeq ($(strip $(CH555_EEPROM_CODE_FLASH)), yes)
    ifneq ($(strip $(EEPROM_DRIVER)), custom)
        $(error CH555_EEPROM_CODE_FLASH = yes needs EEPROM_DRIVER = custom)
    endif
    CH555_EEPROM_SIZE ?= 1024
    CH555_EEPROM_CODE_FLASH_XRAM ?= 0x1800
    CH555_EEPROM_CODE_FLASH_END := $(CODE_SIZE)
    CH555_EEPROM_CODE_FLASH_ADDR := $(shell printf '0x%04x' $$(( $(CH555_EEPROM_CODE_FLASH_END) - 2 * ($(CH555_EEPROM_SIZE) + 64) )))
    CODE_SIZE := $(CH555_EEPROM_CODE_FLASH_ADDR)
    SRC += $(CH555_PATH)/eeprom_code_flash.c
    CH555_OPTS += -DCH555_EEPROM_CODE_FLASH -DEEPROM_SIZE=$(CH555_EEPROM_SIZE)
    CH555_OPTS += -DCH555_EEPROM_CODE_FLASH_ADDR=$(CH555_EEPROM_CODE_FLASH_ADDR)
    CH555_OPTS += -DCH555_EEPROM_CODE_FLASH_END=$(CH555_EEPROM_CODE_FLASH_END)
    CH555_OPTS += -DCH555_EEPROM_CODE_FLASH_XRAM=$(CH555_EEPROM_CODE_FLASH_XRAM)
endif
SRC += $(CH555_PATH)/hal.c
SRC += $(CH555_DIR)/usb_device.c
SRC += ch555_usb_stack.c
SRC += usb_descriptor.c
SRC += $(CH555_DIR)/usb_util.c

# Search Path
VPATH += $(TMK_PATH)/$(CH555_DIR)
VPATH += $(CH555_PATH)
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
    # the 48-bit chip ID: 12 hex digits (26-byte descriptor, one EP0 packet)
    CH555_OPTS += -DSERIAL_NUMBER_LENGTH=12
endif

OPT_DEFS += -DARCH=ARCH_$(ARCH)
OPT_DEFS += $(CH555_OPTS)

# This indicates using CH555 stack
OPT_DEFS += -DPROTOCOL_CH555
