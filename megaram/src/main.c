/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * 
  * SRAM Used in this project: 1024K x 8
  * 
  * Codice Digi-Key 	1450-1067-ND 	1450-1067-ND
  * Quantità disponibile 	230 Spedizione immediata
  * 
  * Fabbricante 	Alliance Memory, Inc. 	PartSearchCore.DksusService3.PidVid
  * Codice produttore 	AS7C38096A-10TIN 	AS7C38096A-10TIN
  * Descrizione 	IC SRAM 8M PARALLEL 44TSOP II 	IC SRAM 8M PARALLEL 44TSOP II
  * Tempi di consegna standard del produttore 	10 settimane 	
  * Descrizione dettagliata 	Memoria CI SRAM -Asincrono 8 Mb (1 M x 8) Parallelo 10ns 44-TSOP2 
  * 
  */

#include <stdio.h>
#include <string.h>
#include "main.h"

typedef enum {
    EXPANSION_TYPE_130XE = 0,      
    EXPANSION_TYPE_192K,           
    EXPANSION_TYPE_256K_RAMBO,     
    EXPANSION_TYPE_320K,           
    EXPANSION_TYPE_320K_COMPYSHOP, 
    EXPANSION_TYPE_576K_MOD,       
    EXPANSION_TYPE_576K_COMPYSHOP,
    EXPANSION_TYPE_1088K_MOD,
    EXPANSION_TYPE_NONE,            // LAST
} t_expansion;

typedef enum {
	WRITERAM = 0,
	READRAM,
} t_command;

#define CONTROL_IN  GPIOI->IDR
#define ADDR_IN     GPIOC->IDR
#define DATA_IN     GPIOA->IDR
#define DATA_OUT    GPIOA->ODR

#define PHI2_RD        (GPIOI->IDR & 0x0001)
#define S5_RD          (GPIOI->IDR & 0x0002)
#define S4_RD          (GPIOI->IDR & 0x0004)
#define S4_AND_S5_HIGH (GPIOI->IDR & 0x0006) == 0x6
#define D1XX_RD        (GPIOI->IDR & 0x0008)

#define PHI2    0x0001
#define S5      0x0002
#define S4      0x0004
#define D1XX    0x0008
#define CCTL    0x0010
#define RW      0x0020

#define SET_DATA_MODE_IN        GPIOA->MODER = 0x00000000;
#define SET_DATA_MODE_OUT       GPIOA->MODER = 0x00005555;

#define GREEN_LED_OFF           LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_0);  /* HIGH ACTIVE */
#define GREEN_LED_ON            LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_0);    /* HIGH ACTIVE */

#define INTERNAL_RAM_DISABLE    LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_5); /* nEXSEL LOW */
#define INTERNAL_RAM_ENABLE     LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_5);   /* nEXSEL HIGH */

#define ATARI_RESET_ASSERT      LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_1); /* RST -> GPIO(B.1) LOW */
#define ATARI_RESET_DEASSERT    LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_1);   /* RST -> GPIO(B.1) HIGH */

#define MEMORY_EXPANSION_TYPE   ((GPIOI->IDR & (0x7 << 6)) >> 6) /* PI9 PC7 PC6 */

/* Default values?? TODO: Read Altirra's Manual... */
static uint8_t PORTB = 0xFF;
static uint8_t PBCTL = 0x00;
static uint8_t EMULATION_TYPE = EXPANSION_TYPE_NONE;

/* We can have up to 1008K! */
#define RAMSIZ (63 * 0x4000)
unsigned char expansion_ram[RAMSIZ] __attribute__((section(".sram")));

enum {
	DBG_ERROR = 0,
	DBG_INFO,
	DBG_VERBOSE,
	DBG_NOISY,
};

