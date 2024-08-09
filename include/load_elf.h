#ifndef __LOAD_ELF_H__
#define __LOAD_ELF_H__

void* load_elf(const char* elf_path);
void* get_symbol_by_name(void* base, const char* symbol);
void* get_symbol_by_offset(void* base, size_t offset);
void register_global_symbol(const char* symbol, void* target); // register symbols before load_elf
void load_global_library(const char* libname); // dlopen or load_elf
void* get_global_symbol(const char* symbol); // register_global_symbol or dlsym or get_symbol_by_name(loaded_global_library, symbol)

extern int (*init_array_filter)(void* base, void (*init_array_item)());

#endif
