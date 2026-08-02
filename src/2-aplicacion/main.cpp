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
//#include "uart.h"
#include "i2c.h"
#include "ctimer.h"
#include "onewire.h"
#include "spi.h"
#include "eth.h"

int main(void) {
	HW_init();

	Spi spiMaster(0, 23, 0, 26, 0, 21, Spi::SPI_NUMBER_0, 1000000);
	Eth eth(0, 22, spiMaster);

	uint8_t ip[4] = {192, 168, 1, 50};
	uint8_t gateway[4] = {192, 168, 1, 1};
	uint8_t subnet[4] = {255, 255, 255, 0};
	uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x33};

    for(volatile int i = 0; i < 500000; i++);

    eth.setMACAndWait(mac);
    eth.setGatewayAndWait(gateway);
    eth.setSubnetAndWait(subnet);
    eth.setIPAndWait(ip);

    uint8_t ip2[4];
    uint8_t gateway2[4];
    uint8_t subnet2[4];
    uint8_t mac2[6];

    eth.readIP(ip2);
    eth.readGateway(gateway2);
    eth.readSubnet(subnet2);
    eth.readMAC(mac2);

    while(1);

    return 0 ;
}