static int debuglevel = DBG_INFO;
#define PDEBUG(str)  usart_putstring(str)
/* ANSI Eye-Candy ;-) */
#define ANSI_RED    "\x1b[31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_YELLOW "\x1b[1;33m"
#define ANSI_BLUE   "\x1b[1;34m"
#define ANSI_RESET  "\x1b[0m"
#define DBG_E(str)  PDEBUG(ANSI_RED); PDEBUG(str); PDEBUG(ANSI_RESET);
#define DBG_I(str)  if (debuglevel >= DBG_INFO)    { PDEBUG(ANSI_GREEN);  PDEBUG(str); PDEBUG(ANSI_RESET); }
#define DBG_V(str)  if (debuglevel >= DBG_VERBOSE) { PDEBUG(ANSI_BLUE);   PDEBUG(str); PDEBUG(ANSI_RESET); }
#define DBG_N(str)  if (debuglevel >= DBG_NOISY)   { PDEBUG(ANSI_YELLOW); PDEBUG(str); PDEBUG(ANSI_RESET); }

SRAM_HandleTypeDef hsram1;
void SystemClock_Config(void);
static void gpio_init(void);
static void fmc_sram_init(void);
static void usart_config(int baudrate);

static void usart_write_byte(uint8_t c)
{
	/* Wait for TXE flag to be raised */
	while (!LL_USART_IsActiveFlag_TXE(USART1))
		;;

	LL_USART_ClearFlag_TC(USART1);

	LL_USART_TransmitData8(USART1, c);
}

static void usart_putchar(char c)
{
	// For character we are using from 0 to 127 ASCII
	usart_write_byte((uint8_t) c);
}

static void usart_putstring(char *s)
{
	// Send a string
	while (*s)
	{
		usart_putchar(*s++);
	}
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);

	if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_5)
	{
		Error_Handler();  
	}
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
	LL_PWR_EnableOverDriveMode();
	LL_RCC_HSE_EnableBypass();
	LL_RCC_HSE_Enable();

	/* Wait till HSE is ready */
	while(LL_RCC_HSE_IsReady() != 1)
		;;

	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_4, 180, LL_RCC_PLLP_DIV_2);
	LL_RCC_PLL_Enable();

	/* Wait till PLL is ready */
	while(LL_RCC_PLL_IsReady() != 1)
		;;

	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

	/* Wait till System clock is ready */
	while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
		;;

	/* We are running @ 180Mhz */
	LL_Init1msTick(180000000);
	LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
	LL_SetSystemCoreClock(180000000);
	LL_RCC_SetTIMPrescaler(LL_RCC_TIM_PRESCALER_TWICE);
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void usart_config(int baudrate)
{
	LL_USART_InitTypeDef USART_InitStruct = {0};

	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* Peripheral clock enable */
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	/**USART1 GPIO Configuration  
	PA9   ------> USART1_TX
	PA10   ------> USART1_RX 
	*/
	GPIO_InitStruct.Pin = DEBUG_TX_Pin|DEBUG_RX_Pin;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
	GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
	LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	USART_InitStruct.BaudRate = 115200;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
	LL_USART_Init(USART1, &USART_InitStruct);
	LL_USART_ConfigAsyncMode(USART1);
	LL_USART_Enable(USART1);

}

