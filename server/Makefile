# Variables
TARGET = aesdsocket
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -Werror
LDFLAGS = -lpthread

# Default target
all: $(TARGET)

# Compile the target
$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean
