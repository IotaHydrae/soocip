#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/stdio_uart.h"

#include "core_cm0plus.h"
#include "systick.h"

#define DEBUG 1

#define pr_debug(...) printf(__VA_ARGS__)
// #define __always_inline inline __attribute__((always_inline))

#define MAX_TASKS  4
#define STACK_SIZE 16
struct task_struct {
	u32 *sp;
};

struct task_struct tasks[MAX_TASKS];
static uint current_task = 0;

#define __WFI() asm volatile("wfi" ::: "memory")

__always_inline void __ISB(void)
{
	asm volatile("isb 0xF" ::: "memory");
}

static __always_inline uint32_t __get_PSP(void)
{
	uint32_t result;
	asm volatile("mrs %0, psp" : "=r"(result));
	return (result);
}

static __always_inline void __set_PSP(uint32_t p)
{
	asm volatile("msr psp, %0" ::"r"(p) :);
}

static __always_inline uint32_t __get_CONTROL(void)
{
	uint32_t result;

	asm volatile("MRS %0, control" : "=r"(result));
	return (result);
}

static __always_inline void __set_CONTROL(uint32_t control)
{
	asm volatile("MSR control, %0" : : "r"(control) : "memory");
	__ISB();
}

static void dump_regs(u32 *sp)
{
	u32 *p = sp;
	unsigned i;

	// pr_debug("============ HARD FAULT ============\n");
	// pr_debug("R0  = 0x%08X    R8  = 0x%08X\n", (unsigned)push[0],
	// 	 (unsigned)regs[0]);
	// pr_debug("R1  = 0x%08X    R9  = 0x%08X\n", (unsigned)push[1],
	// 	 (unsigned)regs[1]);
	// pr_debug("R2  = 0x%08X    R10 = 0x%08X\n", (unsigned)push[2],
	// 	 (unsigned)regs[2]);
	// pr_debug("R3  = 0x%08X    R11 = 0x%08X\n", (unsigned)push[3],
	// 	 (unsigned)regs[3]);
	// pr_debug("R4  = 0x%08X    R12 = 0x%08X\n", (unsigned)regs[4],
	// 	 (unsigned)push[4]);
	// pr_debug("R5  = 0x%08X    SP  = 0x%08X\n", (unsigned)regs[5],
	// 	 (unsigned)sp);
	// pr_debug("R6  = 0x%08X    LR  = 0x%08X\n", (unsigned)regs[6],
	// 	 (unsigned)push[5]);
	// pr_debug("R7  = 0x%08X    PC  = 0x%08X\n", (unsigned)regs[7],
	// 	 (unsigned)push[6]);
	// pr_debug("RA  = 0x%08X    SR  = 0x%08X\n", (unsigned)ret_lr,
	// 	 (unsigned)push[7]);
	// pr_debug("WORDS @ SP: \n");

	// for (i = 0; i < 8; i++)
	// 	pr_debug("[sp, #0x%03X = 0x%08X] = 0x%08x\n", i * 4,
	// 		 (unsigned)&sp[i], (unsigned)sp[i]);

	p += 16;
	for (i = 0; i < 16; i++)
		pr_debug("[sp, #0x%03X = 0x%p] = 0x%08X\n", i * 4, p, *(--p));

	pr_debug("\n\n");
}

void hard_fault_handler_c(uint32_t *push, uint32_t *regs, uint32_t ret_lr)
{
	uint32_t *sp = push;
	unsigned i;

	pr_debug("============ PendSV saved regs Dump ============\n");
	// pr_debug("R0  = 0x%08X    R8  = 0x%08X\n", (unsigned)push[0],
	// 	 (unsigned)regs[0]);
	// pr_debug("R1  = 0x%08X    R9  = 0x%08X\n", (unsigned)push[1],
	// 	 (unsigned)regs[1]);
	// pr_debug("R2  = 0x%08X    R10 = 0x%08X\n", (unsigned)push[2],
	// 	 (unsigned)regs[2]);
	// pr_debug("R3  = 0x%08X    R11 = 0x%08X\n", (unsigned)push[3],
	// 	 (unsigned)regs[3]);
	// pr_debug("R4  = 0x%08X    R12 = 0x%08X\n", (unsigned)regs[4],
	// 	 (unsigned)push[4]);
	// pr_debug("R5  = 0x%08X    SP  = 0x%08X\n", (unsigned)regs[5],
	// 	 (unsigned)sp);
	// pr_debug("R6  = 0x%08X    LR  = 0x%08X\n", (unsigned)regs[6],
	// 	 (unsigned)push[5]);
	// pr_debug("R7  = 0x%08X    PC  = 0x%08X\n", (unsigned)regs[7],
	// 	 (unsigned)push[6]);
	// pr_debug("RA  = 0x%08X    SR  = 0x%08X\n", (unsigned)ret_lr,
	// 	 (unsigned)push[7]);

	// printf("regs(sp): %08x\n", regs[6]);
	printf("LR: %08x\n", ret_lr);
	// pr_debug("WORDS @ SP: \n");

	for (i = 0; i < 16; i++)
		pr_debug("[sp, #0x%03X = 0x%08X] = 0x%08x\n", i * 4,
			 (unsigned)sp, *(sp--));

	// for (i = 0; i < 16; i++)
	// 	pr_debug("[sp, #0x%03X = 0x%p] = 0x%08x\n", i * 4, sp, *(--sp));

	pr_debug("\n\n");
}