/* FMC initialization function */
static void fmc_sram_init(void)
{
	FMC_NORSRAM_TimingTypeDef Timing = {0};

	/** Perform the SRAM1 memory initialization sequence
	*/
	hsram1.Instance = FMC_NORSRAM_DEVICE;
	hsram1.Extended = FMC_NORSRAM_EXTENDED_DEVICE;
	/* hsram1.Init */
	hsram1.Init.NSBank = FMC_NORSRAM_BANK1;
	hsram1.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;
	hsram1.Init.MemoryType = FMC_MEMORY_TYPE_SRAM;
	hsram1.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_8;
	hsram1.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;
	hsram1.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
	hsram1.Init.WrapMode = FMC_WRAP_MODE_DISABLE;
	hsram1.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;
	hsram1.Init.WriteOperation = FMC_WRITE_OPERATION_DISABLE;
	hsram1.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
	hsram1.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;
	hsram1.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;
	hsram1.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;
	hsram1.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
	hsram1.Init.PageSize = FMC_PAGE_SIZE_NONE;
	/* Timing */
	Timing.AddressSetupTime = 15;
	Timing.AddressHoldTime = 15;
	Timing.DataSetupTime = 255;
	Timing.BusTurnAroundDuration = 15;
	Timing.CLKDivision = 16;
	Timing.DataLatency = 17;
	Timing.AccessMode = FMC_ACCESS_MODE_A;
	/* ExtTiming */

	if (HAL_SRAM_Init(&hsram1, &Timing, NULL) != HAL_OK)
	{
		Error_Handler( );
	}

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void gpio_init(void)
{
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOI);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);

	/* Low level by default */
	LL_GPIO_ResetOutputPin(GPIOB, LED_Pin | PB6_Pin | PB9_Pin);

	/* ------------------------
	 * CONTROL SIGNALS TO ATARI
	 * ------------------------
	 * High level (not active) by default */
	LL_GPIO_SetOutputPin(GPIOB,
		m_RST_Pin  | m_RD5_Pin   | m_IRQ_Pin  | m_RD4_Pin | m_EXSEL_Pin |
		m_HALT_Pin | m_MPD_Pin);

	/* -------------------------
	 * CONTROL SIGNAL FROM ATARI
	 * -------------------------
	 * mPHI2 = PHASE 2 CLOCK OUTPUT FROM 6502
	 * mS5   = 6502 ACCESSING TO $A000-$BFFF (LOW ACTIVE)
	 * mS4   = 6502 ACCESSING TO $8000-$9FFF (LOW ACTIVE)
	 * mD1XX = 6502 ACCESSING TO PBI $D100-$D1FF (LOW ACTIVE)
	 * mCCTL = 6502 ACCESSING TO $D500-$D5FF (LOW ACTIVE)
	 * mRW   = 6502 R/_W SIGNAL (Read = 1, Write = 0)
	 */
	GPIO_InitStruct.Pin = 
		m_PHI2_Pin | m_S5_Pin   | m_S4_Pin |
		m_D1XX_Pin | m_CCTL_Pin | m_RW_Pin;
	
	/* --------------------------------
	 * CONFIGURATION PIN EMULATION TYPE
	 * -------------------------------- 
	 * ----------------------------------------
	 *    EXPANSION_TYPE_130XE          = 0 0 0
	 *    EXPANSION_TYPE_192K_COMPYSHOP = 0 0 1
	 *    EXPANSION_TYPE_320K_RAMBO     = 0 1 0
	 *    EXPANSION_TYPE_320K_COMPYSHOP = 0 1 1
	 *    EXPANSION_TYPE_576K_MOD       = 1 0 0
	 *    EXPANSION_TYPE_1088K_MOD      = 1 0 1
	 *    UNUSED                        = 1 1 0
	 *    UNUSED                        = 1 1 1
	 * ---------------------------------------- */
	GPIO_InitStruct.Pin |= 
		CONF0_Pin | CONF1_Pin | CONF2_Pin;

	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/* ----------------------------
	 * ADDRESS BUS FROM ATARI 16BIT
	 * ---------------------------- */
	GPIO_InitStruct.Pin =
		m_A0_Pin  | m_A1_Pin  | m_A2_Pin  | m_A3_Pin  |
		m_A4_Pin  | m_A5_Pin  | m_A6_Pin  | m_A7_Pin  |
		m_A8_Pin  | m_A9_Pin  | m_A10_Pin | m_A11_Pin |
		m_A12_Pin | m_A13_Pin | m_A14_Pin | m_A15_Pin;

	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* ----------------------
	 * DATA BUS FROM/TO ATARI
	 * ---------------------- */
	GPIO_InitStruct.Pin =
		m_D0_Pin | m_D1_Pin | m_D2_Pin | m_D3_Pin |
		m_D4_Pin | m_D5_Pin | m_D6_Pin | m_D7_Pin;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* ------------------------
	 * CONTROL SIGNALS TO ATARI
	 * ------------------------
	 * mRST   = RESET 6502 (LOW ACTIVE)
	 * mRD5   = Accessing Cartridge Area $A000-$BFFF (LOW ACTIVE)
	 * mRD4   = Accessing Cartridge Area $8000-$9FFF (LOW ACTIVE)
	 * mIRQ   = External IRQ requested TBD  (LOW ACTIVE)
	 * mEXSEL = Internal RAM Access Disable (LOW ACTIVE)
	 * mHALT  = HALT 6502 (LOW ACTIVE DMA STEAL CYCLES)
	 * mMPD   = INTERNAL MATH PACK ROM DISABLE ($D800-$DFFF 2K)
	 */
	GPIO_InitStruct.Pin = 
		LED_Pin    | m_RST_Pin   | m_RD5_Pin   | m_IRQ_Pin  |
		m_RD4_Pin  | m_EXSEL_Pin | m_HALT_Pin  | m_MPD_Pin;

	/* Miscellaneous pins for STM32F4 bootup */
	GPIO_InitStruct.Pin |= PB6_Pin | PB9_Pin;

	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

