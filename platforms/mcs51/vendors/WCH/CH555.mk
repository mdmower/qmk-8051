# CH555 specific drivers: GPIO, clock, delays, UART print output, chip ID and reset, the data flash on request, and
# the size and address of QMK's EEPROM in code flash.

CH555_LIB_DIR = $(LIB_PATH)/ch555
WCH_VENDOR_DIR = $(PLATFORM_COMMON_DIR)/vendors/WCH

VPATH += $(WCH_VENDOR_DIR)
VPATH += $(CH555_LIB_DIR)

SRC += $(WCH_VENDOR_DIR)/gpio.c
SRC += $(WCH_VENDOR_DIR)/delay.c
# Data flash driver
ifeq ($(strip $(CH555_DATA_FLASH_ENABLE)), yes)
    SRC += $(WCH_VENDOR_DIR)/data_flash.c
endif
# QMK's EEPROM in code flash (EEPROM_DRIVER = vendor): CH555_EEPROM_SIZE bytes (a multiple of 64) in xRAM at
# CH555_EEPROM_CODE_FLASH_XRAM, saved to two slots of CH555_EEPROM_SIZE + 64 bytes at the top of the app's code space;
# CODE_SIZE then ends below them, and the link and the flash image keep code there.
ifeq ($(strip $(EEPROM_DRIVER)), vendor)
    CH555_EEPROM_SIZE ?= 1024
    CH555_EEPROM_CODE_FLASH_XRAM ?= 0x1800
    CH555_EEPROM_CODE_FLASH_END := $(CODE_SIZE)
    CH555_EEPROM_CODE_FLASH_ADDR := $(shell printf '0x%04x' $$(( $(CH555_EEPROM_CODE_FLASH_END) - 2 * ($(CH555_EEPROM_SIZE) + 64) )))
    CODE_SIZE := $(CH555_EEPROM_CODE_FLASH_ADDR)
    OPT_DEFS += -DEEPROM_SIZE=$(CH555_EEPROM_SIZE)
    OPT_DEFS += -DCH555_EEPROM_CODE_FLASH_ADDR=$(CH555_EEPROM_CODE_FLASH_ADDR)
    OPT_DEFS += -DCH555_EEPROM_CODE_FLASH_END=$(CH555_EEPROM_CODE_FLASH_END)
    OPT_DEFS += -DCH555_EEPROM_CODE_FLASH_XRAM=$(CH555_EEPROM_CODE_FLASH_XRAM)
endif
SRC += $(CH555_LIB_DIR)/hal.c
SRC += $(WCH_VENDOR_DIR)/mcu_reset.c
SRC += $(WCH_VENDOR_DIR)/hardware_id.c
