#include "dr_pll.h"

void (*fro_set_frec) (int ) = FRO_SET_FRECUENCY_FUNC;

void PLL_init(){
	fro_set_frec(30000);	//	FRO_freq = 30MHz

	SYSCON->FROOSCCTRL |= 1<<17;	//	main_clk = FRO

	SYSCON->FRODIRECTCLKUEN &= ~1;	//	Manual indica primero escribir un 0 para actualizar
	SYSCON->FRODIRECTCLKUEN |= 1;	//	Update clk source

	SYSCON->MAINCLKPLLSEL = 0;		//	main_clk = main_clk_pre_PLL

	SYSCON->MAINCLKPLLUEN &= ~1;	//	Manual indica primero escribir un 0 para actualizar
	SYSCON->MAINCLKPLLUEN |= 1;		//	Update PLL clk source

	SYSCON->SYSPLLCLKSEL = 0;		//	PLL_clk = FRO

	SYSCON->SYSPLLCLKUEN &= ~1;		//	Manual indica primero escribir un 0 para actualizar
	SYSCON->SYSPLLCLKUEN |= 1;		//	Update PLL clk source

	SYSCON->SYSAHBCLKDIV = 1;		//	División del clk
}
