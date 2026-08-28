/****************************************************************************
 * boards/arm/stm32n6/atk-dnn647/src/stm32_boot.c
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
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <debug.h>

#include <nuttx/board.h>

#include "arm_internal.h"
#include "stm32_gpio.h"
#include "dnn647.h"

#include <arch/board/board.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_board_initialize
 *
 * Description:
 *   All STM32 architectures must provide the following entry point.  This
 *   entry point is called early in the initialization -- after all memory
 *   has been configured and mapped but before any devices
 *   have been initialized.
 *
 ****************************************************************************/

/* Simple delay for LED blinking - approximate at 600MHz */

#define LED_DELAY() \
  do { \
    volatile uint32_t _i; \
    for (_i = 0; _i < 20000000; _i++); \
  } while (0)

void stm32_board_initialize(void)
{
#ifdef CONFIG_ARCH_LEDS
  /* Configure on-board LEDs if LED support has been selected. */

  board_autoled_initialize();
#endif

  /* Configure LED1 (PE10) as output and blink it continuously.
   * This proves the NuttX application has started executing from XIP flash.
   */

  stm32_configgpio(GPIO_LED1);
  stm32_gpiowrite(GPIO_LED1, true);  /* LED1 OFF initially (active low) */

  for (; ; )
    {
      stm32_gpiowrite(GPIO_LED1, false);  /* LED1 ON  (active low) */
      LED_DELAY();
      stm32_gpiowrite(GPIO_LED1, true);   /* LED1 OFF (active low) */
      LED_DELAY();
    }
}

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   If CONFIG_BOARD_LATE_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_late_initialize().  board_late_initialize() will
 *   be called immediately after up_initialize() is called and just before
 *   the initial application is started.  This additional initialization
 *   phase may be used, for example, to initialize board-specific device
 *   drivers.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  /* Perform board-specific initialization here if so configured */

  stm32_bringup();
}
#endif

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is called
 *   by boardctl() when the BOARDIOC_INIT command is received.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARDCTL
int board_app_initialize(uintptr_t arg)
{
  return OK;
}
#endif
