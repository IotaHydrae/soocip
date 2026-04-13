#include <stdio.h>
#include "pico/stdlib.h"
#include <pico/stdio_usb.h>

#define DEBUG 1

#define pr_debug(...) printf(__VA_ARGS__)

void __attribute__((used)) faultHandlerWithExcFrame(uint32_t* push, uint32_t *regs, uint32_t ret_lr)
{
	uint32_t *sp = push;
	unsigned i;
	
	pr_debug("============ HARD FAULT ============\n");
	pr_debug("R0  = 0x%08X    R8  = 0x%08X\n", (unsigned)push[0], (unsigned)regs[0]);
	pr_debug("R1  = 0x%08X    R9  = 0x%08X\n", (unsigned)push[1], (unsigned)regs[1]);
	pr_debug("R2  = 0x%08X    R10 = 0x%08X\n", (unsigned)push[2], (unsigned)regs[2]);
	pr_debug("R3  = 0x%08X    R11 = 0x%08X\n", (unsigned)push[3], (unsigned)regs[3]);
	pr_debug("R4  = 0x%08X    R12 = 0x%08X\n", (unsigned)regs[4], (unsigned)push[4]);
	pr_debug("R5  = 0x%08X    SP  = 0x%08X\n", (unsigned)regs[5], (unsigned)sp);
	pr_debug("R6  = 0x%08X    LR  = 0x%08X\n", (unsigned)regs[6], (unsigned)push[5]);
	pr_debug("R7  = 0x%08X    PC  = 0x%08X\n", (unsigned)regs[7], (unsigned)push[6]);
	pr_debug("RA  = 0x%08X    SR  = 0x%08X\n", (unsigned)ret_lr,  (unsigned)push[7]);
	// pr_debug("SHCSR = 0x%08X\n", SCB->SHCSR);
    
	pr_debug("WORDS @ SP: \n");
	
	for (i = 0; i < 8; i++)
		pr_debug("[sp, #0x%03X = 0x%08X] = 0x%08x\n", i * 4, (unsigned)&sp[i], (unsigned)sp[i]);
	
	pr_debug("\n\n");
	while(1);
}

#define PSRAM_ADDR_START 0x2F000000
unsigned char fake_memory[16];

void hard_fault_handler_c(uint32_t *push, uint32_t *regs, uint32_t ret_lr)
{
    /*
     * The exception frame were saved onto "push" pointer 32 byte
     * 
     */
    uint32_t *sp = push;
#if DEBUG
    unsigned i;

    pr_debug("============ HARD FAULT ============\n");
	pr_debug("R0  = 0x%08X    R8  = 0x%08X\n", (unsigned)push[0], (unsigned)regs[0]);
	pr_debug("R1  = 0x%08X    R9  = 0x%08X\n", (unsigned)push[1], (unsigned)regs[1]);
	pr_debug("R2  = 0x%08X    R10 = 0x%08X\n", (unsigned)push[2], (unsigned)regs[2]);
	pr_debug("R3  = 0x%08X    R11 = 0x%08X\n", (unsigned)push[3], (unsigned)regs[3]);
	pr_debug("R4  = 0x%08X    R12 = 0x%08X\n", (unsigned)regs[4], (unsigned)push[4]);
	pr_debug("R5  = 0x%08X    SP  = 0x%08X\n", (unsigned)regs[5], (unsigned)sp);
	pr_debug("R6  = 0x%08X    LR  = 0x%08X\n", (unsigned)regs[6], (unsigned)push[5]);
	pr_debug("R7  = 0x%08X    PC  = 0x%08X\n", (unsigned)regs[7], (unsigned)push[6]);
	pr_debug("RA  = 0x%08X    SR  = 0x%08X\n", (unsigned)ret_lr,  (unsigned)push[7]);
	pr_debug("WORDS @ SP: \n");

	for (i = 0; i < 8; i++)
		pr_debug("[sp, #0x%03X = 0x%08X] = 0x%08x\n", i * 4, (unsigned)&sp[i], (unsigned)sp[i]);

	pr_debug("\n\n");
#endif


    unsigned addr = (unsigned)push[0];
    unsigned val  = (unsigned)push[1];

    pr_debug("write 0x%08x to 0x%08x\n", val, addr);

#if DEBUG
    pr_debug("\n------ dump fake memory (cut here) ------\n");
    for (i = 0; i < 16; i++) {
        pr_debug("addr: 0x%08x, val: 0x%08x\n", i + PSRAM_ADDR_START, fake_memory[i]);
    }
    pr_debug("------ dump fake memory (end) ------\n");
#endif

    pr_debug("TODO: Performing an actual PSRAM write operation here.\n");
    unsigned offset = addr - PSRAM_ADDR_START;
    pr_debug("offset = 0x%08x\n", offset);

    unsigned char *p = fake_memory;
    if (val > 0)
        fake_memory[offset] = val;

    pr_debug("p[%d] = 0x%02x\n", offset, p[offset]);

#if DEBUG
    pr_debug("\n------ dump fake memory (cut here) ------\n");
    for (i = 0; i < 16; i++) {
        pr_debug("addr: 0x%08x, val: 0x%08x\n", i + PSRAM_ADDR_START, fake_memory[i]);
    }
    pr_debug("------ dump fake memory (end) ------\n");
#endif

    push[0] = fake_memory[offset];

    /*
     * move PC to the next instruction
     * skip 2 bytes because using the Thumb.
     */
    sp[6]+=2;
    pr_debug("PC  = 0x%08X\n", sp[6]);
    pr_debug("exiting from handler...\n");
}

