// arm-linux-gnueabi-gcc ./main.c -o main -g -ldl -Wno-format
// qemu-arm -L /usr/arm-linux-gnueabi/ -B 0x400000 ./main

// arm-linux-gnueabi-gcc ./main.c -o main -g -ldl -Wno-format && qemu-arm -L /usr/arm-linux-gnueabi/ -B 0x400000 ./main

// qemu-arm -g 12345 -L /usr/arm-linux-gnueabi/ ./main
// target remote 127.0.0.1:12345
// #include <stdio.h>
int getchar();
#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include "logger.h"
#include "elf_struct.h"
#include "load_elf.h"

int (*init_array_filter)(void* base, void (*init_array_item)());

extern int do_reloc(void* base, size_t offset, size_t info, size_t addend, const elf_sym* symtab, const char* strtab) __attribute__((weak));

void* load_with_mmap(const char* path);

typedef struct SymbolList {
	struct SymbolList* next;
	const char* symbol;
	void* addr;
} SymbolList;

typedef struct LibraryList {
	struct LibraryList* next;
	const char* libname;
	void* base;
} LibraryList;

static SymbolList symbol_header = { NULL, "", NULL };

static LibraryList library_header = { NULL, "", NULL };

void register_global_symbol(const char* symbol, void* target) {
	LOGD("register symbol `%s' at %p.\n", symbol, target);
	SymbolList* next = symbol_header.next;
	SymbolList* last = &symbol_header;
	while (next) {
		int r = strcmp(symbol, next->symbol);
		if (r > 0) {
			last = next;
			next = next->next;
		} else if (r == 0) {
			LOGW("registered symbol `%s' (%p) replaced with %p.\n", symbol, next->addr, target);
			next->addr = target;
			return;
		} else {
			break;
		}
	}
	// next == NULL || strcmp(symbol, next->symbol) < 0
	SymbolList* s = (SymbolList*) malloc(sizeof(SymbolList));
	s->symbol = symbol;
	s->addr = target;
	s->next = next;
	last->next = s;
}

void* find_registered_symbol(const char* symbol) {
	SymbolList* iter = symbol_header.next;
	while (iter) {
		int r = strcmp(symbol, iter->symbol);
		if (r == 0) {
			return iter->addr;
		} else if (r < 0) {
			return NULL;
		} else {
			iter = iter->next;
		}
	}
	return NULL;
}

void* get_global_symbol(const char* symbol) {
	void* addr = find_registered_symbol(symbol);
	if (addr) {
		return addr;
	}
	addr = dlsym((void*) -1, symbol);
	if (addr) {
		return addr;
	}
	LibraryList* iter = library_header.next;
	while (iter) {
		addr = get_symbol_by_name(iter->base, symbol);
		if (addr) {
			return addr;
		}
		iter = iter->next;
	}
	return NULL;
}

void load_needed_library(const char* libname) {
	LOGD("loading needed library `%s'.\n", libname);
	void* handle = dlopen(libname, RTLD_NOW | RTLD_GLOBAL);
	if (handle == NULL) {
		LOGW("failed to load needed library `%s': %s.\n", libname, dlerror());
	}
}

void load_global_library(const char* libname) {
	LOGD("loading global library `%s'.\n", libname);
	void* handle = dlopen(libname, RTLD_NOW | RTLD_GLOBAL);
	if (handle) {
		return;
	}
	LOGW("dlopen failed to load global library `%s': %s.\n", libname, dlerror());

	void* base = load_with_mmap(libname);
	if (base) {
		LibraryList* lib = (LibraryList*) malloc(sizeof(LibraryList));
		lib->next = library_header.next;
		library_header.next = lib;
		lib->libname = libname;
		lib->base = base;
	}
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
		load_needed_library(strtab + it->d_un);
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
		if (!init_array_filter) {
			do {
				LOGI("Execute init proc? [(y)es/(n)o] ");
				choice = getchar();
				if (choice != '\n') while (getchar() != '\n') ;
				if (choice >= 'A' && choice <= 'Z') choice += 0x20;
			} while (choice != 'y' && choice != 'n');
		} else if (init_array_filter(base, init)) {
			choice = 'y';
		} else {
			choice = 'n';
		}
		if (choice == 'y') {
			LOGI("\texecuting init at %p...\n", init);
			init();
		} else {
			LOGI("\t skipping init at %p...\n", init);
		}
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
				while (!init_array_filter && choice != 'y' && choice != 'n' && choice != 'a' && choice != 'o') {
					LOGI("\texecute function %p? [(y)es/(n)o/(a)ll items left/n(o)ne items left] ", init_array[i]);
					choice = getchar();
					if (choice != '\n') while (getchar() != '\n') ; // skip line
					if (choice >= 'A' && choice <= 'Z') choice += 0x20; // convert to lower case
				}
				if (init_array_filter) {
					if (init_array_filter(base, init_array[i])) {
						LOGI("\texecuting function at %p...\n", init_array[i]);
						init_array[i]();
					} else {
						LOGI("\t skipping function at %p...\n", init_array[i]);
					}
				} else if ((uchar) (choice - 'n') > 2) { // 'y' or 'a'
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
	if (fd < 0) {
		LOGE("file `%s' not found.\n", path);
		return NULL;
	}

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
			memset((void*) ((size_t) base + pheader.p_vaddr + pheader.p_filesz), 0, pheader.p_memsz - pheader.p_filesz);
		}
		{
			LOGV("testing memory...\n");
			char c = *(unsigned char*) (pheader.p_vaddr + (size_t) base);
			c = *(unsigned char*) (pheader.p_vaddr + (size_t) base + pheader.p_filesz - 1);
			c = *(unsigned char*) (pheader.p_vaddr + (size_t) base + pheader.p_memsz - 1);
			c++;
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
	return NULL;
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
			// LOGE("failed to resolve symbol `%s' from library (%p): not found.\n", symbol, base);
			return NULL;
		}
		if (strcmp(strtab + symtab->st_name, symbol) == 0) {
			if (symtab->st_value == 0) {
				// LOGE("failed to resolve symbol `%s' from library (%p): value is NULL.\n", symbol, base);
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
