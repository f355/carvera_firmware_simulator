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

#ifndef SIMULATOR_LPC17XX_PINSEL_H
#define SIMULATOR_LPC17XX_PINSEL_H

#include "LPC17xx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PINSEL_PINMODE_NORMAL 0
#define PINSEL_PINMODE_OPENDRAIN 1
#define PINSEL_PINMODE_PULLUP 0
#define PINSEL_PINMODE_TRISTATE 2
#define PINSEL_PINMODE_PULLDOWN 3

typedef struct {
  uint8_t Portnum;
  uint8_t Pinnum;
  uint8_t Funcnum;
  uint8_t Pinmode;
  uint8_t OpenDrain;
} PINSEL_CFG_Type;

void PINSEL_ConfigPin(PINSEL_CFG_Type* PinCfg);

#ifdef __cplusplus
}
#endif

#endif