void isr_hardfault(void)
{
    asm volatile(
        ".syntax unified    \n\t"
        ".global HardFault_Handler \n\t"
        ".func HardFault_Handler \n\t"
        ".type HardFault_Handler function \n\t"
        "HardFault_Handler: \n\t"
        "   mov r0, lr \n\t"
        "   lsrs r0, #3 \n\t"
        "   bcs 1f  \n\t"
		"	mov   r0, sp									\n\t"
		"	b     2f										\n\t"
		"1:													\n\t"
		"	mrs   r0, psp									\n\t"
		"2:													\n\t"
		
		//to emulate-for-write fast, we must assume that PC points somewhere valid
		//otherwise we'd have to take the penalty of switching to out safe mode, and then wrangling the MPU
		//whereas now we can use "hard fault uses default map" mode
		
		// "	ldr		r2, [r0, #4 * 6]						\n\t"
		// "	ldrh	r1, [r2]								\n\t"
		// "	lsrs	r3, r1, #8								\n\t"
		// "	add		pc, r3									\n\t"
		// "	nop												\n\t"
        "   push     {r4, lr}                               \n\t"
        "   b      call_handler                             \n\t"
		// ".rept 35											\n\t"
		// "	b		report_some_fault						\n\t"
        // ".endr                                              \n\t"
        "call_handler:                                      \n\t"
        "	mov		r12, r0									\n\t"
		"	mov		r0, r8									\n\t"
		"	mov		r1, r9									\n\t"
		"	mov		r2, r10									\n\t"
		"	mov		r3, r11									\n\t"
		"	push	{r0-r7}									\n\t"
		"	mov		r0, r12									\n\t"
		"	mov		r1, sp									\n\t"
		"	mov		r2, lr									\n\t"
        "   ldr     r3, =hard_fault_handler_c               \n\t"
        "   blx      r3                                      \n\t"

        "   pop     {r0-r7}                                 \n\t"
		"	mov		r8,  r0									\n\t"
		"	mov		r9,  r1									\n\t"
		"	mov		r10, r2									\n\t"
		"	mov		r11, r3 								\n\t"

        "   pop     {r4}                                    \n\t"
        "   pop     {r3}                                    \n\t"
        "   mov     lr, r3                                  \n\t"
        "   bx      lr                                      \n\t"
        // "report_some_fault_pop_r4lr:                        \n\t"
        // "   pop     {r4}                                    \n\t"
        // "   pop     {r3}                                    \n\t"
        // "   mov     lr, r3                                  \n\t"
        // "report_some_fault:									\n\t"
		// "	mov		r12, r0									\n\t"
		// "	mov		r0, r8									\n\t"
		// "	mov		r1, r9									\n\t"
		// "	mov		r2, r10									\n\t"
		// "	mov		r3, r11									\n\t"
		// "	push	{r0-r7}									\n\t"
		// "	mov		r0, r12									\n\t"
		// "	mov		r1, sp									\n\t"
		// "	mov		r2, lr									\n\t"
		// "	ldr		r3, =faultHandlerWithExcFrame			\n\t"
		// "	bx		r3										\n\t"
		".ltorg												\n\t"
        :
        :
        : "cc", "memory", "r0", "r1", "r2", "r3", "r12"
    );
    return;
}


void isr_pendsv(void)
{

}

void isr_systick(void)
{

}

void __attribute__((used)) write_vmem(volatile unsigned addr, volatile unsigned char val)
{
    volatile unsigned int *p = (volatile unsigned int *)addr;
    /* This will trigger Hard Fault */
    *p = val;
}

int main()
{
    stdio_usb_init();

    const uint led_pin = PICO_DEFAULT_LED_PIN;

    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    // write_vmem(0x2f000004, 0xaa);

    while (true) {
        gpio_put(led_pin, 1);
        sleep_ms(50);
        gpio_put(led_pin, 0);
        sleep_ms(50);
    }

    return 0;
}
