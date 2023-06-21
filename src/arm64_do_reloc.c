#include <stddef.h>
#include <dlfcn.h>
#include <string.h>
#include "elf_struct.h"
#include "logger.h"

#define R_COPY 1024
#define R_GLOB_DAT 1025
#define R_JUMP_SLOT 1026
#define R_RELATIVE 1027
#define R_IRELATIVE 1032

int do_reloc(void* base, size_t offset, size_t info, size_t addend, const elf_sym* symtab, const char* strtab) {
	#define sym (elf_r_sym(info))
	#define type (elf_r_type(info))
	#define value (symtab[sym].st_value)
	#define size (symtab[sym].st_size)
	#define name (strtab + symtab[sym].st_name)
	switch (type) {
	case R_NONE:
		break;
	case R_COPY:
		if (value) {
			memcpy((void*) ((size_t) base + offset), (const void*) ((size_t) base + value), size);
		} else {
			const void* sym_value = dlsym((void*) -1, name); // RTLD_DEFAULT
			if (!sym_value) {
				LOGW("failed to resolve symbol `%s'.\n", name);
				break;
			}
			memcpy((void*) ((size_t) base + offset), sym_value, size);
		}
		break;
	case R_GLOB_DAT:
	case R_JUMP_SLOT:
		if (value) {
			*(size_t*) ((size_t) base + offset) = (size_t) base + value;
		} else {
			const void* sym_value = dlsym((void*) -1, name); // RTLD_DEFAULT
			if (!sym_value) {
				LOGW("failed to resolve symbol `%s'.\n", name);
				break;
			}
			*(size_t*) ((size_t) base + offset) = (size_t) sym_value;
		}
		break;
	case R_RELATIVE:
		*(size_t*) ((size_t) base + offset) = (size_t) base + addend;
		break;
	case R_IRELATIVE:
		*(size_t*) ((size_t) base + offset) = ((size_t (*)()) ((size_t) base + addend))();
		break;
	case 0x101: // R_AARCH64_ABS64
		if (value) {
			*(size_t*) ((size_t) base + offset) = (size_t) base + value + addend;
		} else {
			const void* sym_value = dlsym((void*) -1, name); // RTLD_DEFAULT
			if (!sym_value) {
				LOGW("failed to resolve symbol `%s'.\n", name);
				break;
			}
			*(size_t*) ((size_t) base + offset) = (size_t) sym_value + addend;
		}
		break;
	default:
		LOGW("unimplemented reloc type: %d.\n", type);
		break;
	}
	#undef sym
	#undef type
	#undef value
	#undef size
	#undef name
	return 1;
}