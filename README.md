# BabyClon-LPC845
LPC845 code

## PERIPHERAL FEATURES
- GPIO
- PIN INTERRUPT
- SYSTICK TIMER
- DIGITAL INPUTS
- ADC
- UART
- I2C (Tested with MLX90614)
- SPI (Tested with W5500)
- CTIMER
- SCTIMER (Tested with OneWire)
- INMUX
- DMA (Just Software Trigger Tested)\

## APP FEATURES
- STRING
- ONEWIRE (Tested with DS18B20)
- MLX90614
- DS18B20
- WIFI (ESP)
- ETHERNET (W5500)
- SERIAL 7 SEG DISPLAY
- LINEAL / MATRIX KEYBOARD
- LED
- BLUETOOTH (HC-05)\

## FIRMWARE FEATURES
- HARDFAULT ANALYZER\

## NOT FINISHED, missing:
- DAC
- COMPARATOR\

##  NOTES:
1. When trying to use every module at the same time flash size error may appear, configuring Project->Properties->C/C++ Build->Settings->MCU C++ Compiler->Optimization->Optimization Level = Optimize for size (-Os) would fix it. Just check all "volatile" variables are, indeed, volatile (avoiding optimization for size issues).\

2. DMA is not fully tested yet (hardware trigger was not tested with all available sources).