/****************************************************************************
 * boards/arm/stm32n6/atk-dnn647/src/fsbl_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * First Stage Boot Loader (FSBL) for ATK-DNN647 board.
 *
 * This FSBL runs from SRAM2 at 0x34180400 after the STM32N6 boot ROM
 * loads it from external flash.  It performs the following tasks:
 *
 *   1. Configure VDD voltage domains (VDDIO2/3/4)
 *   2. Configure SMPS supply and voltage scaling
 *   3. Initialize XSPI2 flash controller (MX25UM25645G, 32MB)
 *   4. Switch flash to DTR (Double Transfer Rate) mode
 *   5. Enable memory-mapped mode for XIP at 0x70000000
 *   6. Jump to the NuttX application at 0x70080000
 *
 * LED Status Indication (PG10, active low):
 *   - 1 blink:  VDD configuration done
 *   - 2 blinks: Power configuration done
 *   - 3 blinks: XSPI initialization done
 *   - LED on:   Ready to jump to application
 *
 * Reference: demo/atk-dnn647-micropython/ports/stm32/boards/ATK_DNN647/board.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <string.h>

#include "arm_internal.h"
#include "stm32.h"
#include "hardware/stm32n6xxx_rcc.h"
#include "hardware/stm32n6xxx_pwr.h"
#include "hardware/stm32n6xxx_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Application entry point in external flash (XIP) */

#define APPLICATION_ADDR  0x70080000

/* LED definitions (active low, PG10) */

#define LED_GPIO_BASE     STM32_GPIOG_BASE
#define LED_PIN           10
#define LED_RCC_EN        (1 << 5)  /* GPIOGEN - bit 5 in AHB4ENSR */

/* LED macros (active low: 0 = on, 1 = off) */

#define LED_ON()   putreg32((1 << (LED_PIN + 16)), LED_GPIO_BASE + STM32_GPIO_BSRR_OFFSET)
#define LED_OFF()  putreg32((1 << LED_PIN), LED_GPIO_BASE + STM32_GPIO_BSRR_OFFSET)

/* Simple delay - approximately 500ms at 64MHz (HSI default)
 * Each loop iteration takes ~4-5 cycles at 64MHz.
 * 6,000,000 * 5 cycles / 64MHz ≈ 469ms
 */

#define DELAY_500MS() do { volatile uint32_t d = 6000000; while (d-- > 0); } while(0)

/* XSPI flash configuration */

#define XSPI_FLASH_SIZE_BITS_LOG2  28  /* log2(32MB * 8 bits) = 28 */

/* XSPI2 pins: PN0-PN12, AF9 */

#define XSPI2_AF  9

/* XSPI register base addresses */

#define XSPI2_BASE        0x46001000
#define XSPIM_BASE        0x46002000

/* XSPI register offsets */

#define XSPI_CR_OFFSET    0x000
#define XSPI_DCR1_OFFSET  0x008
#define XSPI_DCR2_OFFSET  0x00C
#define XSPI_DCR3_OFFSET  0x010
#define XSPI_DCR4_OFFSET  0x014
#define XSPI_SR_OFFSET    0x020
#define XSPI_FCR_OFFSET   0x024
#define XSPI_DLR_OFFSET   0x040
#define XSPI_CCR_OFFSET   0x100
#define XSPI_TCR_OFFSET   0x108
#define XSPI_IR_OFFSET    0x110
#define XSPI_AR_OFFSET    0x118
#define XSPI_DR_OFFSET    0x120
#define XSPI_LPTR_OFFSET  0x130

/* XSPIM register offsets */

#define XSPIM_CR_OFFSET   0x000

/* XSPI CR register bits */

#define XSPI_CR_EN        (1 << 0)
#define XSPI_CR_FMODE_Pos 28

/* XSPI SR register bits */

#define XSPI_SR_TCF       (1 << 4)
#define XSPI_SR_FTF       (1 << 5)
#define XSPI_SR_BUSY      (1 << 5)
#define XSPI_SR_FLEVEL_Pos 8

/* XSPI FCR register bits */

