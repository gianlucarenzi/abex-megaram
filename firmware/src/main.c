/*
 * --------------------------------------
 * UNOCart Firmware (c)2016 Robin Edwards
 * --------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include "defines.h"
#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

#ifdef STM32F407xE
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif

/*
 * We can use internal ccmram as EXPANSION_TYPE_130XE because we need
 * only 64k of ram.
 * Otherwise we have to select the correct chip to ensure the data
 * to be read/written to be available to the Atari MCU
 */
#define USE_INTERNAL_CCRAM_EXPANSION
#undef  USE_INTERNAL_CCRAM_EXPANSION
#define USE_INTERNAL_DATARAM_EXPANSION
#undef  USE_INTERNAL_DATARAM_EXPANSION

#if defined(USE_INTERNAL_CCRAM_EXPANSION) && defined(USE_INTERNAL_DATARAM_EXPANSION)
	#error "ONLY ONE TYPE OF MEMORY CAN BE USED"
#endif

/*
 * We can use the internal ccram and the sram as ram expansion
 */
#ifdef USE_INTERNAL_CCRAM_EXPANSION
	unsigned char expansion_ram[64*1024] __attribute__((section(".ccmram")));
#endif
#ifdef USE_INTERNAL_DATARAM_EXPANSION
	unsigned char expansion_ram[64*1024] __attribute__((section(".data")));
#endif

typedef enum {
    EXPANSION_TYPE_130XE = 0,       /* 0 0 0 */
    EXPANSION_TYPE_192K_COMPYSHOP,  /* 0 0 1 */
    EXPANSION_TYPE_320K_RAMBO,      /* 0 1 0 */
    EXPANSION_TYPE_320K_COMPYSHOP,  /* 0 1 1 */
    EXPANSION_TYPE_576K_MOD,        /* 1 0 0 */
    EXPANSION_TYPE_1088K_MOD,       /* 1 0 1 */
                                    /* 1 1 0 */
                                    /* 1 1 1 */
    EXPANSION_TYPE_NONE,            // LAST
} t_expansion;

typedef enum {
	WRITERAM = 0,
	READRAM,
} t_command;

#define RD5_LOW     GPIOB->BSRRH  = GPIO_Pin_2;
#define RD4_LOW     GPIOB->BSRRH  = GPIO_Pin_4;
#define RD5_HIGH    GPIOB->BSRRL  = GPIO_Pin_2;
#define RD4_HIGH    GPIOB->BSRRL  = GPIO_Pin_4;

#define CONTROL_IN  GPIOC->IDR
#define ADDR_IN     GPIOD->IDR
#define DATA_IN     GPIOE->IDR
#define DATA_OUT    GPIOE->ODR

#define PHI2_RD        (GPIOC->IDR & 0x0001)
#define S5_RD          (GPIOC->IDR & 0x0002)
#define S4_RD          (GPIOC->IDR & 0x0004)
#define S4_AND_S5_HIGH (GPIOC->IDR & 0x0006) == 0x6
#define D1XX_RD        (GPIOC->IDR & 0x0008)

#define PHI2    0x0001
#define S5      0x0002
#define S4      0x0004
#define D1XX    0x0008
#define CCTL    0x0010
#define RW      0x0020

#define SET_DATA_MODE_IN        GPIOE->MODER = 0x00000000;
#define SET_DATA_MODE_OUT       GPIOE->MODER = 0x00005555;

#define GREEN_LED_OFF           GPIOB->BSRRH = GPIO_Pin_0;
#define GREEN_LED_ON            GPIOB->BSRRL = GPIO_Pin_0;

#define INTERNAL_RAM_DISABLE    GPIOB->BSRRL = GPIO_Pin_8; /* nEXSEL */
#define INTERNAL_RAM_ENABLE     GPIOB->BSRRH = GPIO_Pin_8; /* nEXSEL */

#define ATARI_RESET_ASSERT      GPIOA->BSRRL = GPIO_Pin_3; /* RST -> GPIO(A.3) LOW */
#define ATARI_RESET_DEASSERT    GPIOA->BSRRH = GPIO_Pin_3; /* RST -> GPIO(A.3) HIGH */

#define CS_512K                 (1 << 13) /* CS#0 PC13 */
#define CS_1024K                (1 << 14) /* CS#0 PC14 */
#define MEMORY_EXPANSION_TYPE   (GPIOA->IDR & 0x0007) /* PA0, PA1, PA2 */
#define CHIPRAM_DESELECT        GPIOC->ODR = 0xff00   /* PC8-PC15 RAMCHIP DESELECTED */

