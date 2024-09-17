
#include <stdio.h>
#include "load_elf.h"
#include "logger.h"
#include "breakpoint.h"
#include "plugins/go_compat.h"
// remember to uncomment in Makefile

int filter() {
	return 1;
}

void* base;
void main_main() {
	printf("Enter main_main: %p;\n", main_main);
	call_go_func(get_symbol_by_offset(base, 0x48c460), NULL, 0);
	putchar('\n');
	printf("Exit  main_main: %p;\n", main_main);
}

int main() {
	// SET_LOGV();
	init_array_filter = (void*) filter;

	const char* path = "./plugins/go_linux.bak";
	base = load_elf(path);

	void* go_entry = get_symbol_by_offset(base, 0x45c600);
	void* ptr = get_symbol_by_offset(base, 0x4ac450);
	go_compat_entry(go_entry, ptr, main_main);

	puts("done.");
	return 0;
}
