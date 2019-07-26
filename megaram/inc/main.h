/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"

extern void Error_Handler(const char *c);

/* ADDRESS BUS PINS 16BIT */
#define m_A15_Pin LL_GPIO_PIN_15
#define m_A15_GPIO_Port GPIOC
#define m_A14_Pin LL_GPIO_PIN_14
#define m_A14_GPIO_Port GPIOC
#define m_A13_Pin LL_GPIO_PIN_13
#define m_A13_GPIO_Port GPIOC
#define m_A12_Pin LL_GPIO_PIN_12
#define m_A12_GPIO_Port GPIOC
#define m_A11_Pin LL_GPIO_PIN_11
#define m_A11_GPIO_Port GPIOC
#define m_A10_Pin LL_GPIO_PIN_10
#define m_A10_GPIO_Port GPIOC
#define m_A9_Pin LL_GPIO_PIN_9
#define m_A9_GPIO_Port GPIOC
#define m_A8_Pin LL_GPIO_PIN_8
#define m_A8_GPIO_Port GPIOC
#define m_A7_Pin LL_GPIO_PIN_7
#define m_A7_GPIO_Port GPIOC
#define m_A6_Pin LL_GPIO_PIN_6
#define m_A6_GPIO_Port GPIOC
#define m_A5_Pin LL_GPIO_PIN_5
#define m_A5_GPIO_Port GPIOC
#define m_A4_Pin LL_GPIO_PIN_4
#define m_A4_GPIO_Port GPIOC
#define m_A3_Pin LL_GPIO_PIN_3
#define m_A3_GPIO_Port GPIOC
#define m_A2_Pin LL_GPIO_PIN_2
#define m_A2_GPIO_Port GPIOC
#define m_A1_Pin LL_GPIO_PIN_1
#define m_A1_GPIO_Port GPIOC
#define m_A0_Pin LL_GPIO_PIN_0
#define m_A0_GPIO_Port GPIOC

/* DATA BUS 8BIT */
#define m_D0_Pin LL_GPIO_PIN_0
#define m_D0_GPIO_Port GPIOA
#define m_D1_Pin LL_GPIO_PIN_1
#define m_D1_GPIO_Port GPIOA
#define m_D2_Pin LL_GPIO_PIN_2
#define m_D2_GPIO_Port GPIOA
#define m_D3_Pin LL_GPIO_PIN_3
#define m_D3_GPIO_Port GPIOA
#define m_D4_Pin LL_GPIO_PIN_4
#define m_D4_GPIO_Port GPIOA
#define m_D5_Pin LL_GPIO_PIN_5
#define m_D5_GPIO_Port GPIOA
#define m_D6_Pin LL_GPIO_PIN_6
#define m_D6_GPIO_Port GPIOA
#define m_D7_Pin LL_GPIO_PIN_7
#define m_D7_GPIO_Port GPIOA

/* LED */
#define LED_Pin LL_GPIO_PIN_0
#define LED_GPIO_Port GPIOB

/* CONTROL TO 6502 RESET,RD5,RD4,... */
#define m_RST_Pin LL_GPIO_PIN_1
#define m_RST_GPIO_Port GPIOB
#define m_RD5_Pin LL_GPIO_PIN_2
#define m_RD5_GPIO_Port GPIOB
//#define m_REF_Pin LL_GPIO_PIN_3
//#define m_REF_GPIO_Port GPIOB
#define m_RD4_Pin LL_GPIO_PIN_4
#define m_RD4_GPIO_Port GPIOB
#define m_EXSEL_Pin LL_GPIO_PIN_5
#define m_EXSEL_GPIO_Port GPIOB
#define PB6_Pin LL_GPIO_PIN_6
#define PB6_GPIO_Port GPIOB
#define m_HALT_Pin LL_GPIO_PIN_7
#define m_HALT_GPIO_Port GPIOB
#define m_MPD_Pin LL_GPIO_PIN_8
#define m_MPD_GPIO_Port GPIOB
#define PB9_Pin LL_GPIO_PIN_9
#define PB9_GPIO_Port GPIOB
#define m_IRQ_Pin LL_GPIO_PIN_10
#define m_IRQ_GPIO_Port GPIOB

/* CONTROL FROM 6502 PHI2 CLK, R/W, ... */
#define m_PHI2_Pin LL_GPIO_PIN_0
#define m_PHI2_GPIO_Port GPIOI
#define m_S5_Pin LL_GPIO_PIN_1
#define m_S5_GPIO_Port GPIOI
#define m_S4_Pin LL_GPIO_PIN_2
#define m_S4_GPIO_Port GPIOI
#define m_D1XX_Pin LL_GPIO_PIN_3
#define m_D1XX_GPIO_Port GPIOI
#define m_CCTL_Pin LL_GPIO_PIN_4
#define m_CCTL_GPIO_Port GPIOI
#define m_RW_Pin LL_GPIO_PIN_5
#define m_RW_GPIO_Port GPIOI

/* DEBUG UART */
#define DEBUG_TX_Pin LL_GPIO_PIN_9
#define DEBUG_TX_GPIO_Port GPIOA
#define DEBUG_RX_Pin LL_GPIO_PIN_10
#define DEBUG_RX_GPIO_Port GPIOA

/* EMULATION Configuration PIN */ 
#define CONF0_Pin LL_GPIO_PIN_6
#define CONF0_GPIO_Port GPIOI
#define CONF1_Pin LL_GPIO_PIN_7
#define CONF1_GPIO_Port GPIOI
#define CONF2_Pin LL_GPIO_PIN_8
#define CONF2_GPIO_Port GPIOI

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
