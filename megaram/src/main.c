/**
  ******************************************************************************
  * @file    main.c
  * @brief   ABEX-MEGARAM — STM32H5E5ZJ port (Cortex-M33 @ 250 MHz)
  *
  * Differenze rispetto alla versione STM32F429:
  *   - Nessun FMC: expansion_ram[] in SRAM interna (1 MB su 1.5 MB totali).
  *   - Nessun CCMRAM: bank_lut[] in SRAM normale (256 B).
  *   - GPIO su AHB2 (era AHB1 su F4).
  *   - PLL1: HSE 8 MHz / M=2 × N=125 / P=2 = 250 MHz, VOS0.
  *   - Pin TODO: aggiornare inc/main.h con lo schema della nuova board.
  ******************************************************************************
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
    EXPANSION_TYPE_NONE,
} t_expansion;

/* Bit positions within CONTROL_PORT->IDR (update if pin order changes) */
#define PHI2    0x0001
#define S5      0x0002
#define S4      0x0004
#define D1XX    0x0008
#define CCTL    0x0010
#define RW      0x0020

/* Cached MODER values for fast D0-D7 direction switching */
static uint32_t gpioa_moder_in;
static uint32_t gpioa_moder_out;

#define SET_DATA_MODE_IN   DATA_PORT->MODER = gpioa_moder_in
#define SET_DATA_MODE_OUT  DATA_PORT->MODER = gpioa_moder_out

#define GREEN_LED_OFF          OUTPUT_PORT->BSRR = (LED_Pin << 16U)
#define GREEN_LED_ON           OUTPUT_PORT->BSRR =  LED_Pin
#define INTERNAL_RAM_DISABLE   OUTPUT_PORT->BSRR = (m_EXSEL_Pin << 16U)
#define INTERNAL_RAM_ENABLE    OUTPUT_PORT->BSRR =  m_EXSEL_Pin
#define ATARI_RESET_ASSERT     OUTPUT_PORT->BSRR = (m_RST_Pin << 16U)
#define ATARI_RESET_DEASSERT   OUTPUT_PORT->BSRR =  m_RST_Pin

/* CONF0-2 DIP switches read from CONTROL_PORT bits [8:6] */
#define MEMORY_EXPANSION_TYPE  ((CONTROL_PORT->IDR & (0x7u << 6)) >> 6)

static uint8_t PORTB = 0xFF;
static uint8_t PBCTL = 0x00;
static uint8_t EMULATION_TYPE = EXPANSION_TYPE_NONE;

/* 64 banks × 16 KB = 1 MB in internal SRAM (section mapped in linker script) */
#define RAMSIZ (64 * 0x4000)
unsigned char expansion_ram[RAMSIZ] __attribute__((section(".sram")));

/* Bank lookup table: bank_lut[PORTB] → bank index, 0xFF = no external access */
static uint8_t bank_lut[256];

static int debuglevel = DBG_INFO;

void SystemClock_Config(void);
static void gpio_init(void);
static void usart_config(int baudrate);
static void init_bank_lut(t_expansion type);

