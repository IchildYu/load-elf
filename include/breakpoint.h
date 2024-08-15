#ifndef __BREAKPOINT_H__
#define __BREAKPOINT_H__

/*
ARM:
https://github.com/torvalds/linux/blob/master/arch/arm/include/uapi/asm/sigcontext.h
struct sigcontext {
	size_t trap_no;
	size_t error_code;
	size_t oldmask;
	size_t arm_r0;
	size_t arm_r1;
	size_t arm_r2;
	size_t arm_r3;
	size_t arm_r4;
	size_t arm_r5;
	size_t arm_r6;
	size_t arm_r7;
	size_t arm_r8;
	size_t arm_r9;
	size_t arm_r10;
	size_t arm_fp;
	size_t arm_ip;
	size_t arm_sp;
	size_t arm_lr;
	size_t arm_pc;
	size_t arm_cpsr;
	size_t fault_address;
};

ARM64:
https://github.com/torvalds/linux/blob/master/arch/arm64/include/uapi/asm/sigcontext.h
struct sigcontext {
	__u64 fault_address;
	__u64 regs[31];
	__u64 sp;
	__u64 pc;
	__u64 pstate;
	__u8 __reserved[4096] __attribute__((__aligned__(16)));
};

x64 and x64:
https://github.com/torvalds/linux/blob/master/arch/x86/include/uapi/asm/sigcontext.h
// ignore
**/

// #define ARM
// #define ARM64

#if defined(ARM)
	// arm mode, NOT THUMB MODE
	typedef struct SigContext {
		size_t trap_no;
		size_t error_code;
		size_t oldmask;
		size_t regs[11];
		size_t fp;
		size_t ip;
		size_t sp;
		size_t lr;
		size_t pc;
		size_t cpsr;
		size_t fault_address;
	} SigContext;
#elif defined(ARM64) || defined(AARCH64)
	typedef struct SigContext {
		size_t fault_address;
		size_t regs[31];
		size_t sp;
		size_t pc;
		size_t pstate;
	} SigContext;
#elif defined(X64)
	typedef struct SigContext {
		size_t r8;
		size_t r9;
		size_t r10;
		size_t r11;
		size_t r12;
		size_t r13;
		size_t r14;
		size_t r15;
		size_t rdi;
		size_t rsi;
		size_t rbp;
		size_t rbx;
		size_t rdx;
		size_t rax;
		size_t rcx;
		size_t rsp;
		union { size_t pc; size_t rip; }; // size_t rip;
		size_t eflags; /* RFLAGS */
		unsigned short cs, gs, fs, ss;
		// size_t err, trapno, oldmask, cr2; struct _fpstate* fpstate; // ...
	} SigContext;
	#define TRAP_FLAG (0x100)
#elif defined(X86)
	typedef struct SigContext {
		unsigned short gs, __gsh;
		unsigned short fs, __fsh;
		unsigned short es, __esh;
		unsigned short ds, __dsh;
		size_t edi;
		size_t esi;
		size_t ebp;
		size_t esp;
		size_t ebx;
		size_t edx;
		size_t ecx;
		size_t eax;
		size_t trapno;
		size_t err;
		union { size_t pc; size_t eip; }; // size_t eip;
		unsigned short cs, __csh;
		size_t eflags;
		size_t esp_at_signal;
		unsigned short ss, __ssh;
		// /* struct _fpstate __user */ void *fpstate;
		// size_t oldmask;
		// size_t cr2;
	} SigContext;
	#define TRAP_FLAG (0x100)
#else
	#error "invalid arch"
#endif

void breakpoint(void* address, void (*handler)(SigContext* ctx));

#endif
