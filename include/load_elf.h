#ifndef __LOAD_ELF_H__
#define __LOAD_ELF_H__

void* load_elf(const char* elf_path);
void* get_symbol_by_name(void* base, const char* symbol);
void* get_symbol_by_offset(void* base, size_t offset);

#endif