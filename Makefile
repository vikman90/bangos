CC = gcc
LD = ld
OBJCOPY = objcopy

CFLAGS = -Iinclude -Ikernel -I/usr/include/efi -I/usr/include/efi/x86_64 \
         -DEFI_FUNCTION_WRAPPER -fno-stack-protector -fpic -fvisibility=hidden -fshort-wchar \
         -mno-red-zone -fno-builtin -fno-tree-loop-distribute-patterns -Wall -Wextra -O2 -g

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

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    OVMF_PATH ?= /opt/homebrew/share/qemu/edk2-x86_64-code.fd
else
    OVMF_PATH ?= /usr/share/ovmf/OVMF.fd
endif

DOCKER_IMAGE ?= bangos-builder

.PHONY: all userland esp run-qemu test clean docker-image docker-build docker-test docker-shell docs-build docs-serve

DISK_IMG = $(BUILD_DIR)/disk.img
DISK_ROOT = userland/disk_root

$(DISK_IMG): $(shell find $(DISK_ROOT) -type f 2>/dev/null) | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1M count=32 status=none
	/usr/sbin/mke2fs -q -F -t ext2 -b 1024 -d $(DISK_ROOT) $@ 32M

all: userland esp $(DISK_IMG)

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

run-qemu: esp $(DISK_IMG)
	qemu-system-x86_64 -m 512M -bios $(OVMF_PATH) -drive file=fat:rw:$(ESP_DIR),format=raw -drive file=$(DISK_IMG),format=raw,index=1,media=disk -serial stdio -nographic -net none -monitor none

TEST_TIMEOUT ?= 180

test: esp $(DISK_IMG)
	chmod +x scripts/run_qemu_test.sh && OVMF_PATH="$(OVMF_PATH)" TEST_TIMEOUT="$(TEST_TIMEOUT)" ./scripts/run_qemu_test.sh

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C userland clean

docker-image:
	docker build --platform linux/amd64 -t $(DOCKER_IMAGE) .

docker-build: docker-image
	docker run --rm --platform linux/amd64 -v "$$(pwd):/bangos" $(DOCKER_IMAGE) make clean all

docker-test: docker-image
	docker run --rm --platform linux/amd64 -v "$$(pwd):/bangos" $(DOCKER_IMAGE) make test

docker-shell: docker-image
	docker run --rm -it --platform linux/amd64 -v "$$(pwd):/bangos" $(DOCKER_IMAGE) bash

docs-build:
	@if [ -d ".venv" ]; then .venv/bin/mkdocs build --strict; else mkdocs build --strict; fi

docs-serve:
	@if [ -d ".venv" ]; then .venv/bin/mkdocs serve -a 0.0.0.0:8000; else mkdocs serve -a 0.0.0.0:8000; fi
