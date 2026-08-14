CC = musl-gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -static -lm

BIN_DIR = bin
TARGETS = $(BIN_DIR)/init $(BIN_DIR)/calc $(BIN_DIR)/sysinfo $(BIN_DIR)/bench
INITRD = initrd.tar

.PHONY: all clean

all: $(INITRD)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

src/tui.o: src/tui.c include/tui.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/init: src/init.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

$(BIN_DIR)/calc: src/calc.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

$(BIN_DIR)/sysinfo: src/sysinfo.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

$(BIN_DIR)/bench: src/bench.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

$(INITRD): $(TARGETS)
	tar -cf $(INITRD) --format=ustar -C $(BIN_DIR) init calc sysinfo bench

clean:
	rm -rf src/*.o $(BIN_DIR) $(INITRD) init
