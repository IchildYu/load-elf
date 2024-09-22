
CFLAGS = -g -ldl -I./include -Wall --pie
SRC = ./src/logger.c ./src/load_elf.c ./src/breakpoint.c

# uncomment this two lines to use go_compat (x64 only)
# SRC += ./plugins/go_compat.c
# CFLAGS += -masm=intel

all: all_warning x64

all_warning:
	@echo "arch not specified, default x64"

x64:
	gcc ${SRC} ./src/x64_do_reloc.c ./main.c -o main -D X64 ${CFLAGS}

x86:
	gcc -m32 ${SRC} ./src/x86_do_reloc.c ./main.c -o main -D X86 ${CFLAGS}

arm64:
	aarch64-linux-gnu-gcc ${SRC} ./src/arm64_do_reloc.c ./main.c -o main -D ARM64 ${CFLAGS}

aarch64: arm64

arm:
	arm-linux-gnueabi-gcc ${SRC} ./src/arm_do_reloc.c ./main.c -o main -D ARM ${CFLAGS}