/* ── System Clock ────────────────────────────────────────────────────────────
 *
 * HSE = 8 MHz
 * PLL1: DIVM=2 → VCO_in=4 MHz (range 2–4 MHz)
 *        MULN=125 → VCO=500 MHz (WIDE range: 192–836 MHz)
 *        DIVP=2  → SYSCLK=250 MHz
 * VOS0 required for SYSCLK > 200 MHz.
 * Flash latency: 5 WS @ VOS0, 3.3 V.
 * HCLK=250 MHz, PCLK1/2/3=125 MHz.
 *
 * NOTE: exact macro names (RCC_PLL1VCIRANGE_1, RCC_PLL1VCOWIDE, etc.) depend
 * on the STM32CubeH5 HAL version — verify against stm32h5xx_hal_rcc.h.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* VOS0 required for SYSCLK > 200 MHz (H5: macro + wait, no HAL call) */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
        ;

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLL1_SOURCE_HSE;
    osc.PLL.PLLM       = 2;     /* VCO_in = 8/2 = 4 MHz                 */
    osc.PLL.PLLN       = 125;   /* VCO    = 4 × 125 = 500 MHz           */
    osc.PLL.PLLP       = 2;     /* SYSCLK = 500 / 2 = 250 MHz           */
    osc.PLL.PLLQ       = 2;     /* PLL1Q  = 250 MHz (peripheral use)    */
    osc.PLL.PLLR       = 2;     /* PLL1R  = 250 MHz                     */
    osc.PLL.PLLRGE     = RCC_PLL1_VCIRANGE_1;    /* VCO input 2–4 MHz  */
    osc.PLL.PLLVCOSEL  = RCC_PLL1_VCORANGE_WIDE; /* VCO 192–836 MHz    */
    osc.PLL.PLLFRACN   = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler("OscConfig");

    clk.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                         RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 250 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 = 125 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;     /* PCLK2 = 125 MHz */
    clk.APB3CLKDivider = RCC_HCLK_DIV2;     /* PCLK3 = 125 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler("ClockConfig");
}

/* ── USART1 debug console ────────────────────────────────────────────────────
 * TODO: update DEBUG_TX_Pin, DEBUG_RX_Pin, DEBUG_AF in main.h
 *       for the H5E5ZJ PCB (USART1 TX/RX alternate function — typically AF7).
 */
static void usart_config(int baudrate)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};
    LL_GPIO_InitTypeDef  GPIO_InitStruct  = {0};

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA); /* TODO: update if TX/RX on another port */

    GPIO_InitStruct.Pin        = DEBUG_TX_Pin | DEBUG_RX_Pin;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate  = DEBUG_AF;
    LL_GPIO_Init(DEBUG_TX_GPIO_Port, &GPIO_InitStruct);

    USART_InitStruct.BaudRate            = (uint32_t)baudrate;
    USART_InitStruct.DataWidth           = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits            = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity              = LL_USART_PARITY_NONE;
    USART_InitStruct.TransferDirection   = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling        = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(USART1, &USART_InitStruct);
    LL_USART_ConfigAsyncMode(USART1);
    LL_USART_Enable(USART1);
}

/* ── GPIO init ───────────────────────────────────────────────────────────────
 * TODO: after PCB schematic is finalised, update:
 *   1. Port enables below (remove unused ports).
 *   2. Pin masks in each LL_GPIO_Init call if signals span multiple ports.
 *   3. All _Pin / _GPIO_Port defines in main.h.
 */
static void gpio_init(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* H5: GPIO clocks on AHB2 (was AHB1 on F4) */
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOD);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOE);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOF);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOG);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOH);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOI); /* TODO: verify GPIOI in H5E5ZJ LQFP144 */

    /* Outputs: default low */
    LL_GPIO_ResetOutputPin(OUTPUT_PORT, LED_Pin | PB6_Pin | PB9_Pin);
    /* Outputs: default high (inactive) */
    LL_GPIO_SetOutputPin(OUTPUT_PORT,
        m_RST_Pin | m_RD5_Pin | m_IRQ_Pin | m_RD4_Pin |
        m_EXSEL_Pin | m_HALT_Pin | m_MPD_Pin);

    /* Control inputs: PHI2, S5, S4, D1XX, CCTL, R/W, CONF0-2
     * TODO: split into multiple LL_GPIO_Init if signals span different ports */
    GPIO_InitStruct.Pin =
        m_PHI2_Pin | m_S5_Pin   | m_S4_Pin   |
        m_D1XX_Pin | m_CCTL_Pin | m_RW_Pin   |
        CONF0_Pin  | CONF1_Pin  | CONF2_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(CONTROL_PORT, &GPIO_InitStruct);

    /* Address bus A0-A15
     * TODO: split if address bits span multiple ports */
    GPIO_InitStruct.Pin =
        m_A0_Pin  | m_A1_Pin  | m_A2_Pin  | m_A3_Pin  |
        m_A4_Pin  | m_A5_Pin  | m_A6_Pin  | m_A7_Pin  |
        m_A8_Pin  | m_A9_Pin  | m_A10_Pin | m_A11_Pin |
        m_A12_Pin | m_A13_Pin | m_A14_Pin | m_A15_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(ADDR_PORT, &GPIO_InitStruct);

    /* Data bus D0-D7 (input initially; switched in main loop) */
    GPIO_InitStruct.Pin =
        m_D0_Pin | m_D1_Pin | m_D2_Pin | m_D3_Pin |
        m_D4_Pin | m_D5_Pin | m_D6_Pin | m_D7_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(DATA_PORT, &GPIO_InitStruct);

    /* Control outputs: LED, RST, RD4/5, EXSEL, HALT, MPD, IRQ
     * TODO: split if spread across multiple ports */
    GPIO_InitStruct.Pin =
        LED_Pin    | m_RST_Pin   | m_RD5_Pin  | m_IRQ_Pin  |
        m_RD4_Pin  | m_EXSEL_Pin | m_HALT_Pin | m_MPD_Pin  |
        PB6_Pin    | PB9_Pin;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_NO;
    LL_GPIO_Init(OUTPUT_PORT, &GPIO_InitStruct);
}

