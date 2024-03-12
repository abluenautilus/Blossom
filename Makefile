# Project Name
TARGET = Blossom

# Sources
CPP_SOURCES = Blossom.cpp
DEBUG=0

# Library Locations
LIBDAISY_DIR = ./libDaisy
DAISYSP_DIR = ./DaisySP

# Linker flags
# This is not really required, used only for profiling! Increases executable size by ~8kB
LDFLAGS = -u _printf_float

#Optimization
OPT = -Os

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

