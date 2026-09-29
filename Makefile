# vdlaunch - drop-in virtual desktop launcher
#
# Builds three artifacts:
#   vdlaunch64.exe  x86-64 launcher
#   vdlaunch32.exe  x86 launcher
#   vdlaunch.exe    auto dispatcher (ships next to both of the above)

MINGW64 ?= x86_64-w64-mingw32-
MINGW32 ?= i686-w64-mingw32-
CXX64   := $(MINGW64)g++
CXX32   := $(MINGW32)g++
WINDRES ?= $(MINGW64)windres

SRCS    := src/main.cpp src/launch.cpp src/config.cpp src/ini.cpp src/desktop.cpp \
           src/auto_dispatch.cpp src/switches.cpp src/util.cpp
HDRS    := $(wildcard src/*.h)

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti
LDFLAGS  := -static -s -mwindows
LIBS     := -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -luser32

BUILD    := build
DIST     := dist
TESTDIST := dist-test

.PHONY: all clean dist test console

all: $(DIST)/vdlaunch64.exe $(DIST)/vdlaunch32.exe $(DIST)/vdlaunch.exe

# console-subsystem twins used by the test harness so stdout is capturable
console: $(TESTDIST)/vdlaunch64c.exe $(TESTDIST)/vdlaunch32c.exe

$(BUILD) $(DIST) $(TESTDIST):
	@mkdir -p $@

$(BUILD)/%.64.o: src/%.cpp $(HDRS) | $(BUILD)
	$(CXX64) $(CXXFLAGS) -DVDLAUNCH_AUTO=0 -c $< -o $@

$(BUILD)/%.32.o: src/%.cpp $(HDRS) | $(BUILD)
	$(CXX32) $(CXXFLAGS) -DVDLAUNCH_AUTO=0 -c $< -o $@

$(BUILD)/%.auto.o: src/%.cpp $(HDRS) | $(BUILD)
	$(CXX64) $(CXXFLAGS) -DVDLAUNCH_AUTO=1 -c $< -o $@

OBJ64  := $(patsubst src/%.cpp,$(BUILD)/%.64.o,$(SRCS))
OBJ32  := $(patsubst src/%.cpp,$(BUILD)/%.32.o,$(SRCS))
OBJAUTO:= $(patsubst src/%.cpp,$(BUILD)/%.auto.o,$(SRCS))

$(DIST)/vdlaunch64.exe: $(OBJ64) | $(DIST)
	$(CXX64) $(LDFLAGS) $^ -o $@ $(LIBS)

$(DIST)/vdlaunch32.exe: $(OBJ32) | $(DIST)
	$(CXX32) $(LDFLAGS) $^ -o $@ $(LIBS)

$(DIST)/vdlaunch.exe: $(OBJAUTO) | $(DIST)
	$(CXX64) $(LDFLAGS) $^ -o $@ $(LIBS)

$(TESTDIST)/vdlaunch64c.exe: $(OBJ64) | $(TESTDIST)
	$(CXX64) -static -s -mconsole $^ -o $@ $(LIBS)

$(TESTDIST)/vdlaunch32c.exe: $(OBJ32) | $(TESTDIST)
	$(CXX32) -static -s -mconsole $^ -o $@ $(LIBS)

clean:
	rm -rf $(BUILD) $(DIST) $(TESTDIST)

test: all console
	./test/run.sh
