
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#define ERROR 0
#define WARNING 1
#define INFO 2
#define DEBUG 3
#define VERBOSE 4

const char* LOG_LEVEL_CHARS = "EWIDV";
const char* LOG_LEVEL_COLORS[] = {
	"\x1b[31m",
	"\x1b[33m",
	"\x1b[32m",
	"\x1b[0m",
	"\x1b[34m",
};
int _log_level = INFO;
int _log_color = 1;

void set_log_level(int log_level) {
	if (log_level < 0) log_level = 0;
	if (log_level > 4) log_level = 4;
	_log_level = log_level;
}

void set_log_color(int log_color) {
	_log_color = log_color;
}

void Log(int log_level, const char* format, ...) {
	if (log_level < 0) log_level = 0;
	if (log_level > 4) log_level = 4;
	if (log_level > _log_level) return;
	if (_log_color) printf("%s", LOG_LEVEL_COLORS[log_level]);
	printf("[%c] ", LOG_LEVEL_CHARS[log_level]);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	if (_log_color) printf("\x1b[0m");
}

#define LOGE(format, ...) Log(ERROR, format, ##__VA_ARGS__)
#define LOGW(format, ...) Log(WARNING, format, ##__VA_ARGS__)
#define LOGI(format, ...) Log(INFO, format, ##__VA_ARGS__)
#define LOGD(format, ...) Log(DEBUG, format, ##__VA_ARGS__)
#define LOGV(format, ...) Log(VERBOSE, format, ##__VA_ARGS__)

// default info
#define SET_LOGE() set_log_level(ERROR)
#define SET_LOGW() set_log_level(WARNING)
#define SET_LOGI() set_log_level(INFO)
#define SET_LOGD() set_log_level(DEBUG)
#define SET_LOGV() set_log_level(VERBOSE)

// default on
#define SET_LOGCOLOR_OFF() set_log_color(0)
#define SET_LOGCOLOR_ON() set_log_color(1)


#define R_NONE 0
#define R_COPY 5
#define R_GLOB_DAT 6
#define R_JUMP_SLOT 7
#define R_RELATIVE 8
#define R_IRELATIVE 37

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long long ullong;

typedef struct {
	uchar e_ident[16];
	ushort e_type;
	ushort e_machine;
	uint e_version;
	size_t e_entry;
	size_t e_phoff;
	size_t e_shoff;
	uint e_flags;
	ushort e_ehsize;
	ushort e_phentsize;
	ushort e_phnum;
	ushort e_shentsize;
	ushort e_shnum;
	ushort e_shtrndx;
} elf_header;

typedef struct {
	size_t d_tag;
	size_t d_un;
} elf_dyn;

typedef struct {
	size_t r_offset;
	size_t r_info;
} elf_rel;

typedef struct {
	size_t r_offset;
	size_t r_info;
	size_t r_addend;
} elf_rela;

// elf_sym.st_info
#define elf_st_bind(info) ((info) >> 4)
#define elf_st_type(info) ((info) & 0xf)

typedef struct {
	uint p_type;
	uint p_flags;
	size_t p_offset;
	size_t p_vaddr;
	size_t p_paddr;
	size_t p_filesz;
	size_t p_memsz;
	size_t p_align;
} elf_program_header;

typedef struct {
	uint st_name;
	uchar st_info;
	uchar st_other;
	ushort shndx;
	size_t st_value;
	size_t st_size;
} elf_sym;

// elf_rel[a].r_info
#define elf_r_sym(info) ((info) >> 32)
#define elf_r_type(info) ((uint) (info))

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
	case 1: // R_X86_64_64
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


#define SKIP_LOAD_WITH_DL

void* load_with_dl(const char* path) {
	#ifdef SKIP_LOAD_WITH_DL
		LOGD("SKIP_LOAD_WITH_DL defined, load_with_dl returns NULL.\n");
		return NULL;
	#endif
	LOGI("loading %s with dlopen...\n", path);
	void* handle = dlopen(path, RTLD_LAZY);
	if (handle == NULL) {
		LOGE("load_with_dl failed: %s.\n", dlerror());
		return NULL;
	}
	void* base = *(void**) handle;
	LOGI("done, loaded at %p.\n", base);
	return base;
}

