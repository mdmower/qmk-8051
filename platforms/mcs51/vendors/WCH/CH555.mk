# The CH555 part of the MCS-51 platform (included by platforms/mcs51/platform.mk; MCU_FAMILY and MCU_SERIES in
# mcu_selection.mk): GPIO, clock, delays, UART print output, chip ID and reset, and on request the data flash and
# QMK's EEPROM in code flash. WCH's register header and its clock, delay and UART code are in lib/ch555.

CH555_LIB_DIR = $(LIB_PATH)/ch555
WCH_VENDOR_DIR = $(PLATFORM_COMMON_DIR)/vendors/WCH

VPATH += $(WCH_VENDOR_DIR)
VPATH += $(CH555_LIB_DIR)

SRC += $(WCH_VENDOR_DIR)/gpio.c
# Data flash driver
ifeq ($(strip $(CH555_DATA_FLASH_ENABLE)), yes)
    SRC += $(WCH_VENDOR_DIR)/data_flash.c
endif
# QMK's EEPROM in code flash (platforms/mcs51/drivers/eeprom/eeprom_ch555_code_flash.h): CH555_EEPROM_CODE_FLASH =
# yes and EEPROM_DRIVER = custom in rules.mk. CH555_EEPROM_SIZE bytes (a multiple of 64) in xRAM at
# CH555_EEPROM_CODE_FLASH_XRAM, saved to two slots of CH555_EEPROM_SIZE + 64 bytes at the top of the app's code space;
# CODE_SIZE (bootloader.mk) then ends below them, and the link and the flash image keep code there.
ifeq ($(strip $(CH555_EEPROM_CODE_FLASH)), yes)
    ifneq ($(strip $(EEPROM_DRIVER)), custom)
        $(error CH555_EEPROM_CODE_FLASH = yes needs EEPROM_DRIVER = custom)
    endif
    CH555_EEPROM_SIZE ?= 1024
    CH555_EEPROM_CODE_FLASH_XRAM ?= 0x1800
    CH555_EEPROM_CODE_FLASH_END := $(CODE_SIZE)
    CH555_EEPROM_CODE_FLASH_ADDR := $(shell printf '0x%04x' $$(( $(CH555_EEPROM_CODE_FLASH_END) - 2 * ($(CH555_EEPROM_SIZE) + 64) )))
    CODE_SIZE := $(CH555_EEPROM_CODE_FLASH_ADDR)
    SRC += $(PLATFORM_COMMON_DIR)/drivers/eeprom/eeprom_ch555_code_flash.c
    OPT_DEFS += -DCH555_EEPROM_CODE_FLASH -DEEPROM_SIZE=$(CH555_EEPROM_SIZE)
    OPT_DEFS += -DCH555_EEPROM_CODE_FLASH_ADDR=$(CH555_EEPROM_CODE_FLASH_ADDR)
    OPT_DEFS += -DCH555_EEPROM_CODE_FLASH_END=$(CH555_EEPROM_CODE_FLASH_END)
    OPT_DEFS += -DCH555_EEPROM_CODE_FLASH_XRAM=$(CH555_EEPROM_CODE_FLASH_XRAM)
endif
SRC += $(CH555_LIB_DIR)/hal.c
SRC += $(WCH_VENDOR_DIR)/mcu_reset.c
SRC += $(WCH_VENDOR_DIR)/hardware_id.c