static void banner(t_expansion type)
{
	DBG_I("ABEX-MEGARAM BOARD ");
	switch(type)
	{
		case EXPANSION_TYPE_130XE:
			PDEBUG(ANSI_BLUE);
			PDEBUG("130XE 128K EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_192K:
			PDEBUG(ANSI_BLUE);
			PDEBUG("192K (COMPYSHOP) EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_256K_RAMBO:
			PDEBUG(ANSI_BLUE);
			PDEBUG("256K RAMBO EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_320K_COMPYSHOP:
			PDEBUG(ANSI_BLUE);
			PDEBUG("320K COMPYSHOP EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_320K:
			PDEBUG(ANSI_BLUE);
			PDEBUG("320K (RAMBO) EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_576K_MOD:
			PDEBUG(ANSI_BLUE);
			PDEBUG("576K MOD EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_1088K_MOD:
			PDEBUG(ANSI_BLUE);
			PDEBUG("1088K MOD EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		default:
			DBG_E("UNKNOWN EXPANSION TYPE!\r\n");
			break;
	}
	DBG_I("(C) RetroBit Lab 2019 written by Gianluca Renzi <icjtqr@gmail.com>\r\n");
}

/**
  * @brief  Theory of operation:
  * 
  * ATARI Computers when configuring banked memory for ramdisk and other
  * banked based expansion, access to PORTB ($D301) and PBCTL ($D303)
  * to ensure the next access in a specific address window will be
  * in the expansion memory area (0x4000-0x7FFF).
  * 
  * This software intercept the memory access to PORTB to configure
  * the bank scheme with a compatible mode, enables the external memory
  * only if the BIT 2 of the PBCTL is set.
  * 
  * Every access here disable the internal RAM with the EXSEL signal (low)
  * 
  * Basically it is a PORTB/PBCTL emulation.
  * 
  * -------------------------------------------------------------------
  * The STATIC RAM is connected to the Flexible Memory Controller (FMC)
  * -------------------------------------------------------------------
  * 
  * 
  * @retval int
  */

int main(void)
{
	uint16_t addr;
	uint8_t data;
	uint8_t c;

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	gpio_init();
	/* keep ATARI on RESET as soon as possible */
	ATARI_RESET_ASSERT;
	/* Static RAM FMC Configure */
	fmc_sram_init();
	/* Configure USART1 for 115200 8N1 */
	usart_config(115200);

	/* Read CONFx (0,1,2) from DIP-SWITCH */
	EMULATION_TYPE = MEMORY_EXPANSION_TYPE;

	GREEN_LED_ON; // Means board is alive!
	// Reset EXTERNAL RAM
	memset(expansion_ram, 0, RAMSIZ);
	GREEN_LED_OFF; // Now starts with led off. Only accessing external RAM will blink!

	// Start READING FROM DATABUS!
	SET_DATA_MODE_IN

	// Not needed if using FreeRTOS!
	__disable_irq();

	banner(EMULATION_TYPE);

	/* Now it's time to put ATARI OUT OF RESET */
	ATARI_RESET_DEASSERT;

	while (1)
	{
		// Wait for a valid sequence in the bus

		// wait for phi2 high
		while (!((c = CONTROL_IN) & PHI2))
			;

		// Check for address only if there is a valid state of the PHI2
		// on the bus!
		addr = ADDR_IN;

		switch (addr)
		{
			case 0xD303: /* PBCTL Emulation */
				// ATARI CPU Needs to WRITE Data?
				INTERNAL_RAM_DISABLE;
				if (!(c & RW))
				{
					// Now READs the data to be written
					data = DATA_IN;
					// read the data bus on falling edge of phi2
					while (CONTROL_IN & PHI2)
						data = DATA_IN;
					PBCTL = data;
				}
				else
				{
					// ATARI CPU Needs to READ Data
					SET_DATA_MODE_OUT
					DATA_OUT = PBCTL;

					// wait for phi2 low
					while (CONTROL_IN & PHI2)
						;
					SET_DATA_MODE_IN
				}
				INTERNAL_RAM_ENABLE;
				break;

				/* From warerat (from atariage's forum):
				 * 
				 * Be aware for external PIA emulation, you must not only
				 * save the data destined for $D301. You must also intercept
				 * and save writes to PBCTL $D303 bit 2 to enable writing to
				 * $D301 *only* when PBCTL bit 2 = 1.
				 * This further qualifies writes to the output register
				 * at $D301 (which is what you see externally on the PIA),
				 * not the data direction register at $D301 as there are
				 * two registers at the same location.
				 */
			case 0xD301: /* PORTB Emulation */
				// ATARI CPU Needs to WRITE Data?
				INTERNAL_RAM_DISABLE;
				if (!(c & RW))
				{
					// Now READs the data to be written
					data = DATA_IN;
					// read the data bus on falling edge of phi2
					while (CONTROL_IN & PHI2)
						data = DATA_IN;
					/*
					 * Save internally the PORTB data only if it is intended
					 * as memory banked selection register and not if
					 * it is used as data direction control of the PIA PORTB
					 */
					if (PBCTL & (1 << 2))
						PORTB = data;
				}
				else
				{
					// ATARI CPU Needs to READ Data
					SET_DATA_MODE_OUT
					DATA_OUT = PORTB;

					// wait for phi2 low
					while (CONTROL_IN & PHI2)
						;
					SET_DATA_MODE_IN
				}
				INTERNAL_RAM_ENABLE;
				break;

			default:
				/* WINDOW BANKED MEMORY ACCESS 16K */
				if (addr >= 0x4000 && addr < 0x8000)
				{
					uint16_t rel_addr;
					rel_addr = addr - 0x4000; /* Relative address here */

					/*
						D2 Data direction register enable
							0 PORTB [D301] accesses data direction register
							1 PORTB [D301] accesses input and output registers
					*/
					// We can have expansion ram access only if PBCTL bit 2 is 1!
					if (PBCTL & (1 << 2))
					{
						uint8_t bank = 0;
						uint32_t internal_address; /* from 0 to 1048575 (0x100000)*/

						// When accessing the external RAM the internal
						// RAM must be disabled!
						INTERNAL_RAM_DISABLE;

						/* Good! We can access our AtariMegaRAM EXPANSION! */
						GREEN_LED_ON;
						/**
							7  6  5  4  3  2  1  0
							----------------------
							Bank bits:
							128K:		bits 2, 3             (0x0c) >> 2
							192K:		bits 2, 3, 6          (0x4c) ((0x40) >> 2 + (0x0c)) >> 2
							256K Rambo:	bits 2, 3, 5, 6 -- bits 3 and 2 must be LSBs for main memory aliasing (0x6c) ((0x60 >> 5) + (0x0c))
							320K:		bits 2, 3, 5, 6       (0x6c) ((0x60) >> 1 + (0x0c)) >> 2
							320K COMPY:	bits 2, 3, 6, 7       (0xcc) ((0xc0) >> 2 + (0x0c)) >> 2
							576K:		bits 1, 2, 3, 5, 6    (0x6e) ((0x60) >> 1 + (0x0e)) >> 1
							576K COMPY:	bits 1, 2, 3, 6, 7    (0xce) ((0xc0) >> 2 + (0x0e)) >> 1
							1088K:		bits 1, 2, 3, 5, 6, 7 (0xee) ((0xe0) >> 1 + (0x0e)) >> 1

						**/

						/* Calculate which bankno has to be accessed, depending
						 * on emulation of expansion type */
						switch (EMULATION_TYPE)
						{
							case EXPANSION_TYPE_192K:
								bank = (((PORTB & 0x0c) + ((PORTB & 0x40) >> 2)) >> 2);
								break;
							case EXPANSION_TYPE_256K_RAMBO:
								bank = (((PORTB & 0x60) >> 5) + (PORTB & 0x0c));
								break;
							case EXPANSION_TYPE_320K:
								bank = (((PORTB & 0x0c) + ((PORTB & 0x60) >> 1)) >> 2);
								break;
							case EXPANSION_TYPE_320K_COMPYSHOP:
								bank = (((PORTB & 0x0c) + ((PORTB & 0xc0) >> 2)) >> 2);
								break;
							case EXPANSION_TYPE_576K_MOD:
								bank = (((PORTB & 0x0e) + ((PORTB & 0x60) >> 1)) >> 1);
								break;
							case EXPANSION_TYPE_576K_COMPYSHOP:
								bank = (((PORTB & 0x0e) + ((PORTB & 0xc0) >> 2)) >> 1);
								break;
							case EXPANSION_TYPE_1088K_MOD:
								bank = (((PORTB & 0x0e) + ((PORTB & 0xe0) >> 1)) >> 1);
								/* On real hardware we have 63 banks available, not 64
								 * BANKS are from 0 to 63!! */
								if (bank == 63)
									bank--;
								break;
							default:
							case EXPANSION_TYPE_130XE:
								bank = (PORTB & 0x0c) >> 2;
								break;
						}

						internal_address = (bank * 0x4000) + rel_addr;

						// ATARI CPU Needs to WRITE Data?
						if (!(c & RW))
						{
							// Now we can write the data at the desired address
							// stored in the external ram expansion
							// Now READs the data to be written
							data = DATA_IN;
							// read the data bus on falling edge of phi2
							while (CONTROL_IN & PHI2)
								data = DATA_IN;
							expansion_ram[internal_address] = data;
						}
						else
						{
							// CPU Needs to READ Data from external memory
							SET_DATA_MODE_OUT
							DATA_OUT = expansion_ram[internal_address];
							// wait for phi2 low
							while (CONTROL_IN & PHI2)
								;
							SET_DATA_MODE_IN
						}

						GREEN_LED_OFF;
						INTERNAL_RAM_ENABLE;
					}
					else
					{
						// PBCTL Bit 2 is not set, so do nothing with
						// external memory
					}
				}
				else
				{
					// Other address needed, DO NOTHING
				}
				break;
		}
	}

	return 0; // NEVERREACHED
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
	for (;;)
		;;
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
	/* User can add his own implementation to report the file name and line number,
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	for (;;)
		;;
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
