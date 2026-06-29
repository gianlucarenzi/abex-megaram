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
#include "syscall.h"
#include "debug.h"
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

#define CONTROL_IN  GPIOI->IDR
#define ADDR_IN     GPIOC->IDR
#define DATA_IN     GPIOA->IDR
#define DATA_OUT    GPIOA->ODR

#define PHI2    0x0001
#define S5      0x0002
#define S4      0x0004
#define D1XX    0x0008
#define CCTL    0x0010
#define RW      0x0020

/* Only PA0-PA7 (D0-D7) change mode; PA8-PA15 (USART1 PA9/PA10) are preserved */
static uint32_t gpioa_moder_in;
static uint32_t gpioa_moder_out;

#define SET_DATA_MODE_IN        GPIOA->MODER = gpioa_moder_in;
#define SET_DATA_MODE_OUT       GPIOA->MODER = gpioa_moder_out;

#define GREEN_LED_OFF           GPIOB->BSRR = (GPIO_PIN_0 << 16);  /* HIGH ACTIVE */
#define GREEN_LED_ON            GPIOB->BSRR = GPIO_PIN_0;    /* HIGH ACTIVE */

#define INTERNAL_RAM_DISABLE    GPIOB->BSRR = (GPIO_PIN_5 << 16); /* nEXSEL LOW */
#define INTERNAL_RAM_ENABLE     GPIOB->BSRR = GPIO_PIN_5;   /* nEXSEL HIGH */

#define ATARI_RESET_ASSERT      GPIOB->BSRR = (GPIO_PIN_1 << 16); /* RST -> GPIO(B.1) LOW */
#define ATARI_RESET_DEASSERT    GPIOB->BSRR = GPIO_PIN_1;   /* RST -> GPIO(B.1) HIGH */

#define MEMORY_EXPANSION_TYPE   ((GPIOI->IDR & (0x7 << 6)) >> 6) /* PI9 PC7 PC6 */

/* Default values?? TODO: Read Altirra's Manual... */
static uint8_t PORTB = 0xFF;
static uint8_t PBCTL = 0x00;
static uint8_t EMULATION_TYPE = EXPANSION_TYPE_NONE;

/* 64 banks x 16K = 1024K external; 1024K + 64K internal = 1088K total */
#define RAMSIZ (64 * 0x4000)
unsigned char expansion_ram[RAMSIZ] __attribute__((section(".sram")));

/* Bank lookup table: bank_lut[PORTB] -> bank number, 0xFF = no external access.
 * Placed in CCMRAM (zero wait states, directly connected to Cortex-M4 core). */
static uint8_t bank_lut[256] __attribute__((section(".ccmram")));

static int debuglevel = DBG_INFO;

SRAM_HandleTypeDef hsram1;
void SystemClock_Config(void);
static void gpio_init(void);
static void fmc_sram_init(void);
static void usart_config(int baudrate);
static void init_bank_lut(t_expansion type);

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);

	if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_5)
	{
		Error_Handler("SystemClock_Config");
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
	hsram1.Init.WriteOperation = FMC_WRITE_OPERATION_ENABLE;
	hsram1.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
	hsram1.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;
	hsram1.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;
	hsram1.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;
	hsram1.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
	hsram1.Init.PageSize = FMC_PAGE_SIZE_NONE;
	/* Timing for AS7C38096A-10TIN (tAA=10ns, tRC=10ns) @ 180MHz (HCLK=5.55ns/cycle)
	 * ADDSET=1 -> 2 cycles = 11.1ns (tASU satisfied)
	 * DATAST=2 -> 3 cycles = 16.7ns > tAA=10ns
	 * BUSTURN=1 -> 2 cycles turnaround between accesses */
	Timing.AddressSetupTime = 1;
	Timing.AddressHoldTime = 1;
	Timing.DataSetupTime = 2;
	Timing.BusTurnAroundDuration = 1;
	Timing.CLKDivision = 2;
	Timing.DataLatency = 2;
	Timing.AccessMode = FMC_ACCESS_MODE_A;
	/* ExtTiming */

	if (HAL_SRAM_Init(&hsram1, &Timing, NULL) != HAL_OK)
	{
		Error_Handler("fmc_sram_init");
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
	 *    EXPANSION_TYPE_192K           = 0 0 1
	 *    EXPANSION_TYPE_256K_RAMBO     = 0 1 0
	 *    EXPANSION_TYPE_320K           = 0 1 1
	 *    EXPANSION_TYPE_320K_COMPYSHOP = 1 0 0
	 *    EXPANSION_TYPE_576K_MOD       = 1 0 1
	 *    EXPANSION_TYPE_576K_COMPYSHOP = 1 1 0
	 *    EXPANSION_TYPE_1088K_MOD      = 1 1 1
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
			printf(ANSI_BLUE "130XE 128K EXPANSION\r\n" ANSI_RESET);
			break;
		case EXPANSION_TYPE_192K:
			printf(ANSI_BLUE "192K (COMPYSHOP) EXPANSION\r\n" ANSI_RESET);
			break;
		case EXPANSION_TYPE_256K_RAMBO:
			printf(ANSI_BLUE "256K RAMBO EXPANSION\r\n" ANSI_RESET);
			break;
		case EXPANSION_TYPE_320K_COMPYSHOP:
			printf(ANSI_BLUE "320K COMPYSHOP EXPANSION\r\n" ANSI_RESET);
			break;
		case EXPANSION_TYPE_320K:
			printf(ANSI_BLUE "320K (RAMBO) EXPANSION\r\n" ANSI_RESET);
			break;
		case EXPANSION_TYPE_576K_MOD:
			printf(ANSI_BLUE "576K MOD EXPANSION\r\n" ANSI_RESET);
			break;
		case EXPANSION_TYPE_576K_COMPYSHOP:
			printf(ANSI_BLUE "576K COMPYSHOP EXPANSION\r\n" ANSI_RESET);
			break;
		case EXPANSION_TYPE_1088K_MOD:
			printf(ANSI_BLUE "1088K MOD EXPANSION\r\n" ANSI_RESET);
			break;
		default:
			printf(ANSI_RED "UNKNOWN EXPANSION TYPE!\r\n" ANSI_RESET);
			break;
	}
	DBG_I("(C) RetroBit Lab 2019 written by Gianluca Renzi <icjtqr@gmail.com>\r\n");
}