/* RAM Pin nWE/nOE */
#define mOE                     GPIO_Pin_14
#define mWE                     GPIO_Pin_15
#define READRAM_SET             { GPIOB->BSRRL = mOE; GPIOB->BSRRH = mWE; }
#define WRITERAM_SET            { GPIOB->BSRRH = mOE; GPIOB->BSRRL = mWE; }
#define CHIPRAM_DISABLE         { GPIOB->BSRRH = mOE; GPIOB->BSRRH = mWE; }

/* Default values?? TODO: Read Altirra's Manual... */
static uint8_t PORTB = 0xFF;
static uint8_t PBCTL = 0x00;

/*
 * ******************** DEBUG PORT UART *****************************
 */
#define DEBUG_TX           GPIO_Pin_9
#define DEBUG_RX           GPIO_Pin_10
#define DEBUG_PINSRC_TX    GPIO_PinSource9
#define DEBUG_PINSRC_RX    GPIO_PinSource10
#define DEBUG_PORT         GPIOA
enum {
	DBG_ERROR = 0,
	DBG_INFO,
	DBG_VERBOSE,
	DBG_NOISY,
};
static int debuglevel = DBG_INFO;
#define PDEBUG(str)  USART_PutString(str)
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


static GPIO_InitTypeDef  GPIO_InitStructure;
static void config_gpio(void)
{
	/* Configuration PINS are PA0, PA1 and PA2
	 * As well as the mRST and mREF signals are now on PORTA (A.3 and A.4)
	 * leaving the PORTE only for DATABUS to optimize code handling
	 * (no more shifts...)
	 */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	/* CONF0 -> PA0, CONF1 -> PA1, CONF2 -> PA3 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure for RST & REF as output (100Mhz) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Green LED -> PB0, Red TP1 -> PB1, RD5 -> PB2, RD4 -> PB4
	 * PB5 -> NC, PB6 -> UNUSED (PU), PB7 -> D1xx, PB8 -> EXSEL,
	 * PB9 -> UNUSED (PU), PB10 -> HALT, PB11 -> MPD, PB12 -> IRQ 
	 * PB13 -> UNUSED (PU), PB14 mOE -> mOE, PB15 mWE -> mWE */

	/* GPIOB Periph clock enable (25Mhz) */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	/* Configure PB0, PB1in output pushpull mode */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
		GPIO_Pin_4 | GPIO_Pin_8 | GPIO_Pin_10 |
		GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Input Signals GPIO pins on PHI2 -> PC0, /S5 -> PC1, /S4 ->PC2,
	 * D1XX -> PC3, CCTL -> PC4, R/W -> PC5, PC8 -> A14, -> PC9 -> A15,
	 * PC10 -> A16, PC11 -> A17, PC12 -> A18, PC13 -> nCS0 ,
	 * PC14 -> nCS1, PC15 -> NU */

	/* GPIOC Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	/* Configure GPIO Settings (100Mhz) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
		GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure GPIO for RAM EXTRA ADDRESS (100Mhz) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
		GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* GPIOE Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

	/* Input/Output data GPIO pins on PE{0..7} */

	/* Configure GPIO Settings (25Mhz) */
	GPIO_InitStructure.GPIO_Pin =
		GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
		GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;   // avoid sharp edges
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* Input Address GPIO pins on PD{0..15} */
	/* GPIOD Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	/* Configure GPIO Settings (100Mhz) */
	GPIO_InitStructure.GPIO_Pin =
		GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
		GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 |
		GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
		GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
}

/* Useful for redirecting stdout output to serial line */
static void config_uart(uint32_t baudrate)
{
	// Enable clock for GPIOA
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	// Enable clock for USART1
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	// Connect PA9 to USART1_Tx
	GPIO_PinAFConfig(DEBUG_PORT, DEBUG_PINSRC_TX, GPIO_AF_USART1);
	// Connect PA10 to USART1_Rx
	GPIO_PinAFConfig(DEBUG_PORT, DEBUG_PINSRC_RX, GPIO_AF_USART1);

	// Initialization of DEBUG UART GPIO
	GPIO_InitStructure.GPIO_Pin = DEBUG_TX | DEBUG_RX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(DEBUG_PORT, &GPIO_InitStructure);

	// Initialization of USART1
	USART_InitTypeDef USART_InitStruct;
	USART_InitStruct.USART_BaudRate = baudrate;
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_InitStruct.USART_Parity = USART_Parity_No;
	USART_InitStruct.USART_StopBits = USART_StopBits_1;
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStruct);

	// Enable USART1
	USART_Cmd(USART1, ENABLE);
}

