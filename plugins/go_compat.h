
#ifndef __GO_COMPAT_H__
#define __GO_COMPAT_H__

void __attribute((noreturn)) go_compat_entry(void* entry, void* main_ptr_in_elf, void* main_main);

void call_go_func(void* func, void* out, size_t out_count, ...); // assume out_count <= 7 && in_count <= 7

#endif