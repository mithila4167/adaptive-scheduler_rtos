CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude
SRCDIR = src
TARGET = rtos-sim

# Collect all .c in src, but exclude main.c; use test as entry point
SRC_ALL = $(wildcard $(SRCDIR)/*.c)
SOURCES = $(filter-out $(SRCDIR)/main.c,$(SRC_ALL)) tests/test_producer_consumer.c
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
$(CC) $(OBJECTS) -o $(TARGET)

# Compile source files to object files
%.o: %.c
$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
rm -f $(OBJECTS) $(TARGET)

# Declare phony targets
.PHONY: all clean
