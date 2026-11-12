CC = cc
# Cross-compile the Windows exe when mingw-w64 is installed; plain gcc on Windows itself.
WINCC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null || echo gcc)
CFLAGS = -O2 -Wall
FRAMEWORKS = -framework CoreGraphics -framework CoreFoundation

SRC = src/main.c
BUILD_DIR = build
MAC_TARGET = $(BUILD_DIR)/autoclicker
WIN_TARGET = $(BUILD_DIR)/autoclicker.exe

all: $(MAC_TARGET)

build: $(MAC_TARGET) $(WIN_TARGET)

$(MAC_TARGET): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(FRAMEWORKS)

$(WIN_TARGET): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(WINCC) $(CFLAGS) -o $@ $< -luser32

# Usage: `make mac 200` or `make win11 200` (the number is the interval in ms).
# `make run mac 200` works too. Extra goals after mac/win11 are passed to the
# program as arguments via the no-op rule below.
ifneq (,$(filter run mac win11,$(MAKECMDGOALS)))
ARGS := $(filter-out all build clean run mac win11,$(MAKECMDGOALS))
ifneq ($(ARGS),)
$(ARGS):
	@:
endif
endif

run:
	@:

mac: $(MAC_TARGET)
	./$(MAC_TARGET) $(ARGS)

win11: $(WIN_TARGET)
	./$(WIN_TARGET) $(ARGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all build clean run mac win11
