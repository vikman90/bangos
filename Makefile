CC = gcc
LD = ld
OBJCOPY = objcopy

CFLAGS = -Iinclude -Ikernel -I/usr/include/efi -I/usr/include/efi/x86_64 \
         -DEFI_FUNCTION_WRAPPER -fno-stack-protector -fpic -fshort-wchar \
         -mno-red-zone -Wall -Wextra -O2 -g

LDFLAGS = -shared -Bsymbolic -L/usr/lib /usr/lib/crt0-efi-x86_64.o \
          -T /usr/lib/elf_x86_64_efi.lds -lgnuefi -lefi

BUILD_DIR = build
ESP_DIR = $(BUILD_DIR)/esp

BOOT_SRC = boot/main.c
C_SRCS = $(shell find kernel -name "*.c" 2>/dev/null)
ASM_SRCS = $(shell find kernel -name "*.s" 2>/dev/null)

BOOT_OBJ = $(BUILD_DIR)/boot.o
C_OBJS = $(patsubst kernel/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst kernel/%.s, $(BUILD_DIR)/%.o, $(ASM_SRCS))
KERNEL_OBJS = $(C_OBJS) $(ASM_OBJS)

EFI_SO = $(BUILD_DIR)/bangos.so
EFI_TARGET = $(ESP_DIR)/EFI/BOOT/BOOTX64.EFI

.PHONY: all userland esp run-qemu test clean

all: userland esp

userland:
	$(MAKE) -C userland

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ESP_DIR):
	mkdir -p $(ESP_DIR)/EFI/BOOT

$(BOOT_OBJ): $(BOOT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: kernel/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: kernel/%.s | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

$(EFI_SO): $(BOOT_OBJ) $(KERNEL_OBJS)
	$(LD) -shared -Bsymbolic -L/usr/lib /usr/lib/crt0-efi-x86_64.o \
		$(BOOT_OBJ) $(KERNEL_OBJS) \
		-o $@ -T /usr/lib/elf_x86_64_efi.lds -lgnuefi -lefi

$(EFI_TARGET): $(EFI_SO) | $(ESP_DIR)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $< $@

esp: userland $(EFI_TARGET)
	cp userland/initrd.tar $(ESP_DIR)/initrd.tar

run-qemu: esp
	qemu-system-x86_64 -m 512M -bios /usr/share/ovmf/OVMF.fd -drive file=fat:rw:$(ESP_DIR),format=raw -serial stdio -nographic -net none -monitor none

test: esp
	chmod +x scripts/run_qemu_test.sh && ./scripts/run_qemu_test.sh

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C userland clean
