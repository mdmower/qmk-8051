# Print output: SDCC's printf() (platforms/mcs51/_print.h), which writes through putchar() (the MCU's UART, mcs51_mcu.h).
# lib/printf is not used: SDCC cannot call its output functions through pointers with that many argument bytes.
QUANTUM_SRC += $(QUANTUM_DIR)/logging/print.c