int check_header(elf_header* header) {
	if (*(uint*) header->e_ident != 0x464c457f) {
		LOGE("elf magic header not detected.\n");
		return 0;
	}
	if (header->e_ident[4] != (sizeof(void*) / 4)) { // ei_class, 1: ELFCLASS32, 2: ELFCLASS64
		LOGE("elf class mismatch.\n");
		return 0;
	}
	if (header->e_ident[5] != 1) {
		LOGE("LSB expected.\n");
		return 0;
	}
	if (header->e_type != 2 && header->e_type != 3) {
		LOGE("Dynamic library or executable expected.\n");
		return 0;
	}
	if (header->e_ehsize != sizeof(elf_header)) {
		LOGE("Unexpected header size.\n");
		return 0;
	}
	return 1;
}

const elf_dyn* find_dyn_entry(const elf_dyn* dyn, int type) {
	for (; dyn->d_tag != 0; dyn++) { // DT_NULL
		if (dyn->d_tag == type) return dyn;
	}
	return NULL;
}

int do_rel(void* base, const elf_rel* rel, int count, const elf_sym* symtab, const char* strtab) {
	for (int i = 0; i < count; i++) {
		if (!do_reloc(base, rel[i].r_offset, rel[i].r_info, *(size_t*) ((size_t) base + rel[i].r_offset), symtab, strtab))
			return 0;
	}
	return 1;
}

int do_rela(void* base, const elf_rela* rela, int count, const elf_sym* symtab, const char* strtab) {
	for (int i = 0; i < count; i++) {
		if (!do_reloc(base, rela[i].r_offset, rela[i].r_info, rela[i].r_addend, symtab, strtab))
			return 0;
	}
	return 1;
}

int check_and_do_rel(void* base, const elf_dyn* dyn, const elf_rel* rel, const elf_sym* symtab, const char* strtab) {
	if (find_dyn_entry(dyn, 0x13)->d_un != sizeof(elf_rel)) { // DT_RELENT
		LOGE("unexpected rel table entry size.\n");
		return 0;
	}
	LOGD("do rel.\n");
	int rel_count = find_dyn_entry(dyn, 0x12)->d_un / sizeof(elf_rel); // DT_RELSZ
	if (!do_rel(base, rel, rel_count, symtab, strtab)) return 0;
	return 1;
}

int check_and_do_rela(void* base, const elf_dyn* dyn, const elf_rela* rela, const elf_sym* symtab, const char* strtab) {
	if (find_dyn_entry(dyn, 0x9)->d_un != sizeof(elf_rela)) { // DT_RELAENT
		LOGE("unexpected rela table entry size.\n");
		return 0;
	}
	LOGD("do rela.\n");
	int rela_count = find_dyn_entry(dyn, 0x8)->d_un / sizeof(elf_rela); // DT_RELASZ
	if (!do_rela(base, rela, rela_count, symtab, strtab)) return 0;
	return 1;
}

