# Makefile for Game Debug Trace Logger DLL with dual architecture support

# Default architecture (can be overridden with make ARCH=x86 or make ARCH=x64)
ARCH ?= x64

# Common compiler and linker settings
CXX_FLAGS = -std=c++11 -Wall -O2
LD_FLAGS = -shared -static-libgcc -static-libstdc++ -Wl,--subsystem,windows

# Architecture-specific settings
ifeq ($(ARCH),x86)
    PREFIX = i686-w64-mingw32-
    OUTPUT_DIR = bin/x86
    ARCH_FLAGS = -m32
else
    PREFIX = x86_64-w64-mingw32-
    OUTPUT_DIR = bin/x64
    ARCH_FLAGS = -m64
endif

# Compiler and linker
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
WINDRES = $(PREFIX)windres

# Input and output files
SRC = dllmain.cpp
DEF = xinput1_3.def
RC = resources.rc
OUT = $(OUTPUT_DIR)/xinput1_3.dll

# Libraries
LIBS = -lxinput -ldbghelp -lpsapi

# Build targets
all: prepare $(OUT)

prepare:
	mkdir -p $(OUTPUT_DIR)

$(OUT): $(SRC) $(DEF)
	$(CXX) $(CXX_FLAGS) $(ARCH_FLAGS) $(SRC) -o $(OUT) $(LD_FLAGS) -Wl,--output-def,$(DEF) $(LIBS)

# Build both architectures
both:
	$(MAKE) ARCH=x86
	$(MAKE) ARCH=x64

clean:
	rm -rf bin

.PHONY: all prepare both clean