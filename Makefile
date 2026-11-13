# xp-trim build
#
# Run from an MSYS2 MINGW32 shell (or with /mingw32/bin first on PATH):
#   make          build all shipped binaries into build/
#   make test     build and run unit tests
#   make check    verify imports and PE header of shipped binaries (XP 5.01)
#   make clean

CC      := gcc
OBJDUMP := objdump

TARGET_TRIPLE := i686-w64-mingw32
ifneq ($(shell $(CC) -dumpmachine),$(TARGET_TRIPLE))
$(error $(CC) targets "$(shell $(CC) -dumpmachine)", expected $(TARGET_TRIPLE). Use the MSYS2 MINGW32 environment)
endif

BUILD := build
OBJ   := $(BUILD)/obj

# printf: mingw's own C99 implementation (statically linked), so %lld/%zu work
# on XP's msvcrt.dll. See CLAUDE.md.
CPPFLAGS := -Isrc \
            -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 \
            -DUNICODE -D_UNICODE \
            -D__USE_MINGW_ANSI_STDIO=1
CFLAGS   := -std=c99 -O2 -Wall -Wextra -Werror -MMD -MP
LDFLAGS  := -static -static-libgcc \
            -Wl,--major-subsystem-version,5,--minor-subsystem-version,1 \
            -Wl,--major-os-version,5,--minor-os-version,1

CORE_SRC   := src/core/result.c \
              src/core/geometry.c \
              src/core/ranges.c \
              src/core/dsm.c \
              src/core/identify.c \
              src/core/volcheck.c \
              src/core/ata.c
WIN32_SRC  := src/win32/volume.c \
              src/win32/passthru.c
XPTRIM_SRC := src/cli/main.c \
              $(CORE_SRC) \
              $(WIN32_SRC)
TEST_SRC   := tests/test_main.c \
              tests/test_geometry.c \
              tests/test_ranges.c \
              tests/test_dsm.c \
              tests/test_identify.c \
              tests/test_volcheck.c \
              tests/test_ata.c \
              $(CORE_SRC) \
              $(WIN32_SRC)

XPTRIM_OBJ := $(XPTRIM_SRC:%.c=$(OBJ)/%.o)
TEST_OBJ   := $(TEST_SRC:%.c=$(OBJ)/%.o)

XPTRIM_EXE := $(BUILD)/xptrim.exe
TEST_EXE   := $(BUILD)/tests.exe

# Binaries that ship to users; `make check` inspects these.
SHIPPED := $(XPTRIM_EXE)

.PHONY: all test check clean

all: $(SHIPPED)

$(XPTRIM_EXE): $(XPTRIM_OBJ)
	$(CC) -municode $(LDFLAGS) -o $@ $^

$(TEST_EXE): $(TEST_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

test: $(TEST_EXE)
	./$(TEST_EXE)

check: $(SHIPPED)
	sh tools/check-imports.sh tools/allowed-imports.txt $(SHIPPED)

clean:
	rm -rf $(BUILD)

-include $(XPTRIM_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