int load_dynamic(void* base, const elf_dyn* dyn) {
	const elf_dyn* res = find_dyn_entry(dyn, 5); // DT_STRTAB
	if (res == NULL) {
		LOGE("string table not found.\n");
		return 0;
	}
	const char* strtab = (const char*) ((size_t) base + res->d_un);

	const elf_sym* symtab = NULL;
	res = find_dyn_entry(dyn, 0x6); // DT_SYMTAB
	if (res != NULL) {
		symtab = (const elf_sym*) ((size_t) base + res->d_un);
		if (find_dyn_entry(dyn, 0xB)->d_un != sizeof(elf_sym)) { // DT_SYMENT
			LOGE("unexpected symbol table entry size.\n");
			return 0;
		}
	}

	for (const elf_dyn* it = dyn; it->d_tag != 0; it++) {
		if (it->d_tag != 1) continue; // DT_NEEDED: name of needed library
		LOGD("loading needed library `%s'.\n", strtab + it->d_un);
		if (!dlopen(strtab + it->d_un, RTLD_NOW | RTLD_GLOBAL))
			LOGW("failed to load needed library `%s': %s.\n", strtab + it->d_un, dlerror());
	}

	int rel_done = 0;
	for (const elf_dyn* it = dyn; it->d_tag != 0; it++) { // DT_NULL
		switch (it->d_tag) {
		case 7: // DT_RELA
			if (rel_done) break;
			if (!check_and_do_rela(base, dyn, (const elf_rela*) ((size_t) base + it->d_un), symtab, strtab))
				return 0;
			rel_done = 1;
			break;
		case 0x11: // DT_REL
			if (rel_done) break;
			if (!check_and_do_rel(base, dyn, (const elf_rel*) ((size_t) base + it->d_un), symtab, strtab))
				return 0;
			rel_done = 1;
			break;
		case 0x17: // DT_JMPREL
			;
			size_t plt_rel_size = find_dyn_entry(dyn, 0x2)->d_un; // DT_PLTRELSZ
			int plt_rel = find_dyn_entry(dyn, 0x14)->d_un; // DT_PLTREL
			if (plt_rel == 0x11) { // DT_REL
				if (!rel_done) {
					res = find_dyn_entry(dyn, 0x11); // DT_REL
					if (res != NULL) {
						if (!check_and_do_rel(base, dyn, (const elf_rel*) ((size_t) base + res->d_un), symtab, strtab))
							return 0;
						rel_done = 1;
					}
				}
				plt_rel_size /= sizeof(elf_rel);
				LOGD("do jmprel with rel.\n");
				if (!do_rel(base, (elf_rel*) ((size_t) base + it->d_un), plt_rel_size, symtab, strtab)) return 0;
			} else if (plt_rel == 7) { // DT_RELA
				if (!rel_done) {
					res = find_dyn_entry(dyn, 7); // DT_RELA
					if (res != NULL) {
						if (!check_and_do_rela(base, dyn, (const elf_rela*) ((size_t) base + res->d_un), symtab, strtab))
							return 0;
						rel_done = 1;
					}
				}
				plt_rel_size /= sizeof(elf_rela);
				LOGD("do jmprel with rela.\n");
				if (!do_rela(base, (elf_rela*) ((size_t) base + it->d_un), plt_rel_size, symtab, strtab)) return 0;
			} else {
				LOGE("unexpected plt rel type: %d.\n", plt_rel);
				return 0;
			}
			break;
		}
	}

	res = find_dyn_entry(dyn, 0xC); // DT_INIT
	if (res != NULL) {
		void (*init)() = (void (*)()) ((size_t) base + res->d_un);
		LOGI("init proc detected: %p.\n", init);
		int choice = 'y';
		do {
			LOGI("Execute init proc? [(y)es/(n)o] ");
			choice = getchar();
			if (choice != '\n') while (getchar() != '\n') ;
			if (choice >= 'A' && choice <= 'Z') choice += 0x20;
		} while (choice != 'y' && choice != 'n');
		if (choice == 'y') init();
	}

	res = find_dyn_entry(dyn, 0x19); // DT_INIT_ARRAY
	if (res != NULL) {
		void (**init_array)() = (void (**)()) ((size_t) base + res->d_un);
		int count = find_dyn_entry(dyn, 0x1B)->d_un / sizeof(size_t); // DT_INIT_ARRAYSZ
		while (*init_array == NULL && count) {
			init_array++;
			count--;
		}
		if (count) {
			LOGI("init array detected:\n");
			int choice = '?';
			for (int i = 0; i < count; i++) {
				if (!init_array[i]) continue;
				while (choice != 'y' && choice != 'n' && choice != 'a' && choice != 'o') {
					LOGI("\texecute function %p? [(y)es/(n)o/(a)ll items left/n(o)ne items left] ", init_array[i]);
					choice = getchar();
					if (choice != '\n') while (getchar() != '\n') ; // skip line
					if (choice >= 'A' && choice <= 'Z') choice += 0x20; // convert to lower case
				}
				if ((uchar) (choice - 'n') > 2) { // 'y' or 'a'
					LOGI("\texecuting function at %p...\n", init_array[i]);
					init_array[i]();
					if (choice == 'y') choice = '?';
				} else if (choice == 'n') choice = '?';
			}
		}
	}

	res = find_dyn_entry(dyn, 0xD); // DT_FINI
	if (res != NULL) {
		void (*fini)() = (void (*)()) ((size_t) base + res->d_un);
		LOGI("fini proc detected: %p.\n", fini);
	}

	res = find_dyn_entry(dyn, 0x1A); // DT_FINI_ARRAY
	if (res != NULL) {
		void (**fini_array)() = (void (**)()) ((size_t) base + res->d_un);
		int count = find_dyn_entry(dyn, 0x1C)->d_un / sizeof(size_t); // DT_FINI_ARRAYSZ
		while (*fini_array == NULL && count) {
			fini_array++;
			count--;
		}
		if (count) {
			LOGI("fini array detected:\n");
			for (int i = 0; i < count; i++) {
				if (fini_array[i]) {
					LOGI("\t%p\n", fini_array[i]);
				}
			}
		}
	}
	LOGI("load_dynamic done.\n");
	return 1;
}

