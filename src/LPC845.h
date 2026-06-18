#ifndef LPC845_H_
#define LPC845_H_

#include <stdint.h>

typedef struct {
	volatile uint32_t SYSMEMREMAP;                       /**< System Remap register, offset: 0x0 */
       	   	 uint8_t RESERVED_0[4];
    volatile uint32_t SYSPLLCTRL;                        /**< PLL control, offset: 0x8 */
    volatile uint32_t SYSPLLSTAT;                        /**< PLL status, offset: 0xC */
       	   	 uint8_t RESERVED_1[16];
    volatile uint32_t SYSOSCCTRL;                        /**< system oscillator control, offset: 0x20 */
    volatile uint32_t WDTOSCCTRL;                        /**< Watchdog oscillator control, offset: 0x24 */
    volatile uint32_t FROOSCCTRL;                        /**< FRO oscillator control, offset: 0x28 */
       	   	 uint8_t RESERVED_2[4];
    volatile uint32_t FRODIRECTCLKUEN;                   /**< FRO direct clock source update enable register, offset: 0x30 */
       	   	 uint8_t RESERVED_3[4];
    volatile uint32_t SYSRSTSTAT;                        /**< System reset status register, offset: 0x38 */
       	   	 uint8_t RESERVED_4[4];
    volatile uint32_t SYSPLLCLKSEL;                      /**< System PLL clock source select register, offset: 0x40 */
    volatile uint32_t SYSPLLCLKUEN;                      /**< System PLL clock source update enable register, offset: 0x44 */
    volatile uint32_t MAINCLKPLLSEL;                     /**< Main clock source select register, offset: 0x48 */
    volatile uint32_t MAINCLKPLLUEN;                     /**< Main clock source update enable register, offset: 0x4C */
    volatile uint32_t MAINCLKSEL;                        /**< Main clock source select register, offset: 0x50 */
    volatile uint32_t MAINCLKUEN;                        /**< Main clock source update enable register, offset: 0x54 */
    volatile uint32_t SYSAHBCLKDIV;                      /**< System clock divider register, offset: 0x58 */
       	   	 uint8_t RESERVED_5[4];
    volatile uint32_t CAPTCLKSEL;                        /**< CAPT clock source select register, offset: 0x60 */
    volatile uint32_t ADCCLKSEL;                         /**< ADC clock source select register, offset: 0x64 */
    volatile uint32_t ADCCLKDIV;                         /**< ADC clock divider register, offset: 0x68 */
    volatile uint32_t SCTCLKSEL;                         /**< SCT clock source select register, offset: 0x6C */
    volatile uint32_t SCTCLKDIV;                         /**< SCT clock divider register, offset: 0x70 */
    volatile uint32_t EXTCLKSEL;                         /**< external clock source select register, offset: 0x74 */
       	   	 uint8_t RESERVED_6[8];
    volatile uint32_t SYSAHBCLKCTRL0;                    /**< System clock group 0 control register, offset: 0x80 */
    volatile uint32_t SYSAHBCLKCTRL1;                    /**< System clock group 1 control register, offset: 0x84 */
    volatile uint32_t PRESETCTRL0;                       /**< Peripheral reset group 0 control register, offset: 0x88 */
    volatile uint32_t PRESETCTRL1;                       /**< Peripheral reset group 1 control register, offset: 0x8C */
    volatile uint32_t FCLKSEL[11];                       /**< peripheral clock source select register. FCLK0SEL~FCLK4SEL are for UART0~UART4 clock source select register. FCLK5SEL~FCLK8SEL are for I2C0~I2C3 clock source select register. FCLK9SEL~FCLK10SEL are for SPI0~SPI1 clock source select register., array offset: 0x90, array step: 0x4 */
       	   	 uint8_t RESERVED_7[20];
    struct {                                         /* offset: 0xD0, array step: 0x10 */
       	   	volatile uint32_t FRGDIV;                            /**< fractional generator N divider value register, array offset: 0xD0, array step: 0x10 */
       	   	volatile uint32_t FRGMULT;                           /**< fractional generator N multiplier value register, array offset: 0xD4, array step: 0x10 */
       	   	volatile uint32_t FRGCLKSEL;                         /**< FRG N clock source select register, array offset: 0xD8, array step: 0x10 */
       	   			 uint8_t RESERVED_0[4];
    } FRG[2];
    volatile uint32_t CLKOUTSEL;                         /**< CLKOUT clock source select register, offset: 0xF0 */
    volatile uint32_t CLKOUTDIV;                         /**< CLKOUT clock divider registers, offset: 0xF4 */
       	   	 uint8_t RESERVED_8[4];
    volatile uint32_t EXTTRACECMD;                       /**< External trace buffer command register, offset: 0xFC */
    volatile uint32_t PIOPORCAP[2];                      /**< POR captured PIO N status register(PIO0 has 32 PIOs, PIO1 has 22 PIOs), array offset: 0x100, array step: 0x4 */
       	   	 uint8_t RESERVED_9[44];
    volatile uint32_t IOCONCLKDIV6;                      /**< Peripheral clock 6 to the IOCON block for programmable glitch filter, offset: 0x134 */
    volatile uint32_t IOCONCLKDIV5;                      /**< Peripheral clock 6 to the IOCON block for programmable glitch filter, offset: 0x138 */
    volatile uint32_t IOCONCLKDIV4;                      /**< Peripheral clock 4 to the IOCON block for programmable glitch filter, offset: 0x13C */
    volatile uint32_t IOCONCLKDIV3;                      /**< Peripheral clock 3 to the IOCON block for programmable glitch filter, offset: 0x140 */
    volatile uint32_t IOCONCLKDIV2;                      /**< Peripheral clock 2 to the IOCON block for programmable glitch filter, offset: 0x144 */
    volatile uint32_t IOCONCLKDIV1;                      /**< Peripheral clock 1 to the IOCON block for programmable glitch filter, offset: 0x148 */
    volatile uint32_t IOCONCLKDIV0;                      /**< Peripheral clock 0 to the IOCON block for programmable glitch filter, offset: 0x14C */
    volatile uint32_t BODCTRL;                           /**< BOD control register, offset: 0x150 */
    volatile uint32_t SYSTCKCAL;                         /**< System tick timer calibration register, offset: 0x154 */
       	   	 uint8_t RESERVED_10[24];
    volatile uint32_t IRQLATENCY;                        /**< IRQ latency register, offset: 0x170 */
    volatile uint32_t NMISRC;                            /**< NMI source selection register, offset: 0x174 */
    volatile uint32_t PINTSEL[8];                        /**< Pin interrupt select registers N, array offset: 0x178, array step: 0x4 */
       	   	 uint8_t RESERVED_11[108];
    volatile uint32_t STARTERP0;                         /**< Start logic 0 pin wake-up enable register 0, offset: 0x204 */
       	   	 uint8_t RESERVED_12[12];
    volatile uint32_t STARTERP1;                         /**< Start logic 0 pin wake-up enable register 1, offset: 0x214 */
       	   	 uint8_t RESERVED_13[24];
    volatile uint32_t PDSLEEPCFG;                        /**< Deep-sleep configuration register, offset: 0x230 */
    volatile uint32_t PDAWAKECFG;                        /**< Wake-up configuration register, offset: 0x234 */
    volatile uint32_t PDRUNCFG;                          /**< Power configuration register, offset: 0x238 */
       	   	 uint8_t RESERVED_14[444];
    volatile  uint32_t DEVICE_ID;                         /**< Part ID register, offset: 0x3F8 */
} SYSCON_Type;