#define XSPI_FCR_CTCF     (1 << 4)

/* XSPI CCR register bits */

#define XSPI_CCR_IMODE_Pos  0
#define XSPI_CCR_IDTR_Pos   3
#define XSPI_CCR_ISIZE_Pos  4
#define XSPI_CCR_ADMODE_Pos 8
#define XSPI_CCR_ADDTR_Pos  12
#define XSPI_CCR_ADSIZE_Pos 14
#define XSPI_CCR_DMODE_Pos  24
#define XSPI_CCR_DDTR_Pos   27
#define XSPI_CCR_DQSE_Pos   29

/* XSPI TCR register bits */

#define XSPI_TCR_DCYC_Pos  0

/* XSPI DCR1 register bits */

#define XSPI_DCR1_CKMODE_Pos 0
#define XSPI_DCR1_CSHT_Pos   8
#define XSPI_DCR1_DEVSIZE_Pos 16
#define XSPI_DCR1_MTYP_Pos   24

/* XSPI DCR2 register bits */

#define XSPI_DCR2_PRESCALER_Pos 0

/* Flash commands for MX25UM25645G */

#define CMD_WREN          0x06
#define CMD_RDSR          0x05

/* DTR mode commands */

#define CMD_8DTRD         0xee11

/* RCC register offsets from STM32N6 reference manual */

#define RCC_AHB5ENSR_OFFSET   0x0A60  /* AHB5 enable set register */
#define RCC_AHB5RSTR_OFFSET   0x0220  /* AHB5 reset register */
#define RCC_AHB5RSTCR_OFFSET  0x1220  /* AHB5 reset clear register */
#define RCC_CCIPR6_OFFSET     0x0158  /* Clock config for independent peripheral reg 6 */

/* RCC bit definitions - corrected from STM32N6 reference manual */

#define RCC_AHB5ENSR_XSPIMEN  (1 << 13)  /* XSPIM enable */
#define RCC_AHB5ENSR_XSPI2EN  (1 << 12)  /* XSPI2 enable */
#define RCC_AHB5RSTR_XSPIMRST (1 << 13)  /* XSPIM reset */
#define RCC_AHB5RSTR_XSPI2RST (1 << 12)  /* XSPI2 reset */

/* NVIC vector table register */

#define NVIC_VECTAB  0xe000ed08

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* XSPI register structure */

typedef struct
{
  volatile uint32_t cr;
  volatile uint32_t reserved1;
  volatile uint32_t dcr1;
  volatile uint32_t dcr2;
  volatile uint32_t dcr3;
  volatile uint32_t dcr4;
  volatile uint32_t reserved2[2];
  volatile uint32_t sr;
  volatile uint32_t fcr;
  volatile uint32_t reserved3[6];
  volatile uint32_t dlr;
  volatile uint32_t reserved4[7];
  volatile uint32_t ccr;
  volatile uint32_t reserved5;
  volatile uint32_t tcr;
  volatile uint32_t reserved6;
  volatile uint32_t ir;
  volatile uint32_t reserved7;
  volatile uint32_t ar;
  volatile uint32_t reserved8;
  volatile uint32_t dr;
  volatile uint32_t reserved9[3];
  volatile uint32_t lptr;
} xspi_regs_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* XSPI register pointers */

static volatile xspi_regs_t * const xspi2 =
    (volatile xspi_regs_t *)XSPI2_BASE;

static volatile uint32_t * const xspim_cr =
    (volatile uint32_t *)(XSPIM_BASE + XSPIM_CR_OFFSET);

/* RCC addresses */

static const uint32_t rcc_ahb5ensr = STM32_RCC_BASE + RCC_AHB5ENSR_OFFSET;
static const uint32_t rcc_ccipr6 = STM32_RCC_BASE + RCC_CCIPR6_OFFSET;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: led_init
 *
 * Description:
 *   Initialize LED0 (PG10) for status indication.
 *   LED0 is active low (low = on, high = off).
 *
 ****************************************************************************/

