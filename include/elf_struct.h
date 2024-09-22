#ifndef __ELF_STRUCT_H__
#define __ELF_STRUCT_H__

#ifdef __64__
	#undef __64__
#endif
#ifdef __32__
	#undef __32__
#endif

#if defined(ARM) || defined(X86)
	#define __32__
#elif defined(ARM64) || defined(AARCH64) || defined(X64)
	#define __64__
#else
	#error "invalid arch"
#endif

#define R_NONE 0

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

typedef struct {
	uint s_name;
	uint s_type;
	size_t s_flags;
	size_t s_addr;
	size_t s_offset;
	size_t s_size;
	uint s_link;
	uint s_info;
	size_t s_addralign;
	size_t s_entsize;
} elf_section_header;

// elf_sym.st_info
#define elf_st_bind(info) ((info) >> 4)
#define elf_st_type(info) ((info) & 0xf)

#ifndef __32__ // 64 bit
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

#else // 32 bit
typedef struct {
	uint p_type;
	size_t p_offset;
	size_t p_vaddr;
	size_t p_paddr;
	size_t p_filesz;
	size_t p_memsz;
	uint p_flags;
	size_t p_align;
} elf_program_header;

typedef struct {
	uint st_name;
	size_t st_value;
	size_t st_size;
	uchar st_info;
	uchar st_other;
	ushort shndx;
} elf_sym;

// elf_rel[a].r_info
#define elf_r_sym(info) ((info) >> 8)
#define elf_r_type(info) ((uchar) (info))

#endif

#endif