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
#include "tm_stm32f4_fatfs.h"
#include "tm_stm32f4_delay.h"
#include <stdio.h>
#include <string.h>

#ifdef STM32F407xE
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif

unsigned char expansion_ram[64*1024] __attribute__((section(".ccmram")));

/*
 * We can use internal ccmram as EXPANSION_TYPE_130XE because we need
 * only 64k of ram.
 * Otherwise we have to select the correct chip to ensure the data
 * to be read/written to be available to the Atari MCU
 */
#define USE_INTERNAL_CCRAM_EXPANSION
#undef  USE_INTERNAL_CCRAM_EXPANSION

typedef enum {
	EXPANSION_TYPE_130XE = 0,
	EXPANSION_TYPE_192K_COMPYSHOP,
	EXPANSION_TYPE_320K_RAMBO,
	EXPANSION_TYPE_320K_COMPYSHOP,
	EXPANSION_TYPE_576K_MOD,
	EXPANSION_TYPE_1088K_MOD,
	EXPANSION_TYPE_NONE, // LAST
} t_expansion;

#define RD5_LOW  GPIOB->BSRRH  = GPIO_Pin_2;
#define RD4_LOW  GPIOB->BSRRH  = GPIO_Pin_4;
#define RD5_HIGH GPIOB->BSRRL  = GPIO_Pin_2;
#define RD4_HIGH GPIOB->BSRRL  = GPIO_Pin_4;

#define CONTROL_IN GPIOC->IDR
#define ADDR_IN    GPIOD->IDR
#define DATA_IN    GPIOE->IDR
#define DATA_OUT   GPIOE->ODR

#define PHI2_RD        (GPIOC->IDR & 0x0001)
#define S5_RD          (GPIOC->IDR & 0x0002)
#define S4_RD          (GPIOC->IDR & 0x0004)
#define S4_AND_S5_HIGH (GPIOC->IDR & 0x0006) == 0x6

#define PHI2	0x0001
#define S5		0x0002
#define S4		0x0004
#define CCTL	0x0010
#define RW		0x0020

#define SET_DATA_MODE_IN GPIOE->MODER = 0x00000000;
#define SET_DATA_MODE_OUT GPIOE->MODER = 0x55550000;

#define GREEN_LED_OFF GPIOB->BSRRH = GPIO_Pin_0;
#define GREEN_LED_ON GPIOB->BSRRL = GPIO_Pin_0;

#define INTERNAL_RAM_DISABLE GPIOB->BSRRL = GPIO_Pin_8;
#define INTERNAL_RAM_ENABLE  GPIOB->BSRRH = GPIO_Pin_8;

#define CHIPRAM_BANKSELECT(a)	(GPIO_Write(GPIOC, a << 8)); /* HIGH BYTE PC8-PC15 */

/* Default values?? TODO: */
static uint8_t PORTB = 0xFF;
static uint8_t PBCTL = 0x00;

GPIO_InitTypeDef  GPIO_InitStructure;

