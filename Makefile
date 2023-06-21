
CFLAGS = -g -ldl -I./include
SRC = ./src/logger.c ./src/load_elf.c

all: x64
	@echo "arch not specified, default x64"

x64:
	gcc ${CFLAGS} ${SRC} ./src/x64_do_reloc.c ./main.c -o main

x86:
	gcc -m32 ${CFLAGS} ${SRC} ./src/x86_do_reloc.c ./main.c -o main -D __32__

arm64:
	aarch64-linux-gnu-gcc ${CFLAGS} ${SRC} ./src/arm64_do_reloc.c ./main.c -o main

arm:
	arm-linux-gnueabi-gcc ${CFLAGS} ${SRC} ./src/arm_do_reloc.c ./main.c -o main -D __32__