static void led_init(void)
{
  uint32_t reg;

  /* Enable GPIO G clock */

  reg = getreg32(STM32_RCC_AHB4ENSR);
  reg |= LED_RCC_EN;
  putreg32(reg, STM32_RCC_AHB4ENSR);

  /* Configure PG10 as general purpose output, very high speed, no pull */

  reg = getreg32(STM32_GPIOG_MODER);
  reg &= ~(3 << (LED_PIN * 2));
  reg |= (1 << (LED_PIN * 2));  /* Output mode (0x1) */
  putreg32(reg, STM32_GPIOG_MODER);

  reg = getreg32(STM32_GPIOG_OSPEED);
  reg &= ~(3 << (LED_PIN * 2));
  reg |= (3 << (LED_PIN * 2));  /* Very high speed (0x3) */
  putreg32(reg, STM32_GPIOG_OSPEED);

  reg = getreg32(STM32_GPIOG_PUPDR);
  reg &= ~(3 << (LED_PIN * 2));  /* No pull (0x0) */
  putreg32(reg, STM32_GPIOG_PUPDR);

  /* Start with LED off */

  LED_OFF();
}

/****************************************************************************
 * Name: fsbl_config_vdd
 *
 * Description:
 *   Configure VDD voltage domains for the DNN647 board.
 *
 ****************************************************************************/

static void fsbl_config_vdd(void)
{
  /* Configure VDDIO2 (3.3V) */

  modifyreg32(STM32_PWR_SVMCR3, 0, PWR_SVMCR3_VDDIO2SV);

  /* Configure VDDIO3 (1.8V) */

  modifyreg32(STM32_PWR_SVMCR3, 0, PWR_SVMCR3_VDDIO3SV | PWR_SVMCR3_VDDIO3VRSEL);

  /* VDDIO4 already enabled in fsbl_main before LED init */
}

/****************************************************************************
 * Name: fsbl_config_power
 *
 * Description:
 *   Configure SMPS supply and voltage scaling.
 *
 ****************************************************************************/

static void fsbl_config_power(void)
{
  /* Set voltage scaling to high performance */

  modifyreg32(STM32_PWR_VOSCR, 0, PWR_VOSCR_VOS);
  while (!(getreg32(STM32_PWR_VOSCR) & PWR_VOSCR_VOSRDY))
    {
    }
}

/****************************************************************************
 * Name: xspi_pin_config
 *
 * Description:
 *   Configure XSPI2 pins (PN0-PN12) for flash access.
 *
 ****************************************************************************/

static void xspi_pin_config(void)
{
  uint32_t reg;
  uint32_t moder;
  uint32_t ospeedr;
  uint32_t pupdr;
  uint32_t afrl;
  uint32_t afrh;
  int pin;

  /* Enable GPIO N clock */

  reg = getreg32(STM32_RCC_AHB4ENSR);
  reg |= (1 << 14);  /* GPIONEN */
  putreg32(reg, STM32_RCC_AHB4ENSR);

  /* Configure PN0-PN12 as AF9 (XSPI2), very high speed, no pull */

  moder = getreg32(STM32_GPION_MODER);
  ospeedr = getreg32(STM32_GPION_OSPEED);
  pupdr = getreg32(STM32_GPION_PUPDR);
  afrl = getreg32(STM32_GPION_AFRL);
  afrh = getreg32(STM32_GPION_AFRH);

  for (pin = 0; pin <= 12; pin++)
    {
      /* MODER: AF mode (0x2) */

      moder &= ~(3 << (pin * 2));
      moder |= (2 << (pin * 2));

      /* OSPEEDR: Very High speed (0x3) */

      ospeedr &= ~(3 << (pin * 2));
      ospeedr |= (3 << (pin * 2));

      /* PUPDR: No pull (0x0) */

      pupdr &= ~(3 << (pin * 2));

      /* AFR: AF9 */

      if (pin < 8)
        {
          afrl &= ~(15 << (pin * 4));
          afrl |= (XSPI2_AF << (pin * 4));
        }
      else
        {
          afrh &= ~(15 << ((pin - 8) * 4));
          afrh |= (XSPI2_AF << ((pin - 8) * 4));
        }
    }

  putreg32(moder, STM32_GPION_MODER);
  putreg32(ospeedr, STM32_GPION_OSPEED);
  putreg32(pupdr, STM32_GPION_PUPDR);
  putreg32(afrl, STM32_GPION_AFRL);
  putreg32(afrh, STM32_GPION_AFRH);
}