static void init_bank_lut(t_expansion type)
{
	for (int i = 0; i < 256; i++)
	{
		uint8_t p = (uint8_t)i;
		uint8_t b;
		switch (type)
		{
			case EXPANSION_TYPE_130XE:
				b = ((p & 0x30) != 0x30) ? ((p & 0x0c) >> 2) : 0xFF;
				break;
			case EXPANSION_TYPE_192K:
				b = ((p & 0x30) != 0x30) ?
					(((p & 0x0c) + ((p & 0x40) >> 2)) >> 2) : 0xFF;
				break;
			case EXPANSION_TYPE_256K_RAMBO: {
				uint8_t bk = (((p & 0x0c) + ((p & 0x60) >> 1)) >> 2);
				b = (p & 0x10) ? (bk & 0x3) : bk;
				break;
			}
			case EXPANSION_TYPE_320K:
				b = (!(p & 0x10)) ?
					(((p & 0x0c) + ((p & 0x60) >> 1)) >> 2) : 0xFF;
				break;
			case EXPANSION_TYPE_320K_COMPYSHOP:
				b = ((p & 0x30) != 0x30) ?
					(((p & 0x0c) + ((p & 0xc0) >> 2)) >> 2) : 0xFF;
				break;
			case EXPANSION_TYPE_576K_MOD:
				b = (!(p & 0x10)) ?
					(((p & 0x0e) + ((p & 0x60) >> 1)) >> 1) : 0xFF;
				break;
			case EXPANSION_TYPE_576K_COMPYSHOP:
				b = (!(p & 0x10)) ?
					(((p & 0x0e) + ((p & 0xc0) >> 2)) >> 1) : 0xFF;
				break;
			case EXPANSION_TYPE_1088K_MOD:
				b = (!(p & 0x10)) ?
					(((p & 0x0e) + ((p & 0xe0) >> 1)) >> 1) : 0xFF;
				break;
			default:
				b = 0xFF;
				break;
		}
		bank_lut[i] = b;
	}
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
	uint8_t watchdog = 0xff;

	register GPIO_TypeDef *gpioi = GPIOI;
	register GPIO_TypeDef *gpioc = GPIOC;
	register GPIO_TypeDef *gpioa = GPIOA;

	_write_ready(SYSCALL_NOTREADY); // printf is not functional here
 
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	gpio_init();
	/* keep ATARI on RESET as soon as possible */
	ATARI_RESET_ASSERT;

	/* Configure USART1 for 115200 8N1 */
	usart_config(115200);
	_write_ready(SYSCALL_READY); // printf is functional from now on...

	/* Static RAM FMC Configure */
	fmc_sram_init();

	/* Read CONFx (0,1,2) from DIP-SWITCH */
	EMULATION_TYPE = MEMORY_EXPANSION_TYPE;
	init_bank_lut(EMULATION_TYPE);

	GREEN_LED_ON; // Means board is alive!
	// Reset EXTERNAL RAM
	memset(expansion_ram, 0, RAMSIZ);

	/* Initialize precalculated MODER values for fast PA0-PA7 switching */
	gpioa_moder_in = (GPIOA->MODER & 0xFFFF0000);
	gpioa_moder_out = gpioa_moder_in | 0x00005555;

	// Start READING FROM DATABUS!
	SET_DATA_MODE_IN

	// Not needed if using FreeRTOS!
	__disable_irq();

	banner(EMULATION_TYPE);

	mdelay(500);

	GREEN_LED_OFF; // Now starts with led off. Only accessing external RAM will blink!

	/* Now it's time to put ATARI OUT OF RESET */
	ATARI_RESET_DEASSERT;

	while (1)
	{
		// Wait for a valid sequence in the bus

		/* Capture control signals atomically on first PHI2-high sample */
		while (!((c = gpioi->IDR) & PHI2))
			;
		addr = gpioc->IDR;

		switch (addr)
		{
			case 0xD303: /* PBCTL Emulation */
				/* Good! We can access our AtariMegaRAM EXPANSION! */
				GREEN_LED_ON;
				// ATARI CPU Needs to WRITE Data?
				INTERNAL_RAM_DISABLE;
				if (!(c & RW))
				{
					// read the data bus on falling edge of phi2
					while (gpioi->IDR & PHI2)
						;
					data = gpioa->IDR;
					PBCTL = data;
				}
				else
				{
					// ATARI CPU Needs to READ Data
					SET_DATA_MODE_OUT
					gpioa->ODR = PBCTL;

					// wait for phi2 low
					while (gpioi->IDR & PHI2)
						;
					SET_DATA_MODE_IN
				}
				INTERNAL_RAM_ENABLE;
				GREEN_LED_OFF;
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
				/* Good! We can access our AtariMegaRAM EXPANSION! */
				GREEN_LED_ON;
				// ATARI CPU Needs to WRITE Data?
				INTERNAL_RAM_DISABLE;
				if (!(c & RW))
				{
					// read the data bus on falling edge of phi2
					while (gpioi->IDR & PHI2)
						;
					data = gpioa->IDR;
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
					gpioa->ODR = PORTB;

					// wait for phi2 low
					while (gpioi->IDR & PHI2)
						;
					SET_DATA_MODE_IN
				}
				INTERNAL_RAM_ENABLE;
				GREEN_LED_OFF;
				break;

			case 0xD1FE:
				/* How to change by runtime the compatibility
				 * mode...
				 * 
				 * EXPANSION_TYPE_1088K_MOD = 7
				 * 
				 * value:  db 0x07
				 *         ldx #$ff
				 *         lda value
				 * enable: sta $d1fe
				 *         dex
				 *         bne enable
				 * # Now reset occurs in the next milliseconds!
				 * loop:   jmp loop
				 */
				// ATARI CPU Needs to WRITE Data (MEMORY EXPANSION CONFIG) ?
				if (!(c & RW))
				{
					watchdog--;
					if (watchdog == 0)
					{
						// Now we can write the data at the desired address
						// stored in the external ram expansion
						// read the data bus on falling edge of phi2
						while (gpioi->IDR & PHI2)
							;
						data = gpioa->IDR;
						EMULATION_TYPE = data;
						GREEN_LED_ON;
						ATARI_RESET_ASSERT;
						banner(EMULATION_TYPE);
						init_bank_lut(EMULATION_TYPE);
						PORTB = 0xff; // To be checked!!
						PBCTL = 0x00; // To be checked!!
						mdelay(500); // Assert Reset for at least 500 millis
						GREEN_LED_OFF;
						ATARI_RESET_DEASSERT;
						watchdog = 0xff;
					}
				}
				else
				{
					// Do nothing, reload the watchdog timer...
					watchdog = 0xff;
				}
				break;

			default:
				/* WINDOW BANKED MEMORY ACCESS 16K ($4000-$7FFF) */
				if (addr >= 0x4000 && addr < 0x8000)
				{
					/* Expansion RAM accessible only when PBCTL bit 2 (DDR) is set */
					if (PBCTL & (1 << 2))
					{
						/* bank_lut[PORTB]: precomputed bank index, 0xFF = no access */
						uint8_t bank = bank_lut[PORTB];

						if (bank != 0xFF)
						{
							uint32_t internal_address =
								((uint32_t)bank << 14) | (addr - 0x4000);
							INTERNAL_RAM_DISABLE;
							GREEN_LED_ON;

							if (!(c & RW))
							{
								while (gpioi->IDR & PHI2)
									;
								data = gpioa->IDR;
								expansion_ram[internal_address] = data;
							}
							else
							{
								SET_DATA_MODE_OUT
								gpioa->ODR = expansion_ram[internal_address];
								while (gpioi->IDR & PHI2)
									;
								SET_DATA_MODE_IN
							}
							GREEN_LED_OFF;
						}
						INTERNAL_RAM_ENABLE;
					}
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
void Error_Handler(const char *c)
{
	DBG_E("%s\n\r", c);
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
	DBG_E("Error! Wrong parameters value: file %s on line %d\r\n", file, line);
	for (;;)
		;;
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