void task1_func()
{
	for (;;) {
		printf("%s\n", __func__);
		for (volatile uint i = 0; i < 100000; i++)
			;
	}
}

void task2_func()
{
	for (;;) {
		printf("%s\n", __func__);
		for (volatile uint i = 0; i < 100000; i++)
			;
	}
}

void isr_pendsv(void)
{
	// pr_debug("%s, %d\n", __func__, 123);
	// dump_regs(tasks[current_task].sp);

	asm volatile(
		".syntax unified\n\t"
		"mrs r0, psp\n\t"
		"push     {r4, lr}\n\t"
		"mov r12, r0\n\t"
		"mov r0, r8\n\t"
		"mov r1, r9\n\t"
		"mov r2, r10\n\t"
		"mov r3, r11\n\t"
		"push {r0-r7}\n\t"
		"	mov		r0, r12									\n\t"
		"	mov		r1, sp									\n\t"
		"	mov		r2, lr									\n\t"
		"   ldr     r3, =hard_fault_handler_c               \n\t"
		"   blx      r3                                      \n\t"
		"pop {r0-r7}\n\t"
		"mov r8, r0\n\t"
		"mov r9, r1\n\t"
		"mov r10, r2\n\t"
		"mov r11, r3\n\t"
		"   pop     {r4}                                    \n\t"
		"   pop     {r3}                                    \n\t"
		"   mov     lr, r3                                  \n\t"
		// "   bx      lr                                      \n\t"
	);

	// dump_regs(tasks[current_task].sp);
	// task1_func();
}

void schedule(void)
{
	uint next = (current_task + 1) % MAX_TASKS;

	/* This will trigger PendSV isr */
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void isr_systick(void)
{
	static uint count = 0;
	pr_debug("%s, %d\n", __func__, count++);

	schedule();
}

void start_scheduler(void)
{
	__set_PSP((u32)tasks[0].sp);
	__set_CONTROL(0x02);

	printf("%s, PSP : 0x%08x\n", __func__, __get_PSP());
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

	for (;;)
		__WFI();
}

void task_init(struct task_struct *task, void (*entry)(void), u32 *sp_top)
{
	u32 *sp = sp_top;

	// printf("%s, %p\n", __func__, sp_top);

	/* r0, r1, r2, r3, r12, lr, pc, xpsr */
	*(--sp) = 0x01000000;
	*(--sp) = (u32)entry;
	*(--sp) = 0xFFFFFFFD; /* Return to thread mode and use PSP */
	*(--sp) = 0; // r12
	*(--sp) = 0; // r3
	*(--sp) = 0; // r2
	*(--sp) = 0; // r1
	*(--sp) = 0; // r0

	/* r4 ~ r11 */
	for (int i = 0; i < 8; i++)
		*(--sp) = 0;

	// printf("%s, %p\n", __func__, sp);
	task->sp = sp_top;
}

int main()
{
	stdio_uart_init_full(uart0, 115200, 16, 17);
	// stdio_usb_init();

	pr_debug("\n\n\nsoocip starting...\n");

	const uint led_pin = PICO_DEFAULT_LED_PIN;

	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	// struct task_struct dummy_task;

	// dummy_task.sp = dummy_stack;
	// __set_PSP((uint32_t)dummy_task.sp);

	// printf("%p, %X", dummy_stack, __get_PSP());
	static u32 task1_stack[STACK_SIZE];
	static u32 task2_stack[STACK_SIZE];
	printf("0x%p ~ 0x%p\n", task1_stack, task1_stack + STACK_SIZE);
	// printf("0x%p\n", task2_stack);

	task_init(&tasks[0], task1_func, task1_stack + STACK_SIZE);
	// task_init(&tasks[1], task2_func, task2_stack + STACK_SIZE);

	// printf("0x%p\n", tasks[0].sp + 16);
	printf("0x%p\n", task1_func);
	// printf("0x%p\n", task2_func);

	// systick_init(10);

	start_scheduler();

	// while (true) {
	// 	gpio_put(led_pin, 1);
	// 	sleep_ms(50);
	// 	gpio_put(led_pin, 0);
	// 	sleep_ms(50);
	// }

	return 0;
}