#define SYSCON_BASE           (0x40048000u)
#define SYSCON                ((SYSCON_Type *) SYSCON_BASE)

typedef struct{
  volatile uint32_t PIO[56];                           /**< Digital I/O control for pins PIO0_17..Digital I/O control for pins PIO1_10, array offset: 0x0, array step: 0x4 */
} IOCON_Type;
/* IOCON - Peripheral instance base addresses */
/** Peripheral IOCON base address */
#define IOCON_BASE                               (0x40044000u)
/** Peripheral IOCON base pointer */
#define IOCON                                    ((IOCON_Type *)IOCON_BASE)
/** Array initializer of IOCON peripheral base addresses */
#define IOCON_BASE_ADDRS                         { IOCON_BASE }
/** Array initializer of IOCON peripheral base pointers */
#define IOCON_BASE_PTRS                          { IOCON }

typedef struct{
	  volatile uint8_t B[2][32];
	       uint8_t RESERVED_0[4032];
	  volatile uint32_t W[2][32];
	       uint8_t RESERVED_1[3840];
	  volatile uint32_t DIR[2];
	       uint8_t RESERVED_2[120];
	  volatile uint32_t MASK[2];
	       uint8_t RESERVED_3[120];
	  volatile uint32_t PIN[2];
	       uint8_t RESERVED_4[120];
	  volatile uint32_t MPIN[2];
	       uint8_t RESERVED_5[120];
	  volatile uint32_t SET[2];
	       uint8_t RESERVED_6[120];
	  volatile  uint32_t CLR[2];
	       uint8_t RESERVED_7[120];
	  volatile  uint32_t NOT[2];
	       uint8_t RESERVED_8[120];
	  volatile  uint32_t DIRSET[2];
	       uint8_t RESERVED_9[120];
	  volatile  uint32_t DIRCLR[2];
	       uint8_t RESERVED_10[120];
	  volatile  uint32_t DIRNOT[2];
} GPIO_Type;

