# SH68F90 specific drivers: GPIO, clock, delays, chip ID and reset, and the size and address of QMK's EEPROM in code
# flash.

SINOWEALTH_VENDOR_DIR = $(PLATFORM_COMMON_DIR)/vendors/SINOWEALTH

VPATH += $(SINOWEALTH_VENDOR_DIR)

SRC += $(SINOWEALTH_VENDOR_DIR)/gpio.c
SRC += $(SINOWEALTH_VENDOR_DIR)/clock.c
SRC += $(SINOWEALTH_VENDOR_DIR)/delay.c
# QMK's EEPROM in code flash (EEPROM_DRIVER = vendor): SH68F90_EEPROM_SIZE bytes (512 * n - 16), saved to two
# slots of SH68F90_EEPROM_SIZE + 16 bytes that end at the last 512-byte sector boundary at or below CODE_SIZE, so that
# the store shares no sector with the flash above CODE_SIZE. CODE_SIZE then ends below them.
ifeq ($(strip $(EEPROM_DRIVER)), vendor)
    SH68F90_EEPROM_SIZE ?= 496
    SH68F90_EEPROM_CODE_FLASH_END := $(shell printf '0x%04x' $$(( $(CODE_SIZE) & ~0x1ff )))
    SH68F90_EEPROM_CODE_FLASH_ADDR := $(shell printf '0x%04x' $$(( $(SH68F90_EEPROM_CODE_FLASH_END) - 2 * ($(SH68F90_EEPROM_SIZE) + 16) )))
    CODE_SIZE := $(SH68F90_EEPROM_CODE_FLASH_ADDR)
    OPT_DEFS += -DEEPROM_SIZE=$(SH68F90_EEPROM_SIZE)
    OPT_DEFS += -DSH68F90_EEPROM_CODE_FLASH_ADDR=$(SH68F90_EEPROM_CODE_FLASH_ADDR)
    OPT_DEFS += -DSH68F90_EEPROM_CODE_FLASH_END=$(SH68F90_EEPROM_CODE_FLASH_END)
    # Compiled with fewer optimizations, so that its loops need no spill locations in direct RAM.
    MCS51_FEWER_SPILLS_SRC += eeprom_sh68f90_code_flash.c
endif
SRC += $(SINOWEALTH_VENDOR_DIR)/mcu_reset.c
SRC += $(SINOWEALTH_VENDOR_DIR)/hardware_id.c