void config_gpio()
{
	/* Green LED -> PB0, Red TP1 -> PB1, RD5 -> PB2, RD4 -> PB4
	 * PB5 -> NC, PB6 -> UNUSED (PU), PB7 -> D1xx, PB8 -> EXSEL,
	 * PB9 -> UNUSED (PU), PB10 -> HALT, PB11 -> MPD, PB12 -> IRQ */

	/* GPIOB Periph clock enable (25Mhz) */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	/* Configure PB0, PB1in output pushpull mode */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
		GPIO_Pin_4 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_10 |
		GPIO_Pin_11 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Input Signals GPIO pins on PHI2 -> PC0, /S5 -> PC1, /S4 ->PC2,
	 * CCTL -> PC4, R/W -> PC5, PC8 -> A14, -> PC9 -> A15, PC10 -> A16,
	 * PC11 -> A17, PC12 -> A18, PC13 -> nCS , PC14 -> NU, PC15 -> NU */

	/* GPIOC Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	/* Configure GPIO Settings (100Mhz) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
		GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure GPIO for RAM EXTRA ADDRESS (100Mhz) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
		GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* GPIOE Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

	/* Input/Output data GPIO pins on PE{8..15} */

	/* Configure GPIO Settings (25Mhz) */
	GPIO_InitStructure.GPIO_Pin =
		GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
		GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;	// avoid sharp edges
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* Configure for RST & REF as output (100Mhz) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
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


/*
	Theory of Operation
	-------------------
	Atari sends memory bank setup writing/reading from PORTB & PBCTL addresses.

	Any access into the window 0x4000-0x7FFF will be forced to use the
	expanded memory instead of the internal one. The EXSEL pin will be
	activated to ensure no internal RAM access will be used.
	It will be re-activated after sent into the bus...
*/

int main(void) {

	t_expansion expansion_type = EXPANSION_TYPE_130XE; /* Default */
	uint16_t addr;
	uint8_t data;
	uint8_t c;

	config_gpio();

	GREEN_LED_OFF

	// Start READING FROM DATABUS!
	SET_DATA_MODE_IN

	__disable_irq();

	while (1) {
		/* Check for accessing PORTB, PBCTL and Address Space 0x4000-0x7FFF */

		// Wait for a valid sequence in the bus
		// PHI2 HIGH

		// wait for phi2 high
		while (!((c = CONTROL_IN) & PHI2)) ;

		// Check for address valid on the bus!
		addr = ADDR_IN;

		switch (addr)
		{
			case 0xD303: /* PBCTL */
				// ATARI CPU Needs to WRITE Data?
				if (!(c & RW))
				{
					// Now READs the data to be written
					data = DATA_IN;
					// read the data bus on falling edge of phi2
					while (CONTROL_IN & PHI2)
						data = DATA_IN;
					PBCTL = (data & 0xff00) >> 8;
				}
				else
				{
					// ATARI CPU Needs to READ Data
					SET_DATA_MODE_OUT
					DATA_OUT = PBCTL << 8;

					// wait for phi2 low
					while (CONTROL_IN & PHI2) ;
					SET_DATA_MODE_IN
				}
				break;

			case 0xD301: /* PORTB */
				// ATARI CPU Needs to WRITE Data?
				if (!(c & RW))
				{
					// Now READs the data to be written
					data = DATA_IN;
					// read the data bus on falling edge of phi2
					while (CONTROL_IN & PHI2)
						data = DATA_IN;
					PORTB = (data & 0xff00) >> 8;
				}
				else
				{
					// ATARI CPU Needs to READ Data
					SET_DATA_MODE_OUT
					DATA_OUT = PORTB << 8;

					// wait for phi2 low
					while (CONTROL_IN & PHI2) ;
					SET_DATA_MODE_IN
				}
				break;

			default:
				/* WINDOW BANKED MEMORY ACCESS 16K */
				if (addr <= 0x7fff && addr >= 0x4000)
				{
					addr -= 0x4000; /* Relative address here */
					/*
					 * D2 Data direction register enable
							0 PORTB [D301] accesses data direction register
							1 PORTB [D301] accesses input and output registers
					*/
					// We can have expansion ram access only if PBCTL bit 2 is 1!
					if (PBCTL & (1 << 2))
					{
						uint8_t bank;

						INTERNAL_RAM_DISABLE;

						/* Good! We can access our AM EXPANSION! */
						GREEN_LED_ON;

						// ATARI CPU Needs to WRITE Data?
						if (!(c & RW))
						{
							// Now READs the data to be written
							data = DATA_IN;
							// read the data bus on falling edge of phi2
							while (CONTROL_IN & PHI2)
								data = DATA_IN;

							data = (data & 0xff00) >> 8;

							// Now we can write the data at the desired address
							// stored in the external ram expansion
							// Let's calculate the correct bank address
							switch (expansion_type)
							{
								case EXPANSION_TYPE_130XE:
									bank = (PORTB & 0x0c) >> 2;
							#ifdef USE_INTERNAL_CCRAM_EXPANSION
									expansion_ram[ addr + (bank * 0x4000) ] = data;
							#else
									CHIPRAM_BANKSELECT(bank);
							#endif
									break;
								case EXPANSION_TYPE_192K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x40) >> 2)) >> 2);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_320K_RAMBO:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x60) >> 1)) >> 2);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_320K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0xc0) >> 2)) >> 2);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_576K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0x60) >> 1)) >> 1);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_1088K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0xe0) >> 1)) >> 1);
									CHIPRAM_BANKSELECT(bank);
									break;
								default:
									break; // Not implemented yet!
							}

						}
						else
						{
							switch (expansion_type)
							{
								case EXPANSION_TYPE_130XE:
									bank = (PORTB & 0x0c) >> 2;
#ifdef USE_INTERNAL_CCRAM_EXPANSION
									// CPU Needs to READ Data from internal memory
									SET_DATA_MODE_OUT
									DATA_OUT = (expansion_ram[ addr + (bank * 0x4000) ]) << 8;
#else
									CHIPRAM_BANKSELECT(bank);
#endif
									break;
								case EXPANSION_TYPE_192K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x40) >> 2)) >> 2);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_320K_RAMBO:
									bank = (((PORTB & 0x0c) + ((PORTB & 0x60) >> 1)) >> 2);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_320K_COMPYSHOP:
									bank = (((PORTB & 0x0c) + ((PORTB & 0xc0) >> 2)) >> 2);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_576K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0x60) >> 1)) >> 1);
									CHIPRAM_BANKSELECT(bank);
									break;
								case EXPANSION_TYPE_1088K_MOD:
									bank = (((PORTB & 0x0e) + ((PORTB & 0xe0) >> 1)) >> 1);
									CHIPRAM_BANKSELECT(bank);
									break;
								default:
									// CPU Needs to READ Data. We give it the nop opcode!
									SET_DATA_MODE_OUT
									DATA_OUT = (0xEA << 8); /* NOP 6502 */
									break;
							}
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
						// PBCTL Bit 2 is not set, so do nothing
					}
				}
				else
				{
					// Idle
				}
				break;
		}
	}
}
