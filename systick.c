#include "systick.h"
#include "core_cm0plus.h"

void systick_init(u8 period_ms)
{
	SysTick->LOAD = 1250000 - 1;
	SysTick->VAL = 0;
	SysTick->CTRL = 0x07;
}
