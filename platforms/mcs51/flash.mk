# Hey Emacs, this is a -*- makefile -*-
##############################################################################
# The flash target of MCS-51 keyboards (qmk flash): the flash tool of the bootloader writes the image.

flash: cpfirmware
	$(SILENT) || printf "Flashing for bootloader: $(BLUE)$(BOOTLOADER)$(NO_COLOR)\n"
ifneq ($(strip $(PROGRAM_CMD)),)
	$(UNSYNC_OUTPUT_CMD) && $(PROGRAM_CMD)
else ifeq ($(strip $(BOOTLOADER)), evision-oem)
	$(UNSYNC_OUTPUT_CMD) && $(EVISION_FLASH) flash $(BUILD_DIR)/$(TARGET).bin --poll --send
else ifeq ($(strip $(BOOTLOADER)), sinowealth-isp)
	$(UNSYNC_OUTPUT_CMD) && $(SINOWEALTH_FLASH) flash $(BUILD_DIR)/$(TARGET).hex --send
else
	$(PRINT_OK); $(SILENT) || printf "$(MSG_FLASH_BOOTLOADER)"
endif
