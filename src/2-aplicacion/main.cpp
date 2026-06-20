#include "hwinit.h"
#include "statistics.h"
#include "mlx90614.h"
#include "ds18b20.h"
#include "wifi.h"
#include "gpio.h"
#include "pint.h"
#include "systick.h"
#include "systimer.h"
#include "digitalinput.h"
#include "adc.h"
#include "dac.h"
#include "uart.h"
#include "i2c.h"
#include "ctimer.h"
#include "onewire.h"
#include "spi.h"

int main(void) {
	HW_init();

    while(1) {

    }
    return 0 ;
}
