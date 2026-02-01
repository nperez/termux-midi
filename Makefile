# Package version
VERSION ?= 1.0.0

# =============================================================================
# Cross-compilation Configuration (for building outside Termux)
# =============================================================================
# Set these environment variables for NDK cross-compilation:
#
#   NDK_PATH         - Path to Android NDK installation
#                      Example: /opt/android-ndk-r26b
#
#   TERMUX_SYSROOT   - Path to Termux sysroot with headers/libs (for ALSA, etc.)
#                      Example: /path/to/termux-packages/termux-sysroot
#
#   ARCH             - Target architecture: aarch64 (default), armv7a, or x86_64
#   API_LEVEL        - Android API level (default: 24)
#   USE_ALSA         - Enable ALSA support: 1 or 0 (auto-detected from sysroot)
#
# For native Termux builds, no variables are required.
# =============================================================================

# Android NDK path (empty for native Termux build)
NDK_PATH ?=

# Termux sysroot for cross-compilation libraries
TERMUX_SYSROOT ?=

# Target architecture (aarch64 for most modern Android phones, arm for older 32-bit)
ARCH ?= aarch64
API_LEVEL ?= 24

# ALSA support (enabled by default if termux-sysroot has ALSA headers)
USE_ALSA ?= $(shell test -d "$(TERMUX_SYSROOT)/usr/include/alsa" && echo 1 || echo 0)

# NDK toolchain
ifeq ($(NDK_PATH),)
    # Native build (in Termux)
    CXX ?= $(shell command -v clang++ 2>/dev/null || echo g++)
    STRIP ?= strip
    SYSROOT =
    TERMUX_INCLUDES =
    TERMUX_LIBS =
else
    # Cross-compile with NDK
    TOOLCHAIN = $(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64
    CXX = $(TOOLCHAIN)/bin/$(ARCH)-linux-android$(API_LEVEL)-clang++
    STRIP = $(TOOLCHAIN)/bin/llvm-strip
    SYSROOT = --sysroot=$(TOOLCHAIN)/sysroot
    TERMUX_INCLUDES = -I$(TERMUX_SYSROOT)/usr/include
    TERMUX_LIBS = -L$(TERMUX_SYSROOT)/usr/lib -Wl,-rpath-link,$(TERMUX_SYSROOT)/usr/lib
endif

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -DVERSION=\"$(VERSION)\" $(SYSROOT) $(TERMUX_INCLUDES)
LDFLAGS = -lOpenSLES -pthread -static-libstdc++ $(SYSROOT) $(TERMUX_LIBS)

# Add ALSA flags if enabled
ifeq ($(USE_ALSA),1)
    CXXFLAGS += -DUSE_ALSA
    LDFLAGS += -lasound
endif

# Source files
SRCS = src/main.cpp src/audio.cpp src/synth.cpp src/midi_file.cpp src/input.cpp src/alsa_input.cpp
OBJS = $(SRCS:.cpp=.o)

# Target
TARGET = termux-midi

.PHONY: all clean info strip

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@ $(LDFLAGS)

# Build and strip
strip: $(TARGET)
	$(STRIP) $(TARGET)

# Individual object files (for incremental builds)
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

# Install to Termux bin directory
install: $(TARGET)
	cp $(TARGET) $(PREFIX)/bin/

# Development: build with debug symbols
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: clean $(TARGET)

# Show build configuration
info:
	@echo "NDK_PATH: $(NDK_PATH)"
	@echo "TERMUX_SYSROOT: $(TERMUX_SYSROOT)"
	@echo "CXX: $(CXX)"
	@echo "ARCH: $(ARCH)"
	@echo "API_LEVEL: $(API_LEVEL)"
	@echo "USE_ALSA: $(USE_ALSA)"

# Build without ALSA
no-alsa: USE_ALSA = 0
no-alsa: clean $(TARGET)

# Build for 32-bit ARM (older devices)
arm: ARCH = armv7a
arm: clean $(TARGET)

# Build for x86_64 (emulators)
x86_64: ARCH = x86_64
x86_64: clean $(TARGET)

# Build .deb package (run in Termux or with NDK)
deb: strip
	./packaging/build-deb.sh $(VERSION) $(ARCH)
