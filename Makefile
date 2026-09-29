# vdlaunch - drop-in virtual desktop launcher
#
# Two artifacts, same feature set:
#   vdlaunch64.exe  x64 targets (the usual pick)
#   vdlaunch32.exe  x86 host; also wraps 64-bit targets
#
# The desktop calls all go through explorer's COM server and are cross-process,
# so neither build needs a same-bitness helper.

MINGW64 ?= x86_64-w64-mingw32-
MINGW32 ?= i686-w64-mingw32-
CXX64   := $(MINGW64)g++
CXX32   := $(MINGW32)g++
WINDRES ?= $(MINGW64)windres

SRCS       := src/main.cpp src/launch.cpp src/config.cpp src/ini.cpp src/desktop.cpp \
              src/switches.cpp src/util.cpp
CREATOR_SRCS := src/main.cpp src/creator.cpp src/util.cpp
HDRS       := $(wildcard src/*.h)

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti
LDFLAGS  := -static -s -mwindows
LIBS     := -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -luser32

BUILD    := build
DIST     := dist
TESTDIST := dist-test

.PHONY: all clean dist test console

all: $(DIST)/vdlaunch64.exe $(DIST)/vdlaunch32.exe $(DIST)/vdlaunchCreator.exe

# console-subsystem twins so the test harness can capture stdout
console: $(TESTDIST)/vdlaunch64c.exe $(TESTDIST)/vdlaunch32c.exe

$(BUILD) $(DIST) $(TESTDIST):
	@mkdir -p $@

$(BUILD)/%.64.o: src/%.cpp $(HDRS) | $(BUILD)
	$(CXX64) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.creator.o: src/%.cpp $(HDRS) | $(BUILD)
	$(CXX64) $(CXXFLAGS) -DVDLAUNCH_CREATOR=1 -c $< -o $@

$(BUILD)/creator.res: src/creator.rc $(DIST)/vdlaunch64.exe $(DIST)/vdlaunch32.exe | $(BUILD)
	$(WINDRES) -I. $< -O coff -o $@

$(BUILD)/%.32.o: src/%.cpp $(HDRS) | $(BUILD)
	$(CXX32) $(CXXFLAGS) -c $< -o $@

OBJ64 := $(patsubst src/%.cpp,$(BUILD)/%.64.o,$(SRCS))
OBJ32 := $(patsubst src/%.cpp,$(BUILD)/%.32.o,$(SRCS))

$(DIST)/vdlaunch64.exe: $(OBJ64) | $(DIST)
	$(CXX64) $(LDFLAGS) $^ -o $@ $(LIBS)

$(DIST)/vdlaunch32.exe: $(OBJ32) | $(DIST)
	$(CXX32) $(LDFLAGS) $^ -o $@ $(LIBS)

# Console subsystem: the creator is a prompt-driven tool, and it embeds both
# launchers as RCDATA so one binary serves either target bitness.
OBJCREATOR := $(patsubst src/%.cpp,$(BUILD)/%.creator.o,$(CREATOR_SRCS))

$(DIST)/vdlaunchCreator.exe: $(OBJCREATOR) $(BUILD)/creator.res
	$(CXX64) -static -s -mconsole $^ -o $@ $(LIBS)

$(TESTDIST)/vdlaunch64c.exe: $(OBJ64) | $(TESTDIST)
	$(CXX64) -static -s -mconsole $^ -o $@ $(LIBS)

$(TESTDIST)/vdlaunch32c.exe: $(OBJ32) | $(TESTDIST)
	$(CXX32) -static -s -mconsole $^ -o $@ $(LIBS)

clean:
	rm -rf $(BUILD) $(DIST) $(TESTDIST)

test: all console
	./test/run.sh
