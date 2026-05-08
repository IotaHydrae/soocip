#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/stdio_uart.h"

#include "core_cm0plus.h"
#include "systick.h"

#define pr_debug(...) printf(__VA_ARGS__)
#define __inline      inline __attribute__((always_inline))

#define MAX_TASKS  3
#define STACK_SIZE 256
struct task_struct {
	u32 *sp;
};

struct task_struct tasks[MAX_TASKS];
static uint current_task = 0;
struct task_struct *current = NULL;
struct task_struct *next = NULL;

#define __WFI() asm volatile("wfi" ::: "memory")

static __inline void __disable_irq(void)
{
    asm volatile("cpsid i");
}

static inline void __enable_irq(void)
{
    asm volatile("cpsie i");
}

static __inline void __ISB(void)
{
	asm volatile("isb 0xF" ::: "memory");
}

static __inline uint32_t __get_PSP(void)
{
	uint32_t result;
	asm volatile("mrs %0, psp" : "=r"(result));
	return (result);
}

static __inline void __set_PSP(uint32_t p)
{
	asm volatile("msr psp, %0" ::"r"(p) :);
}

static __inline uint32_t __get_CONTROL(void)
{
	uint32_t result;

	asm volatile("MRS %0, control" : "=r"(result));
	return (result);
}

static __inline void __set_CONTROL(uint32_t control)
{
	asm volatile("MSR control, %0" : : "r"(control) : "memory");
	__ISB();
}

void dump_regs(u32 *sp)
{
	u32 *p = sp;
	unsigned i;

	for (i = 0; i < 16; i++)
		pr_debug("[sp, #0x%03X = 0x%p] = 0x%08X\n", i * 4, p, *(--p));

	pr_debug("\n\n");
}

void pendsv_handler_c(uint32_t *push, uint32_t *regs, uint32_t ret_lr)
{
	uint32_t *sp = push;
	unsigned i;

	printf("%s, %p, PSP : 0x%08X\n", __func__, push, __get_PSP());

	pr_debug("============ PendSV regs Dump ============\n");

	// for (i = 0; i < 16; i++)
	// 	pr_debug("[psp, #0x%03X = 0x%08X] = 0x%08X\n", i * 4,
	// 		 (unsigned)sp, *(--sp));

	printf("r11 = %08X\n", sp[0]);
	printf("r10 = %08X\n", sp[1]);
	printf("r9  = %08X\n", sp[2]);
	printf("r8  = %08X\n", sp[3]);
	printf("r7  = %08X\n", sp[4]);
	printf("r6  = %08X\n", sp[5]);
	printf("r5  = %08X\n", sp[6]);
	printf("r4  = %08X\n", sp[7]);

	printf("--------------------------------------------------\n");

	printf("r0  = %08X\n", sp[8]);
	printf("r1  = %08X\n", sp[9]);
	printf("r2  = %08X\n", sp[10]);
	printf("r3  = %08X\n", sp[11]);
	printf("r12 = %08X\n", sp[12]);
	printf("lr  = %08X\n", sp[13]);
	printf("pc  = %08X\n", sp[14]);
	printf("xpsr= %08X\n", sp[15]);

	printf("--------------------------------------------------\n");

	// for (i = 0; i < 16; i++)
	// 	pr_debug("[regs, #0x%03X = 0x%08X] = 0x%08X\n", i,
	// 		 (unsigned)regs, *(--regs));

	// printf("--------------------------------------------------\n");

	// printf("lr: 0x%08X\n", ret_lr);

	// printf("--------------------------------------------------\n");

	pr_debug("\n\n");

	for (;;)
		;
}

void dump_r0_r1_r2(uint32_t *r0, uint32_t *r1, uint32_t *r2)
{
	printf("R0: 0x%08X = 0x%08X\n", r0, *r0);
	printf("R1: 0x%08X = 0x%08X\n", r1, *r1);
	printf("R2: 0x%08X = 0x%08X\n", r2, *r2);
}

