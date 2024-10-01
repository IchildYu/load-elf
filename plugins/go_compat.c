
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USER_STACK_SIZE (0x100000 - 0x100)
// addr1:
// In go_compat_entry, it is set to user_stack_addr
// In enter_go_entry, user_stack_addr is set to c_ctx.rsp
// In main_main_stub, it is set to 0
// and then it is return address for go_ret_addr_hook
static void* addr1;

// addr2:
// In go_compat_entry, it is set to user's main.main
// In main_main_stub, user's main.main is used
// and then it is set to go runtime retaddr
static void* addr2;

static struct {
	size_t rdi; /* 00 */
	size_t rsi; /* 08 */
	size_t rax; /* 10 */
	size_t rcx; /* 18 */
	size_t rdx; /* 20 */
	size_t rbx; /* 28 */
	size_t rbp; /* 30 */
	size_t rsp; /* 38 */
	size_t r8 ; /* 40 */
	size_t r9 ; /* 48 */
	size_t r10; /* 50 */
	size_t r11; /* 58 */
	size_t r12; /* 60 */
	size_t r13; /* 68 */
	size_t r14; /* 70 */
	size_t r15; /* 78 */
} go_ctx, c_ctx;

static unsigned char saved_go_ins[16];
// saved memory between $fs_base - 0x100 and $fs_base + 0x100
static unsigned char c_fs[0x200];
static unsigned char go_fs[0x200];

static void __attribute__((naked)) save_c_ctx() {
	asm volatile(
		// save fs memory
		"push rax\n"
		"push rbx\n"
		"push rcx\n"
		"mov rax, -0x100\n"
		"lea rbx, [rip + c_fs]\n"
		"save_c_fs_loop:"
		"mov rcx, fs:[rax]\n"
		"mov [rbx], rcx\n"
		"add rax, 8\n"
		"add rbx, 8\n"
		"cmp rax, 0x100\n"
		"jnz save_c_fs_loop\n"
		"pop rcx\n"
		"pop rbx\n"
		"pop rax\n"
		// save registers
		"mov [rip + c_ctx + 0x00], rdi\n"
		"mov [rip + c_ctx + 0x08], rsi\n"
		"mov [rip + c_ctx + 0x10], rax\n"
		"mov [rip + c_ctx + 0x18], rcx\n"
		"mov [rip + c_ctx + 0x20], rdx\n"
		"mov [rip + c_ctx + 0x28], rbx\n"
		"mov [rip + c_ctx + 0x30], rbp\n"
		// here we just save rsp safely
		// though it should be rsp + 8
		// because it must be restored by restore_xxx_ctx
		// and there this extra slot will be put ret addr
		// and returns original rsp after ret
		"mov [rip + c_ctx + 0x38], rsp\n"
		"mov [rip + c_ctx + 0x40], r8 \n"
		"mov [rip + c_ctx + 0x48], r9 \n"
		"mov [rip + c_ctx + 0x50], r10\n"
		"mov [rip + c_ctx + 0x58], r11\n"
		"mov [rip + c_ctx + 0x60], r12\n"
		"mov [rip + c_ctx + 0x68], r13\n"
		"mov [rip + c_ctx + 0x70], r14\n"
		"mov [rip + c_ctx + 0x78], r15\n"
		"ret\n"
	);
}

static void __attribute__((naked)) restore_c_ctx() {
	asm volatile(
		// restore fs memory
		"mov rax, -0x100\n"
		"lea rbx, [rip + c_fs]\n"
		"restore_c_fs_loop:"
		"mov rcx, [rbx]\n"
		"mov fs:[rax], rcx\n"
		"add rax, 8\n"
		"add rbx, 8\n"
		"cmp rax, 0x100\n"
		"jnz restore_c_fs_loop\n"
		// restore registers
		"mov rax, [rsp]\n" // retaddr
		"mov rdi, [rip + c_ctx + 0x00]\n"
		"mov rsi, [rip + c_ctx + 0x08]\n"
		"mov rcx, [rip + c_ctx + 0x18]\n"
		"mov rdx, [rip + c_ctx + 0x20]\n"
		"mov rbx, [rip + c_ctx + 0x28]\n"
		"mov rbp, [rip + c_ctx + 0x30]\n"
		"mov rsp, [rip + c_ctx + 0x38]\n"
		"mov r8 , [rip + c_ctx + 0x40]\n"
		"mov r9 , [rip + c_ctx + 0x48]\n"
		"mov r10, [rip + c_ctx + 0x50]\n"
		"mov r11, [rip + c_ctx + 0x58]\n"
		"mov r12, [rip + c_ctx + 0x60]\n"
		"mov r13, [rip + c_ctx + 0x68]\n"
		"mov r14, [rip + c_ctx + 0x70]\n"
		"mov r15, [rip + c_ctx + 0x78]\n"
		"mov [rsp], rax\n"
		"mov rax, [rip + c_ctx + 0x10]\n"
		"ret\n"
	);
}