/****************************************************************************
 * Name: xspi_write_111
 *
 * Description:
 *   Write data to flash using 1-1-1 SPI mode.
 *
 ****************************************************************************/

static void xspi_write_111(uint8_t cmd, uint32_t addr, int addr_en,
                            const uint8_t *data, size_t len)
{
  uint32_t ccr;

  ccr = (1 << XSPI_CCR_DMODE_Pos)   /* data on 1 line */
      | (3 << XSPI_CCR_ADSIZE_Pos)  /* 32-bit address */
      | ((addr_en ? 1 : 0) << XSPI_CCR_ADMODE_Pos)
      | (1 << XSPI_CCR_IMODE_Pos);  /* instruction on 1 line */

  /* Indirect write mode */

  xspi2->cr = (xspi2->cr & ~(3 << XSPI_CR_FMODE_Pos)) | (0 << XSPI_CR_FMODE_Pos);

  xspi2->ccr = ccr;
  xspi2->tcr = 0;  /* no dummy cycles */

  if (len > 0)
    {
      xspi2->dlr = len - 1;
    }

  xspi2->ir = cmd;

  if (addr_en)
    {
      xspi2->ar = addr;
    }

  /* Write data */

  while (len-- > 0)
    {
      while (!(xspi2->sr & XSPI_SR_FTF))
        {
        }

      *(volatile uint8_t *)&xspi2->dr = *data++;
    }

  /* Wait for completion */

  while (!(xspi2->sr & XSPI_SR_TCF))
    {
    }

  xspi2->fcr = XSPI_FCR_CTCF;

  while (xspi2->sr & XSPI_SR_BUSY)
    {
    }
}

/****************************************************************************
 * Name: xspi_read_111
 *
 * Description:
 *   Read data from flash using 1-1-1 SPI mode.
 *
 ****************************************************************************/

static void xspi_read_111(uint8_t cmd, uint32_t addr, int addr_en,
                           uint8_t *data, size_t len)
{
  uint32_t ccr;

  ccr = (1 << XSPI_CCR_DMODE_Pos)   /* data on 1 line */
      | (3 << XSPI_CCR_ADSIZE_Pos)  /* 32-bit address */
      | ((addr_en ? 1 : 0) << XSPI_CCR_ADMODE_Pos)
      | (1 << XSPI_CCR_IMODE_Pos);  /* instruction on 1 line */

  /* Indirect read mode */

  xspi2->cr = (xspi2->cr & ~(3 << XSPI_CR_FMODE_Pos)) | (1 << XSPI_CR_FMODE_Pos);

  xspi2->ccr = ccr;
  xspi2->tcr = 0;  /* no dummy cycles */
  xspi2->dlr = len - 1;
  xspi2->ir = cmd;

  if (addr_en)
    {
      xspi2->ar = addr;
    }

  /* Read data */

  while (len-- > 0)
    {
      while (!((xspi2->sr >> XSPI_SR_FLEVEL_Pos) & 0x3f))
        {
        }

      *data++ = *(volatile uint8_t *)&xspi2->dr;
    }

  xspi2->fcr = XSPI_FCR_CTCF;
}

/****************************************************************************
 * Name: xspi_switch_to_dtr
 *
 * Description:
 *   Switch the MX25UM25645G flash from SPI mode to DTR (DOPI) mode.
 *
 ****************************************************************************/

static void xspi_switch_to_dtr(void)
{
  uint8_t buf[4];

  /* Send WREN (Write Enable) */

  xspi_write_111(CMD_WREN, 0, 0, NULL, 0);

  /* Wait for WEL bit */

  for (int i = 0; i < 100; i++)
    {
      xspi_read_111(CMD_RDSR, 0, 0, buf, 1);
      if (buf[0] & 0x02)
        {
          break;
        }
    }

  /* Write Configuration Register 2 to switch to DOPI mode */

  buf[0] = 0x02;  /* DOPI mode */
  xspi_write_111(0x72, 0x00000000, 1, buf, 1);
}

