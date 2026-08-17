CC = musl-gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -static -lm

BIN_DIR = bin
TARGETS = $(BIN_DIR)/init $(BIN_DIR)/calc $(BIN_DIR)/sysinfo $(BIN_DIR)/bench $(BIN_DIR)/tasks $(BIN_DIR)/threads \
          $(BIN_DIR)/test_syscall_safety $(BIN_DIR)/test_vmm_demand $(BIN_DIR)/test_process_lifecycle
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

$(BIN_DIR)/tasks: src/tasks.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

$(BIN_DIR)/threads: src/threads.c src/tui.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< src/tui.o -o $@ $(LDFLAGS)

$(BIN_DIR)/test_syscall_safety: src/test_syscall_safety.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/test_vmm_demand: src/test_vmm_demand.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/test_process_lifecycle: src/test_process_lifecycle.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(INITRD): $(TARGETS)
	tar -cf $(INITRD) --format=ustar -C $(BIN_DIR) init calc sysinfo bench tasks threads test_syscall_safety test_vmm_demand test_process_lifecycle

clean:
	rm -rf src/*.o $(BIN_DIR) $(INITRD) init