static void banner(t_expansion type)
{
    DBG_I("ABEX-MEGARAM BOARD ");
    switch (type)
    {
        case EXPANSION_TYPE_130XE:
            printf(ANSI_BLUE "130XE 128K EXPANSION\r\n" ANSI_RESET);         break;
        case EXPANSION_TYPE_192K:
            printf(ANSI_BLUE "192K (COMPYSHOP) EXPANSION\r\n" ANSI_RESET);   break;
        case EXPANSION_TYPE_256K_RAMBO:
            printf(ANSI_BLUE "256K RAMBO EXPANSION\r\n" ANSI_RESET);         break;
        case EXPANSION_TYPE_320K_COMPYSHOP:
            printf(ANSI_BLUE "320K COMPYSHOP EXPANSION\r\n" ANSI_RESET);     break;
        case EXPANSION_TYPE_320K:
            printf(ANSI_BLUE "320K (RAMBO) EXPANSION\r\n" ANSI_RESET);       break;
        case EXPANSION_TYPE_576K_MOD:
            printf(ANSI_BLUE "576K MOD EXPANSION\r\n" ANSI_RESET);           break;
        case EXPANSION_TYPE_576K_COMPYSHOP:
            printf(ANSI_BLUE "576K COMPYSHOP EXPANSION\r\n" ANSI_RESET);     break;
        case EXPANSION_TYPE_1088K_MOD:
            printf(ANSI_BLUE "1088K MOD EXPANSION\r\n" ANSI_RESET);          break;
        default:
            printf(ANSI_RED  "UNKNOWN EXPANSION TYPE!\r\n" ANSI_RESET);       break;
    }
    DBG_I("(C) RetroBit Lab 2019/2026 written by Gianluca Renzi <icjtqr@gmail.com>\r\n");
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
                b = !(p & 0x10) ? ((p & 0x0c) >> 2) : 0xFF;
                break;
            case EXPANSION_TYPE_192K:
                b = !(p & 0x10) ?
                    (((p & 0x0c) + ((p & 0x40) >> 2)) >> 2) : 0xFF;
                break;
            case EXPANSION_TYPE_256K_RAMBO: {
                uint8_t bk = (((p & 0x0c) + ((p & 0x60) >> 1)) >> 2);
                b = (p & 0x10) ? 0xFF : bk;
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

int main(void)
{
    uint16_t addr;
    uint8_t  data;
    uint8_t  c;
    uint8_t  watchdog = 0xff;

    /* Register-cached GPIO pointers for minimal-latency bus polling */
    register GPIO_TypeDef *gpioi = CONTROL_PORT;
    register GPIO_TypeDef *gpioc = ADDR_PORT;
    register GPIO_TypeDef *gpioa = DATA_PORT;

    _write_ready(SYSCALL_NOTREADY);

    HAL_Init();
    SystemClock_Config();

    gpio_init();
    ATARI_RESET_ASSERT;

    usart_config(115200);
    _write_ready(SYSCALL_READY);

    /* No FMC initialisation needed: expansion_ram[] is in internal SRAM */

    EMULATION_TYPE = MEMORY_EXPANSION_TYPE;
    init_bank_lut(EMULATION_TYPE);

    GREEN_LED_ON;
    memset(expansion_ram, 0, RAMSIZ);

    /* Precompute MODER masks for fast D0-D7 direction switching.
     * Bits [15:0] control PA0-PA7; bits [31:16] control PA8-PA15 (USART, keep). */
    gpioa_moder_in  = DATA_PORT->MODER & 0xFFFF0000U;
    gpioa_moder_out = gpioa_moder_in   | 0x00005555U;

    SET_DATA_MODE_IN;
    __disable_irq();

    banner(EMULATION_TYPE);
    HAL_Delay(500);

    GREEN_LED_OFF;
    ATARI_RESET_DEASSERT;

    while (1)
    {
        while (!((c = gpioi->IDR) & PHI2))
            ;
        addr = gpioc->IDR;

        switch (addr)
        {
            case 0xD303:
                GREEN_LED_ON;
                INTERNAL_RAM_DISABLE;
                if (!(c & RW))
                {
                    while (gpioi->IDR & PHI2)
                        ;
                    data = gpioa->IDR;
                    PBCTL = data;
                }
                else
                {
                    SET_DATA_MODE_OUT;
                    gpioa->ODR = PBCTL;
                    while (gpioi->IDR & PHI2)
                        ;
                    SET_DATA_MODE_IN;
                }
                INTERNAL_RAM_ENABLE;
                GREEN_LED_OFF;
                break;

            case 0xD301:
                GREEN_LED_ON;
                INTERNAL_RAM_DISABLE;
                if (!(c & RW))
                {
                    while (gpioi->IDR & PHI2)
                        ;
                    data = gpioa->IDR;
                    if (PBCTL & (1 << 2))
                        PORTB = data;
                }
                else
                {
                    SET_DATA_MODE_OUT;
                    gpioa->ODR = PORTB;
                    while (gpioi->IDR & PHI2)
                        ;
                    SET_DATA_MODE_IN;
                }
                INTERNAL_RAM_ENABLE;
                GREEN_LED_OFF;
                break;

            case 0xD1FE:
                if (!(c & RW))
                {
                    watchdog--;
                    if (watchdog == 0)
                    {
                        while (gpioi->IDR & PHI2)
                            ;
                        data = gpioa->IDR;
                        EMULATION_TYPE = data;
                        GREEN_LED_ON;
                        ATARI_RESET_ASSERT;
                        banner(EMULATION_TYPE);
                        init_bank_lut(EMULATION_TYPE);
                        PORTB = 0xFF;
                        PBCTL = 0x00;
                        HAL_Delay(500);
                        GREEN_LED_OFF;
                        ATARI_RESET_DEASSERT;
                        watchdog = 0xff;
                    }
                }
                else
                {
                    watchdog = 0xff;
                }
                break;

            default:
                if (addr >= 0x4000 && addr < 0x8000)
                {
                    if (PBCTL & (1 << 2))
                    {
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
                                SET_DATA_MODE_OUT;
                                gpioa->ODR = expansion_ram[internal_address];
                                while (gpioi->IDR & PHI2)
                                    ;
                                SET_DATA_MODE_IN;
                            }
                            GREEN_LED_OFF;
                        }
                        INTERNAL_RAM_ENABLE;
                    }
                }
                break;
        }
    }

    return 0;
}

void Error_Handler(const char *c)
{
    DBG_E("%s\n\r", c);
    for (;;)
        ;
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    DBG_E("Assert failed: %s line %lu\r\n", file, (unsigned long)line);
    for (;;)
        ;
}
#endif
