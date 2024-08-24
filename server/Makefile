# Default target
all: aesdsocket

# Cross-compiler prefix (update according to your toolchain)
CROSS_COMPILE ?= aarch64-none-linux-gnu-

# Compiler and linker commands
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror

# Target executable
aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

# Clean command
clean:
	rm -f aesdsocket
