# soocip

soocip. aka picoos since the project is already exists.

a basic context-switching OS designed for learning purposes. It runs on the Raspberry Pi Pico (RP2040).

Three tasks are created by default: two of them loop through and print the string with the count, and the other task blinks the default led on the pico.

Logs output to UART(TX:16, RX:17)

...

## Links

- [Exception entry and return](https://developer.arm.com/documentation/dui0662/b/The-Cortex-M0--Processor/Exception-model/Exception-entry-and-return)

- [https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/d1f551e2539ef7324013aebf22cd430a690e0d42/portable/GCC/ARM_CM0/portasm.c#L307](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/d1f551e2539ef7324013aebf22cd430a690e0d42/portable/GCC/ARM_CM0/portasm.c#L307)

- [FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/GCC/ARM_CM3/port.c)

- [Context Switch on the ARM Cortex-M0](https://www.adamh.cz/blog/2016/07/context-switch-on-the-arm-cortex-m0/)

- [https://github.com/adamheinrich/os.h](https://github.com/adamheinrich/os.h)
