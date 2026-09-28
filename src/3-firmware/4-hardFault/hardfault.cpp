/*
 * hardfault_capture.c / .cpp
 *
 * Drop this into the project (any .c/.cpp compiled into the firmware).
 * It overrides the weak HardFault_Handler provided by the SDK's
 * startup file. When a hardFault occurs, instead of the default
 * "infinite loop with no information," this captures the CPU state
 * at the moment of the fault into plain global variables, then halts.
 *
 * HOW TO USE:
 *   1. Let it fault as you normally do (run main1's config until the
 *      hardFault occurs).
 *   2. Once halted (it spins in place at the end), open a debugger
 *      Expressions/Watch view and read g_fault_pc and g_fault_lr.
 *   3. Open the project's .map file (or MCUXpresso's disassembly view
 *      pointed at g_fault_pc) to find which function that address
 *      falls inside — that's the function that was actually executing
 *      when things went wrong, no more guessing needed.
 *   4. g_fault_lr, if it points into valid code (not 0xFFFFFFF9/FD/F1,
 *      which mean "returning from exception"), is often the caller of
 *      whatever crashed - useful if g_fault_pc alone lands inside a
 *      very generic/inlined helper.
 */

#include <stdint.h>

volatile uint32_t g_fault_r0;
volatile uint32_t g_fault_r1;
volatile uint32_t g_fault_r2;
volatile uint32_t g_fault_r3;
volatile uint32_t g_fault_r12;
volatile uint32_t g_fault_lr;		// the return address INTO the function that faulted
volatile uint32_t g_fault_pc;		// the actual instruction address that faulted - look this one up first
volatile uint32_t g_fault_psr;

#if defined(__cplusplus)
extern "C" {
#endif

void HardFault_Handler(void) __attribute__((naked));
void HardFault_Capture(uint32_t* stack_frame);

void HardFault_Handler(void)
{
	__asm volatile
	(
		" movs r0, #4           \n"
		" mov  r1, lr           \n"
		" tst  r0, r1           \n"	// bit 2 of EXC_RETURN tells us MSP or PSP was in use
		" beq  1f               \n"
		" mrs  r0, psp          \n"
		" b    2f               \n"
		"1:                     \n"
		" mrs  r0, msp          \n"
		"2:                     \n"
		" ldr  r1, =HardFault_Capture \n"
		" bx   r1               \n"
	);
}

void HardFault_Capture(uint32_t* stack_frame)
{
	g_fault_r0  = stack_frame[0];
	g_fault_r1  = stack_frame[1];
	g_fault_r2  = stack_frame[2];
	g_fault_r3  = stack_frame[3];
	g_fault_r12 = stack_frame[4];
	g_fault_lr  = stack_frame[5];
	g_fault_pc  = stack_frame[6];
	g_fault_psr = stack_frame[7];

	while(1);	// halt here on purpose - read the globals above with the debugger
}

#if defined(__cplusplus)
}
#endif
