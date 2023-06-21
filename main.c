
#include <stdio.h>
#include "load_elf.h"

int main() {

	{
		const char* path = "/lib/x86_64-linux-gnu/libm.so.6";
		void* base = load_elf(path);

		double (*pow)(double, double) = get_symbol_by_name(base, "pow");
		double a = 3.14159;
		double b = a;
		printf("%g ** %g == %g\n", a, b, pow(a, b));
	}
	/**/

	const char* path = "/lib/x86_64-linux-gnu/libc++.so.1";
	void* base = load_elf(path);
	void* std_cout = get_symbol_by_name(base, "_ZNSt3__14coutE");
	// offset may be different
	// std::ostream::operator<<(int)
	void* (*print_int)(void*, int) = get_symbol_by_offset(base, 0x5e380);
	// std::ostream::put(char)
	void* (*print_char)(void*, char) = get_symbol_by_offset(base, 0x5f510);
	print_char(print_int(std_cout, 114514), '\n');
	/**/

	puts("done.");
	return 0;
}