#define MMAP_LOAD_BASE ((void*) 0xC0000000)
void* load_with_mmap(const char* path) {
	LOGI("loading %s with mmap...\n", path);
	int fd = open(path, O_RDONLY);
	LOGV("open(path, O_RDONLY) returns %d\n", fd);

	elf_header header;
	LOGV("reading elf header from file...\n");
	if (read(fd, &header, sizeof(header)) != sizeof(header)) {
		LOGE("read header error\n");
		close(fd);
		return NULL;
	}
	LOGV("checking elf header...\n");
	if (!check_header(&header)) {
		close(fd);
		return NULL;
	}

	elf_program_header pheader;
	elf_dyn* dyn = NULL;

	int e_phentsize = header.e_phentsize;
	int e_phnum = header.e_phnum;

	if (e_phentsize != sizeof(pheader)) {
		LOGE("unexpected program header size.\n");
		close(fd);
		return NULL;
	}

	LOGV("determine LOAD_BASE...\n");
	void* base = MMAP_LOAD_BASE;
	while (base != mmap(base, 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) {
		base = (void*) ((size_t) base + 0x1000000);
	}
	munmap(base, 0x1000);
	LOGD("trying loading at %p\n", base);

	lseek(fd, header.e_phoff, SEEK_SET);
	for (int i = 0; i < e_phnum; i++) {
		LOGV("processing phdr %d...\n", i);
		if (read(fd, &pheader, sizeof(pheader)) != sizeof(pheader)) {
			LOGE("read pheader error\n");
			close(fd);
			return NULL;
		}
		if (pheader.p_type != 1 || pheader.p_memsz == 0) { // not PT_LOAD or nothing to load
			if (pheader.p_type == 2) { // DYNAMIC
				if (dyn != NULL) {
					LOGE("duplicated DYNAMIC PHT detected.\n");
					close(fd);
					return NULL;
				} else {
					dyn = (elf_dyn*) ((size_t) base + pheader.p_vaddr);
				}
			}
			continue;
		}
		void* addr = (void*) (((size_t) base + pheader.p_vaddr) & ~0xfff);
		int offset = pheader.p_vaddr & 0xfff;
		size_t size = (offset + pheader.p_filesz + 0xfff) & ~0xfff;
		if (addr != mmap(addr, size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE, fd, pheader.p_offset - offset)) {
		// if (addr != mmap(addr, pheader.p_memsz + offset, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE, fd, pheader.p_offset - offset)) {
		// if ((uchar*) addr != (uchar*) base + pheader.p_vaddr) {
			LOGE("failed to mmap 0x%lx to 0x%lx.\n", pheader.p_offset, pheader.p_vaddr + (size_t) base);
			close(fd);
			return NULL;
		}
		if (offset) {
			memset(addr, 0, offset);
		}
		if (pheader.p_memsz != pheader.p_filesz) {
			if (pheader.p_memsz < pheader.p_filesz) {
				LOGE("unexpected: filesz bigger than memsz.\n");
				close(fd);
				return NULL;
			}
			if (pheader.p_memsz + offset > size) {
				LOGV("mmap extra pages in memory\n");
				addr = (void*) ((size_t) addr + size);
				if (addr != mmap(addr, pheader.p_memsz + offset - size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON | MAP_SHARED, -1, 0)) {
					LOGE("failed to mmap 0x%lx to 0x%lx.\n", pheader.p_offset, pheader.p_vaddr + (size_t) base);
					close(fd);
					return NULL;
				}
			}
		}

		{
			LOGV("testing memory...\n");
			char c = *(unsigned char*) (pheader.p_vaddr + (size_t) base);
			c = *(unsigned char*) (pheader.p_vaddr + (size_t) base + pheader.p_filesz - 1);
			c = *(unsigned char*) (pheader.p_vaddr + (size_t) base + pheader.p_memsz - 1);
		}
		LOGD("mmaped 0x%lx to 0x%lx, filesz 0x%lx, memsz 0x%lx\n", pheader.p_offset, pheader.p_vaddr + (size_t) base, pheader.p_filesz, pheader.p_memsz);
	}
	LOGI("done, loaded at %p\n", base);
	close(fd);

	if (!dyn) return base;

	LOGI("DYNAMIC detected, loading...\n");
	if (!load_dynamic(base, dyn)) return NULL;
	return base;
}

const elf_dyn* get_dyn(void* base) {
	elf_header* header = (elf_header*) base;
	int e_phnum = header->e_phnum;
	elf_program_header* pheader = (elf_program_header*) ((size_t) base + header->e_phoff);
	for (int i = 0; i < e_phnum; i++, pheader++) {
		if (pheader->p_type == 2) {
			return (elf_dyn*) ((size_t) base + pheader->p_vaddr);
		}
	}
}

void* get_symbol_by_name(void* base, const char* symbol) {
	const elf_dyn* dyn = get_dyn(base);
	const char* strtab = (const char*) (find_dyn_entry(dyn, 5)->d_un); // DT_STRTAB

	if (strtab < (const char*) base)
		strtab = (const char*) strtab + (size_t) base;
	size_t strsz = find_dyn_entry(dyn, 0xa)->d_un; // DT_STRSZ
	const elf_sym* symtab = (const elf_sym*) (find_dyn_entry(dyn, 6)->d_un); // DT_SYMTAB
	if ((const char*) symtab < (const char*) base)
		symtab = (const elf_sym*) ((const char*) symtab + (size_t) base);

	for (; ; symtab++) {
		if (symtab->st_name == 0) continue;
		if (symtab->st_name >= strsz) {
			LOGE("failed to resolve symbol `%s' from library (%p): not found.\n", symbol, base);
			return NULL;
		}
		if (strcmp(strtab + symtab->st_name, symbol) == 0) {
			if (symtab->st_value == 0) {
				LOGE("failed to resolve symbol `%s' from library (%p): value is NULL.\n", symbol, base);
				return NULL;
			}
			if (elf_st_type(symtab->st_info) != 10) { // STT_GNU_IFUNC
				return (void*) ((size_t) base + symtab->st_value);
			}
			return ((void* (*)()) ((size_t) base + symtab->st_value))();
		}
	}
}

void* get_symbol_by_offset(void* base, size_t offset) {
	return (void*) ((size_t) base + offset);
}

void* load_elf(const char* elf_path) {
	void* base = load_with_dl(elf_path);
	if (base == NULL) {
		base = load_with_mmap(elf_path);
	}
	assert(base != NULL && *(unsigned int*) base == 0x464c457f);
	return base;
}


// gcc ./x64_main.c -o main -g -ldl
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
