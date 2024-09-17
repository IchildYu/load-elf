
#ifndef __GO_COMPAT_H__
#define __GO_COMPAT_H__

/*

go_compat.c

- go_compat_entry
void __attribute((noreturn)) go_compat_entry(void* entry, void* main_ptr_in_elf, void* main_main);
entry: elf entry in loaded go binary;
main_ptr_in_elf: the address saving main.main, called in runtime.main;
main_main: your function to run in go env

- call_go_func
void call_go_func(void* func, void* out, size_t out_count, ...); // assume out_count <= 7 && in_count <= 7

**/

void __attribute((noreturn)) go_compat_entry(void* entry, void* main_ptr_in_elf, void* main_main);

void call_go_func(void* func, void* out, size_t out_count, ...); // assume out_count <= 7 && in_count <= 7

#endif