#define GPIO ((GPIO_Type *) 0xA0000000u)

typedef struct {
  volatile uint32_t ISEL;                              /**< Pin Interrupt Mode register, offset: 0x0 */
  volatile uint32_t IENR;                              /**< Pin interrupt level or rising edge interrupt enable register, offset: 0x4 */
  volatile uint32_t SIENR;                             /**< Pin interrupt level or rising edge interrupt set register, offset: 0x8 */
  volatile uint32_t CIENR;                             /**< Pin interrupt level (rising edge interrupt) clear register, offset: 0xC */
  volatile uint32_t IENF;                              /**< Pin interrupt active level or falling edge interrupt enable register, offset: 0x10 */
  volatile uint32_t SIENF;                             /**< Pin interrupt active level or falling edge interrupt set register, offset: 0x14 */
  volatile uint32_t CIENF;                             /**< Pin interrupt active level or falling edge interrupt clear register, offset: 0x18 */
  volatile uint32_t RISE;                              /**< Pin interrupt rising edge register, offset: 0x1C */
  volatile uint32_t FALL;                              /**< Pin interrupt falling edge register, offset: 0x20 */
  volatile uint32_t IST;                               /**< Pin interrupt status register, offset: 0x24 */
  volatile uint32_t PMCTRL;                            /**< Pattern match interrupt control register, offset: 0x28 */
  volatile uint32_t PMSRC;                             /**< Pattern match interrupt bit-slice source register, offset: 0x2C */
  volatile uint32_t PMCFG;                             /**< Pattern match interrupt bit slice configuration register, offset: 0x30 */
} PINT_Type;

#define PINT                                     ((PINT_Type *) 0xA0004000u)

#define NUMBER_OF_INT_VECTORS 48                 /**< Number of interrupts in the Vector table */