static void __attribute__((naked)) save_go_ctx() {
	asm volatile(
		"push rax\n"
		"push rbx\n"
		"push rcx\n"
		"mov rax, -0x100\n"
		"lea rbx, [rip + go_fs]\n"
		"save_go_fs_loop:"
		"mov rcx, fs:[rax]\n"
		"mov [rbx], rcx\n"
		"add rax, 8\n"
		"add rbx, 8\n"
		"cmp rax, 0x100\n"
		"jnz save_go_fs_loop\n"
		"pop rcx\n"
		"pop rbx\n"
		"pop rax\n"
		"mov [rip + go_ctx + 0x00], rdi\n"
		"mov [rip + go_ctx + 0x08], rsi\n"
		"mov [rip + go_ctx + 0x10], rax\n"
		"mov [rip + go_ctx + 0x18], rcx\n"
		"mov [rip + go_ctx + 0x20], rdx\n"
		"mov [rip + go_ctx + 0x28], rbx\n"
		"mov [rip + go_ctx + 0x30], rbp\n"
		"mov [rip + go_ctx + 0x38], rsp\n"
		"mov [rip + go_ctx + 0x40], r8\n"
		"mov [rip + go_ctx + 0x48], r9\n"
		"mov [rip + go_ctx + 0x50], r10\n"
		"mov [rip + go_ctx + 0x58], r11\n"
		"mov [rip + go_ctx + 0x60], r12\n"
		"mov [rip + go_ctx + 0x68], r13\n"
		"mov [rip + go_ctx + 0x70], r14\n"
		"mov [rip + go_ctx + 0x78], r15\n"
		"ret\n"
	);
}

static void __attribute__((naked)) restore_go_ctx() {
	asm volatile(
		"mov rax, -0x100\n"
		"lea rbx, [rip + go_fs]\n"
		"restore_go_fs_loop:"
		"mov rcx, [rbx]\n"
		"mov fs:[rax], rcx\n"
		"add rax, 8\n"
		"add rbx, 8\n"
		"cmp rax, 0x100\n"
		"jnz restore_go_fs_loop\n"
		"mov rax, [rsp]\n"
		"mov rdi, [rip + go_ctx + 0x00]\n"
		"mov rsi, [rip + go_ctx + 0x08]\n"
		"mov rcx, [rip + go_ctx + 0x18]\n"
		"mov rdx, [rip + go_ctx + 0x20]\n"
		"mov rbx, [rip + go_ctx + 0x28]\n"
		"mov rbp, [rip + go_ctx + 0x30]\n"
		"mov rsp, [rip + go_ctx + 0x38]\n"
		"mov r8 , [rip + go_ctx + 0x40]\n"
		"mov r9 , [rip + go_ctx + 0x48]\n"
		"mov r10, [rip + go_ctx + 0x50]\n"
		"mov r11, [rip + go_ctx + 0x58]\n"
		"mov r12, [rip + go_ctx + 0x60]\n"
		"mov r13, [rip + go_ctx + 0x68]\n"
		"mov r14, [rip + go_ctx + 0x70]\n"
		"mov r15, [rip + go_ctx + 0x78]\n"
		"mov [rsp], rax\n"
		"mov rax, [rip + go_ctx + 0x10]\n"
		"ret\n"
	);
}

static void __attribute__((naked,noreturn)) enter_go_entry(void* entry) {
	asm volatile(
		"call save_c_ctx\n"
		"mov rax, [rip + addr1]\n"
		"mov [rip + c_ctx + 0x38], rax\n" // c_ctx.rsp = addr1
		"push 0\n"
		"push 0\n"
		"push 0\n"
		"push 0\n"
		// currently args-setting not supported
		// though it's simple
		"lea rax, [rip+cmdline]\n"
		"push rax\n"
		"push 1\n"
		"jmp rdi\n"
		"cmdline:\n"
		".string \"./main\"\n"
	);
}

static void __attribute__((naked)) go_ret_addr_hook() {
	// here a go func returns back to go runtime
	asm volatile(
		"cmp qword ptr [rip + addr1], 0\n"
		"jz go_runtime_exit\n"
		"push [rip + addr2]\n"
		"push [rip + addr1]\n"
		"mov qword ptr [rip + addr1], 0\n"
		"ret\n"
		"go_runtime_exit:"
		"push [rip + saved_go_ins + 8]\n"
		"push [rip + saved_go_ins]\n"
		"mov rax, [rip + addr2]\n"
		"pop [rax]\n"
		"pop [rax + 8]\n"
		"jmp rax\n"
	);
}

