/**
  ******************************************************************************
  * @file    main.h
  * @brief   ABEX-MEGARAM — STM32H5E5ZJ port (Cortex-M33 @ 250 MHz)
  *
  * TODO: once the H5E5ZJ PCB schematic is finalised, update:
  *   1. CONTROL_PORT / ADDR_PORT / DATA_PORT / OUTPUT_PORT macros
  *   2. All _Pin and _GPIO_Port defines
  *   3. DEBUG_TX_Pin / DEBUG_RX_Pin / DEBUG_AF (USART1 alternate function)
  *   4. Remove unused GPIO port clock enables in gpio_init() in main.c
  ******************************************************************************
  */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── HAL / LL headers (STM32H5 family) ─────────────────────────────────────*/
#include "stm32h5xx_hal.h"
#include "stm32h5xx_ll_bus.h"
#include "stm32h5xx_ll_cortex.h"
#include "stm32h5xx_ll_gpio.h"
#include "stm32h5xx_ll_pwr.h"
#include "stm32h5xx_ll_rcc.h"
#include "stm32h5xx_ll_system.h"
#include "stm32h5xx_ll_usart.h"
#include "stm32h5xx_ll_utils.h"
#include "stm32h5xx_ll_exti.h"

/* ── Port mapping ────────────────────────────────────────────────────────────
 *
 * TODO: update all four port defines once the H5E5ZJ PCB schematic is ready.
 * The names below are carried over from the STM32F429 board as placeholders.
 */
#define CONTROL_PORT  GPIOI  /* PHI2, S5, S4, D1XX, CCTL, R/W, CONF0-2 — TODO */
#define ADDR_PORT     GPIOC  /* A0–A15                                    — TODO */
#define DATA_PORT     GPIOA  /* D0–D7                                     — TODO */
#define OUTPUT_PORT   GPIOB  /* LED, RST, EXSEL, RD4/5, IRQ, HALT, MPD   — TODO */

/* ── Debug USART ─────────────────────────────────────────────────────────────
 * TODO: check USART1 TX/RX alternate function number in H5E5ZJ datasheet
 * (typically AF7, but verify).
 */
#define DEBUG_TX_Pin       LL_GPIO_PIN_9  /* USART1 TX — TODO */
#define DEBUG_TX_GPIO_Port GPIOA          /* TODO */
#define DEBUG_RX_Pin       LL_GPIO_PIN_10 /* USART1 RX — TODO */
#define DEBUG_RX_GPIO_Port GPIOA          /* TODO */
#define DEBUG_AF           LL_GPIO_AF_7   /* USART1 AF on H5 — TODO: verify */

/* ── Control input pins (CONTROL_PORT = GPIOI — TODO) ───────────────────────*/
#define m_PHI2_Pin  LL_GPIO_PIN_0  /* PHI2 system clock       — TODO */
#define m_S5_Pin    LL_GPIO_PIN_1  /* Atari S5 signal         — TODO */
#define m_S4_Pin    LL_GPIO_PIN_2  /* Atari S4 signal         — TODO */
#define m_D1XX_Pin  LL_GPIO_PIN_3  /* D1XX decode             — TODO */
#define m_CCTL_Pin  LL_GPIO_PIN_4  /* CCTL (cartridge ctrl)   — TODO */
#define m_RW_Pin    LL_GPIO_PIN_5  /* R/W line                — TODO */
#define CONF0_Pin   LL_GPIO_PIN_6  /* DIP switch CONF0        — TODO */
#define CONF1_Pin   LL_GPIO_PIN_7  /* DIP switch CONF1        — TODO */
#define CONF2_Pin   LL_GPIO_PIN_8  /* DIP switch CONF2        — TODO */

/* ── Address bus pins (ADDR_PORT = GPIOC — TODO) ────────────────────────────*/
#define m_A0_Pin    LL_GPIO_PIN_0
#define m_A1_Pin    LL_GPIO_PIN_1
#define m_A2_Pin    LL_GPIO_PIN_2
#define m_A3_Pin    LL_GPIO_PIN_3
#define m_A4_Pin    LL_GPIO_PIN_4
#define m_A5_Pin    LL_GPIO_PIN_5
#define m_A6_Pin    LL_GPIO_PIN_6
#define m_A7_Pin    LL_GPIO_PIN_7
#define m_A8_Pin    LL_GPIO_PIN_8
#define m_A9_Pin    LL_GPIO_PIN_9
#define m_A10_Pin   LL_GPIO_PIN_10
#define m_A11_Pin   LL_GPIO_PIN_11
#define m_A12_Pin   LL_GPIO_PIN_12
#define m_A13_Pin   LL_GPIO_PIN_13
#define m_A14_Pin   LL_GPIO_PIN_14
#define m_A15_Pin   LL_GPIO_PIN_15

/* ── Data bus pins (DATA_PORT = GPIOA — TODO) ───────────────────────────────*/
#define m_D0_Pin    LL_GPIO_PIN_0
#define m_D1_Pin    LL_GPIO_PIN_1
#define m_D2_Pin    LL_GPIO_PIN_2
#define m_D3_Pin    LL_GPIO_PIN_3
#define m_D4_Pin    LL_GPIO_PIN_4
#define m_D5_Pin    LL_GPIO_PIN_5
#define m_D6_Pin    LL_GPIO_PIN_6
#define m_D7_Pin    LL_GPIO_PIN_7

/* ── Output / control pins (OUTPUT_PORT = GPIOB — TODO) ─────────────────────*/
#define LED_Pin      LL_GPIO_PIN_0   /* Green status LED       — TODO */
#define m_RST_Pin    LL_GPIO_PIN_1   /* ATARI RESET (active L) — TODO */
#define m_RD5_Pin    LL_GPIO_PIN_2   /* RD5 control            — TODO */
#define m_IRQ_Pin    LL_GPIO_PIN_3   /* IRQ line               — TODO */
#define m_RD4_Pin    LL_GPIO_PIN_4   /* RD4 control            — TODO */
#define m_EXSEL_Pin  LL_GPIO_PIN_5   /* EXSEL (ext RAM enable) — TODO */
#define m_HALT_Pin   LL_GPIO_PIN_6   /* HALT line              — TODO */
#define m_MPD_Pin    LL_GPIO_PIN_7   /* MPD line               — TODO */
#define PB6_Pin      LL_GPIO_PIN_8   /* General purpose        — TODO */
#define PB9_Pin      LL_GPIO_PIN_9   /* General purpose        — TODO */

/* ── Error handler ───────────────────────────────────────────────────────────*/
void Error_Handler(const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
