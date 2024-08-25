# Default target
all: aesdsocket

# Compiler and linker commands for native build
CC = gcc
CFLAGS = -Wall -Werror
LIBS = -lpthread

# Override for cross-compilation
ifeq ($(CROSS_COMPILE),1)
	CROSS_COMPILE_PREFIX ?= aarch64-none-linux-gnu-
	CC = $(CROSS_COMPILE_PREFIX)gcc
endif

# Target executable
aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LIBS)

# Clean command
clean:
	rm -f aesdsocket