/*
push rax
mov rax, 0xdeadbeefcafebabe
xchg [rsp], rax
ret
**/
static void __attribute__((naked)) main_main_stub() {
	asm volatile(
		"mov qword ptr [rip + addr1], 0\n"
		"call save_go_ctx\n"
		"call restore_c_ctx\n"

		"push [rip + addr2]\n"
		"mov rax, [rip + go_ctx + 0x38]\n" // rsp
		"mov rax, [rax + 8]\n" // go runtime retaddr
		"mov [rip + addr2], rax\n"
		"push [rax + 8]\n"
		"push [rax]\n"
		"pop [rip + saved_go_ins]\n"
		"pop [rip + saved_go_ins + 8]\n"
		"mov dword ptr [rax], 0xb84850\n"
		"lea rcx, [rip + go_ret_addr_hook]\n"
		"mov [rax + 3], rcx\n"
		"mov byte ptr [rax + 0xb], 0x48\n"
		"mov dword ptr [rax + 0xc], 0xc3240487\n"
		"pop rax\n"
		"call rax\n"
		"call save_c_ctx\n"
		"call restore_go_ctx\n"
		"ret\n"
	);
}

void __attribute((noreturn)) go_compat_entry(void* entry, void* main_ptr_in_elf, void* main_main) {
	memset(&c_ctx, 0, sizeof(c_ctx));
	memset(&go_ctx, 0, sizeof(go_ctx));

	// In go env, if main.main returns, it directly calls sys_exit_group to exit
	// If output was redirected (e.g. python subprocess),
	// stdout was buffered fully.
	// And if no fflush was called,
	// output would disappear.
	// For normal executables,
	// they call exit when main returns,
	// and fflush would always be called.
	// So here we only call setvbuf in go_compat_entry.
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	// before main_main_stub, it's saved user stack
	addr1 = (void*) ((((size_t) malloc(USER_STACK_SIZE) + USER_STACK_SIZE) & ~0xff) - 0x18);
	addr2 = main_main;
	*(void**) main_ptr_in_elf = main_main_stub;
	enter_go_entry(entry);

	// never reaches here
	puts("Error: enter_go_entry returned");
	exit(-1);

	// unused
	// avoid warning: ‘save_go_ctx’ defined but not used
	(void) save_go_ctx;
	(void) save_c_ctx;
	(void) restore_go_ctx;
	(void) restore_c_ctx;
	(void) go_ret_addr_hook;
	(void) c_fs;
	(void) go_fs;
	(void) saved_go_ins;
}

void call_go_func(void* func, void* out, size_t out_count, ...); // assume out_count <= 7 && in_count <= 7

asm(
	".global call_go_func\n"
	"call_go_func:\n"
	"call save_c_ctx\n"
	"call restore_go_ctx\n"
	"lea rax, [rip + go_func_ret]\n"
	"mov [rip + addr1], rax\n" // set go runtime retaddr

	// prepare args
	"mov rax, [rip + c_ctx + 0x18]\n" // arg1: rcx -> rax
	"mov rbx, [rip + c_ctx + 0x40]\n" // arg2: r8 -> rbx
	"mov rcx, [rip + c_ctx + 0x48]\n" // arg3: r9 -> rcx
	"mov r9, [rip + c_ctx + 0x38]\n" // rsp
	"mov rdi, [r9 + 0x10]\n" // arg4: [rsp+0x08] -> rdi
	"mov rsi, [r9 + 0x18]\n" // arg5: [rsp+0x10] -> rsi
	"mov r8, [r9 + 0x20]\n" // arg6: [rsp+0x18] -> r8
	"mov r9, [r9 + 0x28]\n" // arg7: [rsp+0x20] -> r9
	"jmp [rip + c_ctx + 0x00]\n" // rdi, func

	"go_func_ret:\n"
	"call save_go_ctx\n"
	"call restore_c_ctx\n"

	"cmp rsi, 0\n" // out is NULL
	"jz call_go_func_ret\n"

	"cmp rdx, 0\n"
	"jz call_go_func_ret\n"

	"mov rax, [rip + go_ctx + 0x10]\n" // ret1, rax
	"mov [rsi + 0x00], rax\n"
	"cmp rdx, 1\n"
	"jz call_go_func_ret\n"

	"mov rax, [rip + go_ctx + 0x28]\n" // ret2, rbx
	"mov [rsi + 0x08], rax\n"
	"cmp rdx, 2\n"
	"jz call_go_func_ret\n"

	"mov rax, [rip + go_ctx + 0x18]\n" // ret3, rcx
	"mov [rsi + 0x10], rax\n"
	"cmp rdx, 3\n"
	"jz call_go_func_ret\n"

	"mov rax, [rip + go_ctx + 0x00]\n" // ret4, rdi
	"mov [rsi + 0x18], rax\n"
	"cmp rdx, 4\n"
	"jz call_go_func_ret\n"

	"mov rax, [rip + go_ctx + 0x08]\n" // ret5, rsi
	"mov [rsi + 0x20], rax\n"
	"cmp rdx, 5\n"
	"jz call_go_func_ret\n"

	"mov rax, [rip + go_ctx + 0x40]\n" // ret6, r8
	"mov [rsi + 0x28], rax\n"
	"cmp rdx, 6\n"
	"jz call_go_func_ret\n"

	"mov rax, [rip + go_ctx + 0x48]\n" // ret7, r9
	"mov [rsi + 0x30], rax\n"

	// ignore more ret

	"call_go_func_ret:\n"
	"ret\n"
);