void task1_func(void *data)
{
	static uint count = 0;
	for (;;) {
		printf("\n++++++++++ %s, %04d ++++++++++\n", (char *)data,
		       count++);
		for (volatile uint i = 0; i < 1000000; i++)
			;
	}
}

void task3_func(void *data)
{
	int led_pin = PICO_DEFAULT_LED_PIN;

	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	for (;;) {
		gpio_put(led_pin, 1);
		sleep_ms(500);
		gpio_put(led_pin, 0);
		sleep_ms(500);
	}
}

void isr_hardfault(void)
{
	panic("%s\n", __func__);
}

void schedule(void)
{
	/* A simple Round Robin scheduler */
	__disable_irq();
	current = &tasks[current_task];
	current_task = (current_task + 1) % MAX_TASKS;
	next = &tasks[current_task];
	__enable_irq();

	// static uint count = 0;
	// pr_debug("%s-[%d], switch to {%d} | %08x, %08x\n", __func__, count++,
	// 	 current_task, *current, *next);

	/* This will trigger PendSV isr */
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void isr_systick(void)
{
	schedule();
}

void start_scheduler(void)
{
	/* the first task ready to run */
	current = &tasks[0];

	/* load the first task's sp position to the PSP */
	__set_PSP((u32)tasks[0].sp);
	__set_CONTROL(0x02);

	/* trigger SVC and start the first task, see `isr_scvall` */
	asm volatile("svc 0");

	for (;;)
		__WFI();
}

void task_init(struct task_struct *task, void (*entry)(void *data), u32 *sp_top,
	       void *data)
{
	u32 *sp = sp_top;

	/* r0, r1, r2, r3, r12, lr, pc, xpsr */
	*(--sp) = 0x01000000; /* Thumb mode */
	*(--sp) = (u32)entry;
	*(--sp) = 0xFFFFFFFD; /* Return to thread mode and use PSP */
	*(--sp) = 0x12; // r12
	*(--sp) = 0x03; // r3
	*(--sp) = 0x02; // r2
	*(--sp) = 0x01; // r1
	*(--sp) = (u32)data; // r0

	/* r4 ~ r11 */
	for (int i = 0; i < 8; i++) {
		// printf("%s, %p\n", __func__, sp);
		*(--sp) = 4 + i;
	}

	// dump_regs(sp);
	task->sp = sp; /* save the current pos of it's stack */
	printf("%s, %08X\n", __func__, task->sp);
}

static u32 task1_stack[STACK_SIZE] __attribute__((aligned(4)));
static u32 task2_stack[STACK_SIZE] __attribute__((aligned(4)));
static u32 task3_stack[STACK_SIZE] __attribute__((aligned(4)));
int main()
{
	stdio_uart_init_full(uart0, 115200, 16, 17);
	// stdio_usb_init();

	pr_debug("\n\n\nsoocip starting...\n\n");

	/* we can use the same func for two tasks */
	task_init(&tasks[0], task1_func, task1_stack + STACK_SIZE, "task-1");
	task_init(&tasks[1], task1_func, task2_stack + STACK_SIZE, "task-2");

	task_init(&tasks[2], task3_func, task3_stack + STACK_SIZE, "task-3");

	printf("%p\n", task1_stack + STACK_SIZE);
	// uint32_t *sp = tasks[0].sp + 8;

	// printf("PSP = %p\n", sp);
	// printf("r0  = %08X\n", sp[0]);
	// printf("r1  = %08X\n", sp[1]);
	// printf("r2  = %08X\n", sp[2]);
	// printf("r3  = %08X\n", sp[3]);
	// printf("r12 = %08X\n", sp[4]);
	// printf("lr  = %08X\n", sp[5]);
	// printf("pc  = %08X\n", sp[6]);
	// printf("xpsr= %08X\n", sp[7]);

	systick_init(10);

	start_scheduler();
	return 0;
}
