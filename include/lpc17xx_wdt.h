/*
 * This file is part of the Carvera Firmware Simulator.
 *
 * Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SIMULATOR_LPC17XX_WDT_H
#define SIMULATOR_LPC17XX_WDT_H

#include <stdint.h>

#include "LPC17xx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WDT_WDMOD_WDEN ((uint32_t)(1u << 0))
#define WDT_WDMOD_WDRESET ((uint32_t)(1u << 1))
#define WDT_WDMOD_WDTOF ((uint32_t)(1u << 2))
#define WDT_WDMOD_WDINT ((uint32_t)(1u << 3))

typedef enum {
  WDT_CLKSRC_IRC = 0,
  WDT_CLKSRC_PCLK = 1,
  WDT_CLKSRC_RTC = 2,
} WDT_CLK_OPT;

typedef enum {
  WDT_MODE_INT_ONLY = 0,
  WDT_MODE_RESET = 1,
} WDT_MODE_OPT;

typedef enum {
  RESET = 0,
  SET = !RESET,
} FlagStatus;

void WDT_Init(WDT_CLK_OPT ClkSrc, WDT_MODE_OPT WDTMode);
void WDT_Start(uint32_t TimeOut);
void WDT_Feed(void);
void WDT_UpdateTimeOut(uint32_t TimeOut);
FlagStatus WDT_ReadTimeOutFlag(void);
void WDT_ClrTimeOutFlag(void);
uint32_t WDT_GetCurrentCount(void);

#ifdef __cplusplus
}
#endif

#endif
