/**
  ******************************************************************************
  * @file    stm32h5xx_hal_conf.h
  * @brief   HAL configuration for ABEX-MEGARAM STM32H5E5ZJ port.
  *          Only the modules actually used are enabled.
  *          No HAL_SRAM / HAL_FMC — expansion RAM is in internal SRAM.
  ******************************************************************************
  */
#ifndef __STM32H5xx_HAL_CONF_H
#define __STM32H5xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Module selection ────────────────────────────────────────────────────────
 * Enable only what this application needs.
 * No HAL_SRAM, no HAL_FMC — internal SRAM replaces external FMC SRAM.
 */
#define HAL_MODULE_ENABLED

#define HAL_GPIO_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED

/* ── Oscillator values ───────────────────────────────────────────────────────*/
#if !defined(HSE_VALUE)
  #define HSE_VALUE             8000000U   /* 8 MHz crystal */
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT   100U
#endif
#if !defined(HSI_VALUE)
  #define HSI_VALUE             64000000U
#endif
#if !defined(HSI48_VALUE)
  #define HSI48_VALUE           48000000U
#endif
#if !defined(CSI_VALUE)
  #define CSI_VALUE             4000000U
#endif
#if !defined(LSI_VALUE)
  #define LSI_VALUE             32000U
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE             32768U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT   5000U
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE  12288000U
#endif

/* ── System configuration ────────────────────────────────────────────────────*/
#define VDD_VALUE               3300U
#define TICK_INT_PRIORITY       0U
#define USE_RTOS                0U

/* ── Assert ──────────────────────────────────────────────────────────────────*/
/* #define USE_FULL_ASSERT */

/* ── HAL header includes ─────────────────────────────────────────────────────*/
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32h5xx_hal_rcc.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
  #include "stm32h5xx_hal_exti.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32h5xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32h5xx_hal_dma.h"
  #include "stm32h5xx_hal_dma_ex.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32h5xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32h5xx_hal_flash.h"
  #include "stm32h5xx_hal_flash_ex.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32h5xx_hal_pwr.h"
  #include "stm32h5xx_hal_pwr_ex.h"
#endif

/* ── Assert macro ────────────────────────────────────────────────────────────*/
#ifdef USE_FULL_ASSERT
  #define assert_param(expr) \
    ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32H5xx_HAL_CONF_H */
