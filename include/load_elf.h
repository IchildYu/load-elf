#ifndef __LOAD_ELF_H__
#define __LOAD_ELF_H__

void* load_elf(const char* elf_path);
void* get_symbol_by_name(void* base, const char* symbol);
void* get_symbol_by_offset(void* base, size_t offset);

extern int (*init_array_filter)(void* base, void (*init_array_item)());

#endif
