CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=c99 -Isrc -IC:/msys64/ucrt64/include/SDL2 -DSDL_MAIN_HANDLED $(EXTRA)
LDFLAGS := -LC:/msys64/ucrt64/lib -mconsole
LIBS := -lSDL2_image -lSDL2 -lcomdlg32 -lm

SRCS := $(wildcard src/core/*.c src/graphics/*.c src/renderer3d/*.c src/scene/*.c src/assets/*.c src/sys/*.c src/editor/*.c) src/main.c
OBJS := $(SRCS:.c=.o)
TARGET := ps13d.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

q88: EXTRA += -DPS1_FIXED_SHIFT=8
q88: all

q1616: EXTRA += -DPS1_FIXED_SHIFT=16
q1616: all

verify-no-float:
	grep -rEn '\b(float|double)\b' src --include=*.c --include=*.h | grep -Ev '/sys/|ps1_lut.c|ps1_timing.c|ps1_fixed.h' || echo "no float tokens outside PC/sys layer"

selftest: all
	./$(TARGET) --selftest 120 --png selftest.png

run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean q88 q1616 verify-no-float selftest run