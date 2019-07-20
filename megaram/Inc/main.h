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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define mPHI2_Pin GPIO_PIN_8
#define mPHI2_GPIO_Port GPIOI
#define mA13_Pin GPIO_PIN_13
#define mA13_GPIO_Port GPIOC
#define mA14_Pin GPIO_PIN_14
#define mA14_GPIO_Port GPIOC
#define mA15_Pin GPIO_PIN_15
#define mA15_GPIO_Port GPIOC
#define mA0_Pin GPIO_PIN_0
#define mA0_GPIO_Port GPIOC
#define mA1_Pin GPIO_PIN_1
#define mA1_GPIO_Port GPIOC
#define mA2_Pin GPIO_PIN_2
#define mA2_GPIO_Port GPIOC
#define mA3_Pin GPIO_PIN_3
#define mA3_GPIO_Port GPIOC
#define mD0_Pin GPIO_PIN_0
#define mD0_GPIO_Port GPIOA
#define mD1_Pin GPIO_PIN_1
#define mD1_GPIO_Port GPIOA
#define mD2_Pin GPIO_PIN_2
#define mD2_GPIO_Port GPIOA
#define mD3_Pin GPIO_PIN_3
#define mD3_GPIO_Port GPIOA
#define mD4_Pin GPIO_PIN_4
#define mD4_GPIO_Port GPIOA
#define mD5_Pin GPIO_PIN_5
#define mD5_GPIO_Port GPIOA
#define mD6_Pin GPIO_PIN_6
#define mD6_GPIO_Port GPIOA
#define mD7_Pin GPIO_PIN_7
#define mD7_GPIO_Port GPIOA
#define mA4_Pin GPIO_PIN_4
#define mA4_GPIO_Port GPIOC
#define mA5_Pin GPIO_PIN_5
#define mA5_GPIO_Port GPIOC
#define LED_Pin GPIO_PIN_0
#define LED_GPIO_Port GPIOB
#define mRST_Pin GPIO_PIN_1
#define mRST_GPIO_Port GPIOB
#define mRD5_Pin GPIO_PIN_2
#define mRD5_GPIO_Port GPIOB
#define mIRQ_Pin GPIO_PIN_10
#define mIRQ_GPIO_Port GPIOB
#define mA6_Pin GPIO_PIN_6
#define mA6_GPIO_Port GPIOC
#define mA7_Pin GPIO_PIN_7
#define mA7_GPIO_Port GPIOC
#define mA8_Pin GPIO_PIN_8
#define mA8_GPIO_Port GPIOC
#define mA9_Pin GPIO_PIN_9
#define mA9_GPIO_Port GPIOC
#define mPHI2I0_Pin GPIO_PIN_0
#define mPHI2I0_GPIO_Port GPIOI
#define mS5_Pin GPIO_PIN_1
#define mS5_GPIO_Port GPIOI
#define mS4_Pin GPIO_PIN_2
#define mS4_GPIO_Port GPIOI
#define mD1XX_Pin GPIO_PIN_3
#define mD1XX_GPIO_Port GPIOI
#define mA10_Pin GPIO_PIN_10
#define mA10_GPIO_Port GPIOC
#define mA11_Pin GPIO_PIN_11
#define mA11_GPIO_Port GPIOC
#define mA12_Pin GPIO_PIN_12
#define mA12_GPIO_Port GPIOC
#define mREF_Pin GPIO_PIN_3
#define mREF_GPIO_Port GPIOB
#define mRD4_Pin GPIO_PIN_4
#define mRD4_GPIO_Port GPIOB
#define mEXSEL_Pin GPIO_PIN_5
#define mEXSEL_GPIO_Port GPIOB
#define PB6_Pin GPIO_PIN_6
#define PB6_GPIO_Port GPIOB
#define mHALT_Pin GPIO_PIN_7
#define mHALT_GPIO_Port GPIOB
#define mMPD_Pin GPIO_PIN_8
#define mMPD_GPIO_Port GPIOB
#define PB9_Pin GPIO_PIN_9
#define PB9_GPIO_Port GPIOB
#define mCCTL_Pin GPIO_PIN_4
#define mCCTL_GPIO_Port GPIOI
#define mRW_Pin GPIO_PIN_5
#define mRW_GPIO_Port GPIOI
#define mCONF2_Pin GPIO_PIN_6
#define mCONF2_GPIO_Port GPIOI
#define mCONF1_Pin GPIO_PIN_7
#define mCONF1_GPIO_Port GPIOI
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
