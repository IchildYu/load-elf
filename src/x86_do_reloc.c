#include <stddef.h>
#include <dlfcn.h>
#include <string.h>
#include "elf_struct.h"
#include "logger.h"
#include "load_elf.h"

#define R_COPY 5
#define R_GLOB_DAT 6
#define R_JUMP_SLOT 7
#define R_RELATIVE 8
#define R_IRELATIVE 42

int do_reloc(void* base, size_t offset, size_t info, size_t addend, const elf_sym* symtab, const char* strtab) {
	#define sym (elf_r_sym(info))
	#define type (elf_r_type(info))
	#define value (symtab[sym].st_value)
	#define size (symtab[sym].st_size)
	#define name (strtab + symtab[sym].st_name)
	switch (type) {
	case R_NONE:
		LOGV("R_NONE.\n");
		break;
	case R_COPY:
		if (value) {
			LOGV("R_COPY: from +0x%lx to +0x%lx size 0x%lx.\n", value, offset, size);
			if (offset != value) {
				memcpy((void*) ((size_t) base + offset), (const void*) ((size_t) base + value), size);
			} else {
				LOGE("Unspecified R_COPY at +0x%lx size 0x%lx.\n", offset, size);
				goto R_COPY_name;
			}
		} else {
			R_COPY_name:
			LOGV("R_COPY: from `%s' to +0x%lx size 0x%lx.\n", name, offset, size);
			const void* sym_value = get_global_symbol(name);
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
			LOGV("R_GLOB_DAT/R_JUMP_SLOT: set +0x%lx at +0x%lx.\n", value, offset);
			*(size_t*) ((size_t) base + offset) = (size_t) base + value;
		} else {
			LOGV("R_GLOB_DAT/R_JUMP_SLOT: set `%s' at +0x%lx.\n", name, offset);
			const void* sym_value = get_global_symbol(name);
			if (!sym_value) {
				LOGW("failed to resolve symbol `%s'.\n", name);
				break;
			}
			*(size_t*) ((size_t) base + offset) = (size_t) sym_value;
		}
		break;
	case R_RELATIVE:
		LOGV("R_RELATIVE: set +0x%lx at +0x%lx.\n", addend, offset);
		*(size_t*) ((size_t) base + offset) = (size_t) base + addend;
		break;
	case R_IRELATIVE:
		LOGV("R_IRELATIVE: set (+0x%lx)() at +0x%lx.\n", addend, offset);
		*(size_t*) ((size_t) base + offset) = ((size_t (*)()) ((size_t) base + addend))();
		break;
	case 2: // R_386_PC32
		LOGV("R_386_PC32: set 0x%lx-0x%lx+0x%lx at +0x%lx.\n", value, offset, addend, offset);
		*(size_t*) ((size_t) base + offset) = value - offset + addend;
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