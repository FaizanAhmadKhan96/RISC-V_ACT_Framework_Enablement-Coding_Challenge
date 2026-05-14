# ============================================================
# Makefile — uart_riscv UART Interface
# ============================================================
# Usage:
#   make          → build the binary
#   make clean    → remove build artifacts
#   make help     → print usage hints
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=gnu11 -g
TARGET  = uart_riscv
SRC     = uart_riscv.c

.PHONY: all clean help

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build successful → ./$(TARGET)"

clean:
	rm -f $(TARGET)

help:
	@echo "Build:  make"
	@echo "Run:    sudo ./uart_riscv /dev/ttyUSB0 115200"
	@echo "Clean:  make clean"
