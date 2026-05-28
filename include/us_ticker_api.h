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

#ifndef SIMULATOR_US_TICKER_API_H
#define SIMULATOR_US_TICKER_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t us_ticker_read(void);

typedef void (*ticker_event_handler)(uint32_t id);
void us_ticker_set_handler(ticker_event_handler handler);

typedef struct ticker_event_s {
  uint32_t timestamp;
  uint32_t id;
  struct ticker_event_s* next;
} ticker_event_t;

void us_ticker_insert_event(ticker_event_t* obj, unsigned int timestamp, uint32_t id);
void us_ticker_remove_event(ticker_event_t* obj);

#ifdef __cplusplus
}
#endif

#endif