/****************************************************************************
 * Name: xspi_memory_map_888
 *
 * Description:
 *   Enable memory-mapped mode for XIP using 8-8-8 DTR read.
 *
 ****************************************************************************/

static void xspi_memory_map_888(void)
{
  uint32_t ccr;

  ccr = (1 << XSPI_CCR_DQSE_Pos)    /* DQS enabled */
      | (1 << XSPI_CCR_DDTR_Pos)     /* data DTR */
      | (4 << XSPI_CCR_DMODE_Pos)    /* data on 8 lines */
      | (3 << XSPI_CCR_ADSIZE_Pos)   /* 32-bit address */
      | (1 << XSPI_CCR_ADDTR_Pos)    /* address DTR */
      | (4 << XSPI_CCR_ADMODE_Pos)   /* address on 8 lines */
      | (1 << XSPI_CCR_ISIZE_Pos)    /* 16-bit instruction */
      | (1 << XSPI_CCR_IDTR_Pos)     /* instruction DTR */
      | (4 << XSPI_CCR_IMODE_Pos);   /* instruction on 8 lines */

  xspi2->ccr = ccr;
  xspi2->tcr = 20 << XSPI_TCR_DCYC_Pos;  /* 20 dummy cycles */
  xspi2->ir = CMD_8DTRD;
  xspi2->lptr = 1024;  /* timeout period */

  /* Enable memory-mapped mode */

  xspi2->cr = (xspi2->cr & ~(3 << XSPI_CR_FMODE_Pos)) | (3 << XSPI_CR_FMODE_Pos);
}

/****************************************************************************
 * Name: xspi_init
 *
 * Description:
 *   Initialize XSPI2 for flash access and enable memory-mapped mode.
 *
 ****************************************************************************/

static void xspi_init(void)
{
  uint32_t reg;

  /* Configure XSPI pins */

  xspi_pin_config();

  /* Blink to show pin config done */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Ensure XSPIM and XSPI2 are not in reset state */

  reg = getreg32(STM32_RCC_BASE + RCC_AHB5RSTR_OFFSET);
  reg &= ~(RCC_AHB5RSTR_XSPIMRST | RCC_AHB5RSTR_XSPI2RST);
  putreg32(reg, STM32_RCC_BASE + RCC_AHB5RSTR_OFFSET);

  /* Enable XSPI clocks */

  reg = getreg32(rcc_ahb5ensr);
  reg |= RCC_AHB5ENSR_XSPIMEN | RCC_AHB5ENSR_XSPI2EN;
  putreg32(reg, rcc_ahb5ensr);

  /* Set XSPI2 clock source to HCLK */

  reg = getreg32(rcc_ccipr6);
  reg &= ~(3 << 4);
  reg |= (0 << 4);  /* HCLK = 0 */
  putreg32(reg, rcc_ccipr6);

  /* Small delay for clock stabilization */

  volatile uint32_t delay = 1000;
  while (delay-- > 0);

  /* Blink to show clock config done */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Disable XSPI2 before configuring XSPIM */

  xspi2->cr &= ~XSPI_CR_EN;

  /* Configure XSPIM in direct mode */

  *xspim_cr = 0;

  /* Configure XSPI2:
   * - Macronix memory type
   * - 32MB flash size
   * - CS high time: 2 cycles
   * - CLK idles low
   */

  xspi2->dcr1 = (1 << XSPI_DCR1_MTYP_Pos)
               | ((XSPI_FLASH_SIZE_BITS_LOG2 - 3 - 1) << XSPI_DCR1_DEVSIZE_Pos)
               | (1 << XSPI_DCR1_CSHT_Pos)
               | (0 << XSPI_DCR1_CKMODE_Pos);

  /* Prescaler: F_CLK = F_AHB / 4 */

  xspi2->dcr2 = (4 - 1) << XSPI_DCR2_PRESCALER_Pos;
  xspi2->dcr3 = 0;
  xspi2->dcr4 = 0;
  xspi2->tcr = 0;

  /* Enable XSPI2 */

  xspi2->cr |= XSPI_CR_EN;

  /* Blink to show XSPI enabled */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Reconfigure XSPIM */

  xspi2->cr &= ~XSPI_CR_EN;
  *xspim_cr = 0;
  xspi2->cr |= XSPI_CR_EN;

  /* Switch flash to DTR mode */

  xspi_switch_to_dtr();

  /* Blink to show DTR mode done */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Enable memory-mapped mode (8DTRD) */

  xspi_memory_map_888();
}

