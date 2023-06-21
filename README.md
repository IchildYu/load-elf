# load-elf

ELF loader for x86-64/x86/aarch64/arm.

## Why load-elf?

For binary analysis, it is common that we can't run this binary (malware) or it's difficult to debug (android native library). With elf-loader, these binaries can be viewed as simple external libraries and we can do whatever we want.

## How to use

For x86-64, `x64_main.c` is enough. Just write your code in function main at end of the file.

For other arches, a simple makefile is provided. Write code in `main.c` and `make x86/arm/arm64`.

*Note*
- Host is assumed to be x86-64 (or amd64)
- Some relocation types are not implemented
- It only loads the elf itself into memory, not the same process as ld.so does

## Examples

### x64

Compile `x64_main.c` and run:

```
# gcc ./x64_main.c -o main -g -ldl
# ./main
[I] loading /lib/x86_64-linux-gnu/libm.so.6 with mmap...
[I] done, loaded at 0xc0000000
[I] DYNAMIC detected, loading...
[W] failed to resolve symbol `_ITM_deregisterTMCloneTable'.
[W] unimplemented reloc type: 18.
[W] failed to resolve symbol `__gmon_start__'.
[W] failed to resolve symbol `_ITM_registerTMCloneTable'.
[I] init proc detected: 0xc000e000.
[I] Execute init proc? [(y)es/(n)o] y
[I] init array detected:
[I]     execute function 0xc000e460? [(y)es/(n)o/(a)ll items left/n(o)ne items left] a
[I]     executing function at 0xc000e460...
[I] fini proc detected: 0xc00898e8.
[I] fini array detected:
[I]     0xc000e420
[I] load_dynamic done.
3.14159 ** 3.14159 == 36.462
[I] loading /lib/x86_64-linux-gnu/libc++.so.1 with mmap...
[I] done, loaded at 0xc1000000
[I] DYNAMIC detected, loading...
[W] failed to resolve symbol `_ITM_registerTMCloneTable'.
[W] failed to resolve symbol `_ITM_deregisterTMCloneTable'.
[W] failed to resolve symbol `__gmon_start__'.
[I] init proc detected: 0xc1039660.
[I] Execute init proc? [(y)es/(n)o] y
[I] init array detected:
[I]     execute function 0xc103a210? [(y)es/(n)o/(a)ll items left/n(o)ne items left] a
[I]     executing function at 0xc103a210...
[I]     executing function at 0xc103a820...
[I]     executing function at 0xc103a240...
[I] fini proc detected: 0xc10a6d80.
[I] fini array detected:
[I]     0xc103a7e0
[I] load_dynamic done.
114514
done.
```

### other arches

Cross-compile and run with qemu.

SCTF 2023 SycLock level1:

![image](https://github.com/IchildYu/load-elf/assets/54837947/118b4298-932f-40fe-93be-0fdcec687ac0)

![image](https://github.com/IchildYu/load-elf/assets/54837947/3bb321ca-2e4c-46c6-a801-fb00905b5f22)

Use load_elf to recover huffman table, edit main.c:

```cpp
#include <stdio.h>
#include "load_elf.h"

int main() {
	const char* path = "./liblevel1.so";
	void* base = load_elf(path);
	void* (*buildTree)(void*) = get_symbol_by_offset(base, 0x8750);
	void* (*buildtable)(void*) = get_symbol_by_offset(base, 0x875c);
	void* tree = buildTree("reverseisfun");
	void* table = buildtable(tree);
	for (unsigned char** iter = table; *iter; iter = (unsigned char **)(*iter + 8)) {
		printf("%c: %s\n", **iter, ((const char**) *iter) [1]);
	}
	puts("done.");
	return 0;
}
```

Result:

```
# make arm
arm-linux-gnueabi-gcc -g -ldl -I./include ./src/logger.c ./src/load_elf.c ./src/arm_do_reloc.c ./main.c -o main -D __32__
# qemu-arm -B 0x400000 -L /usr/arm-linux-gnueabi/ ./main
[I] loading ./liblevel1.so with mmap...
[I] done, loaded at 0xc0000000
[I] DYNAMIC detected, loading...
[W] failed to load needed library `liblog.so': liblog.so: cannot open shared object file: No such file or directory.
[W] failed to load needed library `libc.so': /lib/libc.so: invalid ELF header.
[W] failed to resolve symbol `__sF'.
[W] failed to resolve symbol `__android_log_print'.
[W] failed to resolve symbol `__strchr_chk'.
[W] failed to resolve symbol `__assert2'.
[I] fini array detected:
[I]     0xc0008c20
[I]     0xc0008c0c
[I] load_dynamic done.
r: 00
f: 010
n: 0110
i: 0111
e: 10
v: 1100
u: 1101
s: 111
done.
```
