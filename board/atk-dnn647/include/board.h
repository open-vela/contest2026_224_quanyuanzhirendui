/****************************************************************************
 * boards/arm/stm32n6/atk-dnn647/include/board.h
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

#ifndef __BOARDS_ARM_STM32N6_ATK_DNN647_INCLUDE_BOARD_H
#define __BOARDS_ARM_STM32N6_ATK_DNN647_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#ifndef __ASSEMBLY__
#  include <stdint.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Clocking *****************************************************************/

/* Clock tree (PLL1 fed from the external 48 MHz HSE crystal):
 *
 *   HSE 48 MHz / M=6 * N=100 = 800 MHz VCO
 *     IC1  /4 = 200 MHz  -> CPU clock (CPUSW)
 *   HPRE /4  = 50 MHz   -> HCLK
 *   PPRE1 /1 = 50 MHz   -> PCLK1
 *   PPRE2 /1 = 50 MHz   -> PCLK2
 *
 * Works with the default VOS SCALE1, no SMPS overdrive required.
 */

#define STM32_HSE_FREQUENCY     48000000ul

#define STM32_PLL1_M            6
#define STM32_PLL1_N            100
#define STM32_PLL1_IC1_DIV      4

#define STM32_CPUCLK_FREQUENCY  200000000ul
#define STM32_SYSCLK_FREQUENCY  (STM32_CPUCLK_FREQUENCY / 2)
#define STM32_HCLK_FREQUENCY    (STM32_CPUCLK_FREQUENCY / 4)
#define STM32_PCLK1_FREQUENCY   STM32_HCLK_FREQUENCY
#define STM32_PCLK2_FREQUENCY   STM32_HCLK_FREQUENCY

/* Timer input clock = SYSCLK (TIMPRE=0 default) */

#define STM32_APB1_TIM_FREQUENCY STM32_SYSCLK_FREQUENCY
#define STM32_APB2_TIM_FREQUENCY STM32_SYSCLK_FREQUENCY

/* I/O voltage domains ******************************************************/

/* The DNN647 has multiple I/O voltage domains:
 *   VDDIO2 = 3.3V (most GPIOs)
 *   VDDIO3 = 1.8V (PN/PO/PP ports)
 *   VDDIO4 = 3.3V (PG port)
 */

#define BOARD_PWR_VDDIO  (PWR_SVMCR3_VDDIO2SV    | PWR_SVMCR3_VDDIO3SV | \
                          PWR_SVMCR3_VDDIO2VRSEL | PWR_SVMCR3_VDDIO3VRSEL)

/* LED definitions **********************************************************/

/* The ATK-DNN647 has two user LEDs:
 *
 *   LED0  PG10  (active low)
 *   LED1  PE10  (active low)
 *
 * They are not used by the board port unless CONFIG_ARCH_LEDS is defined.
 */

/* LED index values for use with board_userled() */

#define BOARD_LED1        0
#define BOARD_LED2        1
#define BOARD_NLEDS       2

#define BOARD_LED_RED     BOARD_LED1
#define BOARD_LED_GREEN   BOARD_LED2

/* LED bits for use with board_userled_all() */

#define BOARD_LED1_BIT    (1 << BOARD_LED1)
#define BOARD_LED2_BIT    (1 << BOARD_LED2)

/* If CONFIG_ARCH_LEDS is defined, the LEDs are used to encode OS-related
 * events as follows:
 *
 *   SYMBOL                     Meaning                      LED state
 *                                                        LED0   LED1
 *   ----------------------  --------------------------  ------ ------
 */

#define LED_STARTED        0 /* NuttX has been started   OFF    OFF   */
#define LED_HEAPALLOCATE   1 /* Heap has been allocated  OFF    ON    */
#define LED_IRQSENABLED    2 /* Interrupts enabled       ON     OFF   */
#define LED_STACKCREATED   3 /* Idle stack created       ON     ON    */
#define LED_INIRQ          4 /* In an interrupt          N/C    GLOW  */
#define LED_SIGNAL         5 /* In a signal handler      GLOW   N/C   */
#define LED_ASSERTION      6 /* An assertion failed      GLOW   GLOW  */
#define LED_PANIC          7 /* The system has crashed   Blink  N/C   */
#define LED_IDLE           8 /* MCU is in sleep mode     ON     OFF   */

/* Alternate function pin selections ****************************************/

/* USART1 GPIOs *************************************************************/

/* USART1 (DNN647 console header): PE5=TX (AF7), PE6=RX (AF7) */

#define GPIO_USART1_TX   GPIO_USART1_TX_1
#define GPIO_USART1_RX   GPIO_USART1_RX_1

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_board_initialize
 *
 * Description:
 *   All STM32N6 architectures must provide the following entry point.
 *   This entry point is called early in the initialization -- after all
 *   memory has been configured and mapped but before any devices
 *   have been initialized.
 *
 ****************************************************************************/

void stm32_board_initialize(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __BOARDS_ARM_STM32N6_ATK_DNN647_INCLUDE_BOARD_H */