/****************************************************************************
 * Name: fsbl_jump_to_app
 *
 * Description:
 *   Jump to the application at the given address.
 *
 ****************************************************************************/

static void fsbl_jump_to_app(uint32_t app_addr)
{
  uint32_t msp;
  uint32_t reset_handler;

  /* Validate application: check that the reset handler address is valid */

  msp = *(volatile uint32_t *)app_addr;
  reset_handler = *(volatile uint32_t *)(app_addr + 4);

  /* Check that MSP looks reasonable (in SRAM range 0x34xxxxxx) and reset
   * handler is in the XIP flash range (0x70xxxxxx)
   */

  if ((msp & 0xff000000) != 0x34000000 ||
      (reset_handler & 0xff000000) != 0x70000000)
    {
      /* Invalid application, hang with LED blinking fast to indicate error */

      for (; ; )
        {
          LED_ON();
          for (volatile uint32_t d = 0; d < 50000; d++);
          LED_OFF();
          for (volatile uint32_t d = 0; d < 50000; d++);
        }
    }

  /* Set the vector table to the application's location */

  putreg32(app_addr, NVIC_VECTAB);

  /* On ARMv8.1-M, set MSPLIM before setting MSP to avoid unwanted
   * stack overflow faults.  The boot ROM leaves MSPLIM set to a
   * value that may be above the application's initial MSP.
   */

  __asm__ volatile (
      "mov r0, #0\n"
      "msr msplim, r0\n"
      "msr psplim, r0\n"
      "msr msp, %0\n"
      "bx %1\n"
      :
      : "r" (msp), "r" (reset_handler)
      : "r0"
  );

  /* Should not reach here */

  for (; ; )
    {
    }
}

/****************************************************************************
 * Board Stubs - minimal implementations for FSBL
 ****************************************************************************/

/* Board initialization stub */

void stm32_board_initialize(void)
{
  /* Nothing to do for FSBL */
}

/* LED stubs */

void board_autoled_on(int led)
{
  (void)led;
}

void board_autoled_off(int led)
{
  (void)led;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: fsbl_main
 *
 * Description:
 *   Main entry point for the FSBL.
 *
 ****************************************************************************/

int fsbl_main(int argc, char *argv[])
{
  /* Disable interrupts */

  up_irq_disable();

  /* Enable PWR clock first (needed for VDD configuration) */

  modifyreg32(STM32_RCC_AHB4ENSR, 0, (1 << 16));  /* PWREN */

  /* Enable VDDIO4 (3.3V) for GPIO G */

  modifyreg32(STM32_PWR_SVMCR1, 0, PWR_SVMCR1_VDDIO4SV);

  /* Now initialize LED for status indication */

  led_init();

  /* Blink LED0 twice to show FSBL started */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();
  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Step 1: Configure VDD */

  fsbl_config_vdd();

  /* Blink LED0 twice to show VDD done */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();
  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Step 2: Configure power */

  fsbl_config_power();

  /* Blink LED0 twice to show power done */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();
  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Step 3: Initialize XSPI */

  xspi_init();

  /* Blink LED0 twice to show XSPI done */

  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();
  LED_ON();  DELAY_500MS();
  LED_OFF(); DELAY_500MS();

  /* Step 4: Jump to application */

  fsbl_jump_to_app(APPLICATION_ADDR);

  /* Should not reach here */

  return 0;
}