typedef enum IRQn {
  /* Auxiliary constants */
  NotAvail_IRQn                = -128,             /**< Not available device specific interrupt */

  /* Core interrupts */
  NonMaskableInt_IRQn          = -14,              /**< Non Maskable Interrupt */
  HardFault_IRQn               = -13,              /**< Cortex-M0 SV Hard Fault Interrupt */
  SVCall_IRQn                  = -5,               /**< Cortex-M0 SV Call Interrupt */
  PendSV_IRQn                  = -2,               /**< Cortex-M0 Pend SV Interrupt */
  SysTick_IRQn                 = -1,               /**< Cortex-M0 System Tick Interrupt */

  /* Device specific interrupts */
  SPI0_IRQn                    = 0,                /**< SPI0 interrupt */
  SPI1_IRQn                    = 1,                /**< SPI1 interrupt */
  DAC0_IRQn                    = 2,                /**< DAC0 interrupt */
  USART0_IRQn                  = 3,                /**< USART0 interrupt */
  USART1_IRQn                  = 4,                /**< USART1 interrupt */
  USART2_IRQn                  = 5,                /**< USART2 interrupt */
  Reserved22_IRQn              = 6,                /**< Reserved interrupt */
  I2C1_IRQn                    = 7,                /**< I2C1 interrupt */
  I2C0_IRQn                    = 8,                /**< I2C0 interrupt */
  SCT0_IRQn                    = 9,                /**< State configurable timer interrupt */
  MRT0_IRQn                    = 10,               /**< Multi-rate timer interrupt */
  CMP_CAPT_IRQn                = 11,               /**< Analog comparator interrupt or Capacitive Touch interrupt */
  WDT_IRQn                     = 12,               /**< Windowed watchdog timer interrupt */
  BOD_IRQn                     = 13,               /**< BOD interrupts */
  FLASH_IRQn                   = 14,               /**< flash interrupt */
  WKT_IRQn                     = 15,               /**< Self-wake-up timer interrupt */
  ADC0_SEQA_IRQn               = 16,               /**< ADC0 sequence A completion. */
  ADC0_SEQB_IRQn               = 17,               /**< ADC0 sequence B completion. */
  ADC0_THCMP_IRQn              = 18,               /**< ADC0 threshold compare and error. */
  ADC0_OVR_IRQn                = 19,               /**< ADC0 overrun */
  DMA0_IRQn                    = 20,               /**< DMA0 interrupt */
  I2C2_IRQn                    = 21,               /**< I2C2 interrupt */
  I2C3_IRQn                    = 22,               /**< I2C3 interrupt */
  CTIMER0_IRQn                 = 23,               /**< Timer interrupt */
  PIN_INT0_IRQn                = 24,               /**< Pin interrupt 0 or pattern match engine slice 0 interrupt */
  PIN_INT1_IRQn                = 25,               /**< Pin interrupt 1 or pattern match engine slice 1 interrupt */
  PIN_INT2_IRQn                = 26,               /**< Pin interrupt 2 or pattern match engine slice 2 interrupt */
  PIN_INT3_IRQn                = 27,               /**< Pin interrupt 3 or pattern match engine slice 3 interrupt */
  PIN_INT4_IRQn                = 28,               /**< Pin interrupt 4 or pattern match engine slice 4 interrupt */
  PIN_INT5_DAC1_IRQn           = 29,               /**< Pin interrupt 5 or pattern match engine slice 5 interrupt or DAC1 interrupt */
  PIN_INT6_USART3_IRQn         = 30,               /**< Pin interrupt 6 or pattern match engine slice 6 interrupt or UART3 interrupt */
  PIN_INT7_USART4_IRQn         = 31                /**< Pin interrupt 7 or pattern match engine slice 7 interrupt or UART4 interrupt */
} IRQn_Type;

typedef struct
{
  volatile uint32_t ISER[1U];               /*!< Offset: 0x000 (R/W)  Interrupt Set Enable Register */
        uint32_t RESERVED0[31U];
  volatile uint32_t ICER[1U];               /*!< Offset: 0x080 (R/W)  Interrupt Clear Enable Register */
        uint32_t RSERVED1[31U];
  volatile uint32_t ISPR[1U];               /*!< Offset: 0x100 (R/W)  Interrupt Set Pending Register */
        uint32_t RESERVED2[31U];
  volatile uint32_t RESERVED3[31U];
        uint32_t RESERVED4[64U];
  volatile uint32_t IP[8U];                 /*!< Offset: 0x300 (R/W)  Interrupt Priority Register */
}NVIC_Type;

