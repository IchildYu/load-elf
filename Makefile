
CFLAGS = -g -ldl -I./include
SRC = ./src/logger.c ./src/load_elf.c

all: x64
	@echo "arch not specified, default x64"

x64:
	gcc ${SRC} ./src/x64_do_reloc.c ./main.c -o main ${CFLAGS}

x86:
	gcc -m32 ${SRC} ./src/x86_do_reloc.c ./main.c -o main -D __32__ ${CFLAGS}

arm64:
	aarch64-linux-gnu-gcc ${SRC} ./src/arm64_do_reloc.c ./main.c -o main ${CFLAGS}

arm:
	arm-linux-gnueabi-gcc ${SRC} ./src/arm_do_reloc.c ./main.c -o main -D __32__ ${CFLAGS}

