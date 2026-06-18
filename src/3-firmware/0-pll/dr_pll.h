#ifndef DR_PLL_H_
#define DR_PLL_H_

#include "LPC845.h"

#define FRO_SET_FRECUENCY_FUNC (void (*)(int))(0x0F0026F5U)

void PLL_init();

#endif /* DR_PLL_H_ */