static void USART_PutChar(char c)
{
	// Wait until transmit data register is empty
	while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE))
		;
	// Send a char using USART1
	USART_SendData(USART1, c);
}

static void USART_PutString(char *s)
{
	// Send a string
	while (*s)
	{
		USART_PutChar(*s++);
	}
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
		case EXPANSION_TYPE_192K_COMPYSHOP:
			PDEBUG(ANSI_BLUE);
			PDEBUG("192K COMPYSHOP EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_320K_COMPYSHOP:
			PDEBUG(ANSI_BLUE);
			PDEBUG("320K COMPYSHOP EXPANSION\r\n");
			PDEBUG(ANSI_RESET);
			break;
		case EXPANSION_TYPE_320K_RAMBO:
			PDEBUG(ANSI_BLUE);
			PDEBUG("320K RAMBO EXPANSION\r\n");
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


#define CHIPRAM_BANKSELECT(a)   GPIOC->ODR = a & 0xff00 /* HIGH BYTE PC8-PC12 */
static inline void activate_ram_chip(uint8_t bank, t_command op)
{
	uint16_t CHIPSELECT; /* All used bit for bank & chip addressing */
	// 5 BITS (16K WINDOW x 32 banks PER CHIP)
	// B0 B1 B2 B3 B4 B5 B6 B7
	// X  X  X  X  X  X  -- --
	// Banks can be from 0 to 63
	// HARDWARE CONNECTIONS
	// C8  C9  C10  C11  C12  C13  C14  C15
	// 0   0   0    0    0    CS0  CS1   NC
	CHIPSELECT = (bank & 0x1F) << 8;
	if (bank < 32)
	{
		// we are referring the the first Chip
		CHIPSELECT &= ~CS_512K;
		CHIPSELECT |= CS_1024K;
	}
	else
	{
		// we are referring to the second Chip
		CHIPSELECT &= ~CS_1024K;
		CHIPSELECT |= CS_512K;
	}

	// Now select nOE (LOW) and nWE (high) for READING FROM RAM
	// or nOE (HIGH) and nWE (low) for WRITING TO RAM
	switch( op )
	{
		case READRAM:
			READRAM_SET;
			break;
		case WRITERAM:
			WRITERAM_SET;
			break;
	}
	CHIPRAM_BANKSELECT(CHIPSELECT);
}

/*
    Theory of Operation
    -------------------
    Atari sends memory bank setup writing/reading from PORTB & PBCTL addresses.

    Any access into the window 0x4000-0x7FFF will be forced to use the
    expanded memory instead of the internal one. The EXSEL pin will be
    activated to ensure no internal RAM access will be used.
    It will be re-activated after sent into the bus...
*/

int main(void)
{
	t_expansion expansion_type = EXPANSION_TYPE_130XE; /* Default */
	uint16_t addr;
	uint8_t data;
	uint8_t c;

	config_gpio();

	/* KEEP ATARI IN RESET */
	ATARI_RESET_ASSERT

	/* Read configuration DIP-SWITCHES from PORT! */
	expansion_type = MEMORY_EXPANSION_TYPE;

	config_uart(115200);

	banner( expansion_type );

	GREEN_LED_OFF

	// Start READING FROM DATABUS!
	SET_DATA_MODE_IN

	// Not needed if using FreeRTOS!
	__disable_irq();

	/* REMOVE ATARI FROM RESET STATE */
	ATARI_RESET_DEASSERT

	while (1)
	{
		/* First of all, the external ram is always disabled */
		CHIPRAM_DESELECT;
		CHIPRAM_DISABLE;

		/* 
		 * Emulating of PORTB/PBCTL and RAM selection bits when
		 * accessing 0x4000-0x7FFF area.
		 */

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
				break;

			case 0xD301: /* PORTB Emulation */
				// ATARI CPU Needs to WRITE Data?
				if (!(c & RW))
				{
					// Now READs the data to be written
					data = DATA_IN;
					// read the data bus on falling edge of phi2
					while (CONTROL_IN & PHI2)
						data = DATA_IN;
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
				break;

			default:
				/* WINDOW BANKED MEMORY ACCESS 16K */
				if (addr >= 0x4000 && addr < 0x8000)
				{
					addr -= 0x4000; /* Relative address here */
					/*
						D2 Data direction register enable
							0 PORTB [D301] accesses data direction register
							1 PORTB [D301] accesses input and output registers
					*/
					// We can have expansion ram access only if PBCTL bit 2 is 1!
					if (PBCTL & (1 << 2))
					{
						uint8_t bank;
						int skipmemory = 1; 

						// When accessing the external RAM the internal
						// RAM must be disabled!
						INTERNAL_RAM_DISABLE;

						/* Good! We can access our AtariMegaRAM EXPANSION! */
						GREEN_LED_ON;

						// ATARI CPU Needs to WRITE Data?
						if (!(c & RW))
						{
							// Now we can write the data at the desired address
							// stored in the external ram expansion
							// Let's calculate the correct bank address
							switch (expansion_type)
							{
								case EXPANSION_TYPE_130XE:
									bank = (PORTB & 0x0c) >> 2;
									skipmemory = 0;
							#ifdef USE_INTERNAL_CCRAM_EXPANSION
									// Now READs the data to be written in internal memory
									data = DATA_IN;
									// read the data bus on falling edge of phi2
									while (CONTROL_IN & PHI2)
										data = DATA_IN;
									expansion_ram[ addr + (bank * 0x4000) ] = data;
									skipmemory = 1;
							#endif
									break;
								case EXPANSION_TYPE_192K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x40) >> 2)) >> 2);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_320K_RAMBO:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x60) >> 1)) >> 2);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_320K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0xc0) >> 2)) >> 2);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_576K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0x60) >> 1)) >> 1);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_1088K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0xe0) >> 1)) >> 1);
									skipmemory = 0;
									break;
								default:
									bank = 0;
									skipmemory = 1;
									break; // Not implemented yet!
							}

							// Activate CHIP RAM if needed
							if (!skipmemory)
								activate_ram_chip(bank, WRITERAM);

							// The DATABUS it is directly connected to the RAM Chip
						}
						else
						{
							// ATARI CPU needs to READ Data!
							switch (expansion_type)
							{
								case EXPANSION_TYPE_130XE:
									bank = (PORTB & 0x0c) >> 2;
									skipmemory = 0;
#ifdef USE_INTERNAL_CCRAM_EXPANSION
									// CPU Needs to READ Data from internal memory
									SET_DATA_MODE_OUT
									// Only the upper 8 bit of the port for DATA are used
									DATA_OUT = (expansion_ram[ addr + (bank * 0x4000) ]);
									skipmemory = 1;
#endif
									break;
								case EXPANSION_TYPE_192K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x40) >> 2)) >> 2);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_320K_RAMBO:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x60) >> 1)) >> 2);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_320K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0xc0) >> 2)) >> 2);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_576K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0x60) >> 1)) >> 1);
									skipmemory = 0;
									break;
								case EXPANSION_TYPE_1088K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0xe0) >> 1)) >> 1);
									skipmemory = 0;
									break;
								default:
									// CPU Needs to READ Data. We give it the nop opcode!
									SET_DATA_MODE_OUT
									DATA_OUT = 0xEA; /* NOP 6502 */
									skipmemory = 1;
									break;
							}

							// ACTIVATE RAM CHIP if needed
							if (!skipmemory)
								activate_ram_chip(bank, READRAM);

							// The memory chip should be prepare the DATA BUS with the correct
							// data fetched from the address selected
 
							// wait for phi2 low
							while (CONTROL_IN & PHI2)
								;
							SET_DATA_MODE_IN
						}
						GREEN_LED_OFF;

						INTERNAL_RAM_ENABLE;
						CHIPRAM_DESELECT;
						CHIPRAM_DISABLE;
					}
					else
					{
						// PBCTL Bit 2 is not set, so do nothing with
						// external memory
						INTERNAL_RAM_ENABLE;
						CHIPRAM_DESELECT;
						CHIPRAM_DISABLE;
					}
				}
				else
				{
					// Idle
					INTERNAL_RAM_ENABLE;
					CHIPRAM_DESELECT;
					CHIPRAM_DISABLE;
				}
				break;
		}
	}
}