#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address */
#define NVIC_BASE           (SCS_BASE +  0x0100UL)                    /*!< NVIC Base Address */
#define NVIC                ((NVIC_Type *) NVIC_BASE)   			  /*!< NVIC configuration struct */

typedef struct{
  volatile uint32_t CSR;     /*!< Offset: 0x000 (R/W)  SysTick Control and Status Register */
  volatile uint32_t RVR;   	 /*!< Offset: 0x004 (R/W)  SysTick Reload Value Register */
  volatile uint32_t CVR;     /*!< Offset: 0x008 (R/W)  SysTick Current Value Register */
  volatile uint32_t CALIB;   /*!< Offset: 0x00C (R/ )  SysTick Calibration Register */
} SysTick_t;

#define SYSTICK    ( (SysTick_t *) 0xE000E010UL)   /*!< SysTick configuration struct */

#define FREQ_CLOCK	(30000000UL)


typedef struct {
  volatile uint32_t PINASSIGN[15];
  	  	   uint32_t RESERVED_0[97];
  volatile uint32_t PINENABLE0;
  volatile uint32_t PINENABLE1;
} SWM_Type;

#define SWM ((SWM_Type *) 0x4000C000u)

typedef struct{
	volatile uint32_t CFG;
	volatile uint32_t CTL;
	volatile uint32_t STAT;
    volatile uint32_t INTENSET;
    volatile uint32_t INTENCLR;
    volatile uint32_t RXDAT;
    volatile uint32_t RXDATSTAT;
    volatile uint32_t TXDAT;
    volatile uint32_t BRG;
    volatile uint32_t INTSTAT;
    volatile uint32_t OSR;
    volatile uint32_t ADDR;
} UART_Type;

#define UART0 ((UART_Type *) 0x40064000u)
#define UART1 ((UART_Type *) 0x40068000u)
#define UART2 ((UART_Type *) 0x4006C000u)
#define UART3 ((UART_Type *) 0x40070000u)
#define UART4 ((UART_Type *) 0x40074000u)

typedef struct{
	volatile uint32_t CFG;
	volatile uint32_t STAT;
	volatile uint32_t INTENSET;
    volatile uint32_t INTENCLR;
    volatile uint32_t TIMEOUT;
    volatile uint32_t CLKDIV;
    volatile uint32_t INTSTAT;
    		 uint32_t RESERVED_0[1];
    volatile uint32_t MSTCTL;
    volatile uint32_t MSTTIME;
    volatile uint32_t MSTDAT;
    		 uint32_t RESERVED_1[5];
    volatile uint32_t SLVCTL;
    volatile uint32_t SLVDAT;
    volatile uint32_t SLVADR[4];
    volatile uint32_t SLVQUAL0;
    		 uint32_t RESERVED_2[9];
    volatile uint32_t MONRXDAT;
} I2C_Type;

#define I2C0 ((I2C_Type *) 0x40050000u)
#define I2C1 ((I2C_Type *) 0x40054000u)
#define I2C2 ((I2C_Type *) 0x40030000u)
#define I2C3 ((I2C_Type *) 0x40034000U)

typedef struct{
	volatile uint32_t IR;			//	Clear interrupt
	volatile uint32_t TCR;			//	Control register
	volatile uint32_t TC;			//	Timer counter register
	volatile uint32_t PR;			//	Prescale register
	volatile uint32_t PC;			//	Prescale Counter Value
	volatile uint32_t MCR;			//	Match Control register
	volatile uint32_t MR[4];		//	Match register (if MR == TC -> interrupt)
	volatile uint32_t CCR;			//	Capture Control register
	volatile const uint32_t CR[4];	//	Capture register
	volatile uint32_t EMR;			//	External Match Register
			 uint32_t RESERVED_0[12];
	volatile uint32_t CTCR;			//	Count Control register (selects between Timer & Counter mode)
	volatile uint32_t PWMC;			//	PWM Control register
	volatile uint32_t MSR[4];		//	Match Shadow register
} CTIMER_Type;

#define CTIMER0 ((CTIMER_Type *) 0x40038000u)

#endif /* LPC845_H_ */
