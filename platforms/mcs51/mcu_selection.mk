ifneq (,$(filter $(MCU),ch555))
  PROTOCOL = CH555
  # The MCU part of the platform: platforms/mcs51/vendors/$(MCU_FAMILY)/$(MCU_SERIES).mk and mcs51_mcu.h
  MCU_FAMILY = WCH
  MCU_SERIES = CH555

  # LUFA's architecture macro (tmk_core/protocol/ch555/ch555.mk), for the LUFA headers that the USB descriptors use
  ARCH = MCS51

  # Internal RAM: 256 bytes, the upper 128 only through @Ri (SDCC puts the stack and __idata there).
  IRAM_SIZE = 0x0100
  # xRAM is 0x0000-0x1FFF, but the USB buffers are fixed at 0x1000-0x13FF (pUSB_BUF_DEV0 .. pUSB_BUF_HUB in
  # lib/ch555/ch555.h), so the linker only gets 0x0100-0x0FFF. Below 0x0100: with the Evision OEM bootloader,
  # each interrupt stub of that bootloader stores 3 bytes in xdata between 0x0042 and 0x00FC. The xSFRs are at
  # 0x2100-0x22FF, not here.
  XRAM_LOC  = 0x0100
  XRAM_SIZE = 0x0f00
  # Code flash up to the data flash (0xF000); a bootloader can lower it (bootloader.mk).
  CODE_SIZE ?= 0xf000

  # Fsys; a keyboard's rules.mk may set it (CfgFsys() in lib/ch555/hal.c: 12, 16, 24 or 32 MHz)
  F_CPU ?= 24000000
endif
