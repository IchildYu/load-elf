
/*

go_compat.c

- go_compat_entry
void __attribute((noreturn)) go_compat_entry(void* entry, void* main_ptr_in_elf, void* main_main);
entry: elf entry in loaded go binary;
main_ptr_in_elf: the address saving main.main, called in runtime.main;
main_main: your function to run in go env

note: if your main_main returns instead of calling exit(0), python subprocess may truncate output


- call_go_func
void call_go_func(void* func, void* out, size_t out_count, ...); // assume out_count <= 7 && in_count <= 7

**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
	size_t rdi;
	size_t rsi;
	size_t rax;
	size_t rcx;
	size_t rdx;
	size_t rbx;
	size_t rbp;
	size_t r8;
	size_t r9;
	size_t r10;
	size_t r11;
	size_t r12;
	size_t r13;
	size_t r14;
	size_t r15;
} go_ctx, c_ctx;

static void __attribute__((naked)) save_c_ctx() {
	asm volatile(
		"push rdi\n"
		"lea rdi, [rip+c_ctx]\n"
		"mov [rdi + 0x08], rsi\n"
		"mov [rdi + 0x10], rax\n"
		"mov [rdi + 0x18], rcx\n"
		"mov [rdi + 0x20], rdx\n"
		"mov [rdi + 0x28], rbx\n"
		"mov [rdi + 0x30], rbp\n"
		"mov [rdi + 0x38], r8 \n"
		"mov [rdi + 0x40], r9 \n"
		"mov [rdi + 0x48], r10\n"
		"mov [rdi + 0x50], r11\n"
		"mov [rdi + 0x58], r12\n"
		"mov [rdi + 0x60], r13\n"
		"mov [rdi + 0x68], r14\n"
		"mov [rdi + 0x70], r15\n"
		"push rax\n"
		"mov rax, [rsp+8]\n"
		"mov [rdi], rax\n"
		"pop rax\n"
		"pop rdi\n"
		"ret\n"
	);
}

static void __attribute__((naked)) restore_c_ctx() {
	asm volatile(
		"push rdi\n"
		"lea rdi, [rip+c_ctx]\n"
		"mov rbx, [rdi + 0x28]\n"
		"mov rbp, [rdi + 0x30]\n"
		"mov r8 , [rdi + 0x38]\n"
		"mov r9 , [rdi + 0x40]\n"
		"mov r10, [rdi + 0x48]\n"
		"mov r11, [rdi + 0x50]\n"
		"mov r12, [rdi + 0x58]\n"
		"mov r13, [rdi + 0x60]\n"
		"mov r14, [rdi + 0x68]\n"
		"mov r15, [rdi + 0x70]\n"
		"pop rdi\n"
		"ret\n"
	);
}

static void __attribute__((naked)) save_go_ctx() {
	asm volatile(
		"push rdi\n"
		"lea rdi, [rip+go_ctx]\n"
		"mov [rdi + 0x08], rsi\n"
		"mov [rdi + 0x10], rax\n"
		"mov [rdi + 0x18], rcx\n"
		"mov [rdi + 0x20], rdx\n"
		"mov [rdi + 0x28], rbx\n"
		"mov [rdi + 0x30], rbp\n"
		"mov [rdi + 0x38], r8 \n"
		"mov [rdi + 0x40], r9 \n"
		"mov [rdi + 0x48], r10\n"
		"mov [rdi + 0x50], r11\n"
		"mov [rdi + 0x58], r12\n"
		"mov [rdi + 0x60], r13\n"
		"mov [rdi + 0x68], r14\n"
		"mov [rdi + 0x70], r15\n"
		"push rax\n"
		"mov rax, [rsp+8]\n"
		"mov [rdi], rax\n"
		"pop rax\n"
		"pop rdi\n"
		"ret\n"
	);
}

static void __attribute__((naked)) restore_go_ctx() {
	asm volatile(
		"push rdi\n"
		"lea rdi, [rip+go_ctx]\n"
		"mov rdx, [rdi + 0x20]\n"
		"mov rbp, [rdi + 0x30]\n"
		"mov r8 , [rdi + 0x38]\n"
		"mov r9 , [rdi + 0x40]\n"
		"mov r10, [rdi + 0x48]\n"
		"mov r11, [rdi + 0x50]\n"
		"mov r12, [rdi + 0x58]\n"
		"mov r13, [rdi + 0x60]\n"
		"mov r14, [rdi + 0x68]\n"
		"mov r15, [rdi + 0x70]\n"
		"pop rdi\n"
		"ret\n"
	);
}

static void __attribute__((naked,noreturn)) enter_go_entry(void* entry) {
	asm volatile(
		"call save_c_ctx\n"
		"push 0\n"
		"push 0\n"
		"push 0\n"
		"push 0\n"
		"jmp rdi\n"
	);
}

static void* _main_main;

static void __attribute__((naked)) main_main_stub() {
	asm volatile(
		"sub rsp, 8\n"
		"call save_go_ctx\n"
		"call restore_c_ctx\n"
		"call [rip+_main_main]\n"
		"call save_c_ctx\n"
		"call restore_go_ctx\n"
		"add rsp, 8\n"
		"ret\n"
	);
}

void __attribute((noreturn)) go_compat_entry(void* entry, void* main_ptr_in_elf, void* main_main) {
	memset(&c_ctx, 0, sizeof(c_ctx));
	memset(&go_ctx, 0, sizeof(go_ctx));
	_main_main = main_main;
	*(void**) main_ptr_in_elf = main_main_stub;
	enter_go_entry(entry);

	// never reaches here
	puts("Error: enter_go_entry returned");
	exit(-1);

	// unused
	// avoid warning: ‘save_go_ctx’ defined but not used
	save_go_ctx();
	save_c_ctx();
	restore_go_ctx();
	restore_c_ctx();
}

void call_go_func(void* func, void* out, size_t out_count, ...); // assume out_count <= 7 && in_count <= 7

asm(
	".global call_go_func\n"
	"call_go_func:\n"
	"call save_c_ctx\n"
	"push rdx\n" // out_count
	"push rsi\n" // out
	"push rdi\n" // func
	"push rcx\n" // arg1
	"push r8\n" // arg2
	"push r9\n" // arg3
	"mov rax, [rsp + 0x38]\n"
	"push rax\n" // arg4
	"mov rax, [rsp + 0x48]\n"
	"push rax\n" // arg5
	"mov rax, [rsp + 0x58]\n"
	"push rax\n" // arg6
	"mov rax, [rsp + 0x68]\n"
	"push rax\n" // arg7
	"call restore_go_ctx\n"
	"pop r9\n" // arg7
	"pop r8\n" // arg6
	"pop rsi\n" // arg5
	"pop rdi\n" // arg4
	"pop rcx\n" // arg3
	"pop rbx\n" // arg2
	"pop rax\n" // arg1
	"call [rsp]\n"
	"call save_go_ctx\n"

	"cmp qword ptr [rsp + 0x8], 0\n"
	"jz call_go_func_ret\n"

	"cmp qword ptr [rsp + 0x10], 0\n" // ret1
	"jz call_go_func_ret\n"
	"xchg [rsp + 8], rbx\n"
	"mov [rbx], rax\n"
	"xchg [rsp + 8], rbx\n"
	"mov rax, [rsp + 8]\n"
	"add rax, 8\n"
	"dec qword ptr [rsp + 0x10]\n"

	"cmp qword ptr [rsp + 0x10], 0\n" // ret2
	"jz call_go_func_ret\n"
	"mov [rax], rbx\n"
	"add rax, 8\n"
	"dec qword ptr [rsp + 0x10]\n"

	"cmp qword ptr [rsp + 0x10], 0\n" // ret3
	"jz call_go_func_ret\n"
	"mov [rax], rcx\n"
	"add rax, 8\n"
	"dec qword ptr [rsp + 0x10]\n"

	"cmp qword ptr [rsp + 0x10], 0\n" // ret4
	"jz call_go_func_ret\n"
	"mov [rax], rdi\n"
	"add rax, 8\n"
	"dec qword ptr [rsp + 0x10]\n"

	"cmp qword ptr [rsp + 0x10], 0\n" // ret5
	"jz call_go_func_ret\n"
	"mov [rax], rsi\n"
	"add rax, 8\n"
	"dec qword ptr [rsp + 0x10]\n"

	"cmp qword ptr [rsp + 0x10], 0\n" // ret6
	"jz call_go_func_ret\n"
	"mov [rax], r8\n"
	"add rax, 8\n"
	"dec qword ptr [rsp + 0x10]\n"

	"cmp qword ptr [rsp + 0x10], 0\n" // ret7
	"jz call_go_func_ret\n"
	"mov [rax], r9\n"
	"add rax, 8\n"
	"dec qword ptr [rsp + 0x10]\n"

	// ignore more ret

	"call_go_func_ret:\n"
	"call restore_c_ctx\n"
	"add rsp, 0x18\n"
	"ret\n"
);

