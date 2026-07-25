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

#ifndef SIMULATOR_LPC17XX_H
#define SIMULATOR_LPC17XX_H

#include <stdint.h>

#define __I volatile const
#define __O volatile
#define __IO volatile

#include <stdbool.h>

typedef enum { DISABLE = 0, ENABLE = 1 } FunctionalState;

typedef enum IRQn {
  PendSV_IRQn = -2,
  WDT_IRQn = 0,
  TIMER0_IRQn = 1,
  TIMER1_IRQn = 2,
  TIMER2_IRQn = 3,
  TIMER3_IRQn = 4,
  UART0_IRQn = 5,
  UART1_IRQn = 6,
  UART2_IRQn = 7,
  UART3_IRQn = 8,
  EINT3_IRQn = 21,
  ADC_IRQn = 22,
  USB_IRQn = 24,
} IRQn_Type;

typedef struct {
  __IO uint32_t PCONP;
  __IO uint32_t PCLKSEL0;
  __IO uint32_t PCLKSEL1;
  __IO uint32_t PLL0CFG;
  __IO uint32_t CCLKCFG;
} LPC_SC_TypeDef;

struct LPC_TIM_TypeDef;

struct LPC_TIM_ControlRegister {
  uint32_t value;
  LPC_TIM_TypeDef* owner;

  void bind(LPC_TIM_TypeDef* timer);
  LPC_TIM_ControlRegister& operator=(uint32_t bits);
  LPC_TIM_ControlRegister& operator|=(uint32_t bits);
  operator uint32_t() const { return value; }
};

typedef struct LPC_TIM_TypeDef {
  __IO uint32_t IR;
  LPC_TIM_ControlRegister TCR;
  __IO uint32_t TC;
  __IO uint32_t PR;
  __IO uint32_t PC;
  __IO uint32_t MCR;
  __IO uint32_t MR0;
  __IO uint32_t MR1;
  __IO uint32_t MR2;
  __IO uint32_t MR3;
} LPC_TIM_TypeDef;

struct LPC_GPIO_TypeDef;

struct LPC_GPIO_WriteRegister {
  uint32_t value;
  LPC_GPIO_TypeDef* owner;
  uint8_t port_number;
  bool sets_bits;

  void bind(LPC_GPIO_TypeDef* gpio, uint8_t port, bool set_register);
  void apply(uint32_t bits);
  LPC_GPIO_WriteRegister& operator=(uint32_t bits);
  LPC_GPIO_WriteRegister& operator|=(uint32_t bits);
  operator uint32_t() const { return value; }
};

typedef struct LPC_GPIO_TypeDef {
  __IO uint32_t FIOMASK;
  __IO uint32_t FIOPIN;
  __IO uint32_t FIODIR;
  LPC_GPIO_WriteRegister FIOSET;
  LPC_GPIO_WriteRegister FIOCLR;
} LPC_GPIO_TypeDef;

typedef struct {
  __IO uint32_t PINSEL0;
  __IO uint32_t PINSEL1;
  __IO uint32_t PINSEL2;
  __IO uint32_t PINSEL3;
  __IO uint32_t PINSEL4;
  __IO uint32_t PINSEL5;
  __IO uint32_t PINSEL6;
  __IO uint32_t PINSEL7;
  __IO uint32_t PINSEL8;
  __IO uint32_t PINSEL9;
  __IO uint32_t PINSEL10;
  uint32_t RESERVED0[5];
  __IO uint32_t PINMODE0;
  __IO uint32_t PINMODE1;
  __IO uint32_t PINMODE2;
  __IO uint32_t PINMODE3;
  __IO uint32_t PINMODE4;
  __IO uint32_t PINMODE5;
  __IO uint32_t PINMODE6;
  __IO uint32_t PINMODE7;
  __IO uint32_t PINMODE8;
  __IO uint32_t PINMODE9;
  __IO uint32_t PINMODE_OD0;
  __IO uint32_t PINMODE_OD1;
  __IO uint32_t PINMODE_OD2;
  __IO uint32_t PINMODE_OD3;
  __IO uint32_t PINMODE_OD4;
} LPC_PINCON_TypeDef;

typedef struct {
  __IO uint32_t ADCR;
  __IO uint32_t ADGDR;
  uint32_t RESERVED0;
  __IO uint32_t ADINTEN;
  __IO uint32_t ADDR0;
  __IO uint32_t ADDR1;
  __IO uint32_t ADDR2;
  __IO uint32_t ADDR3;
  __IO uint32_t ADDR4;
  __IO uint32_t ADDR5;
  __IO uint32_t ADDR6;
  __IO uint32_t ADDR7;
  __IO uint32_t ADSTAT;
  __IO uint32_t ADTRM;
} LPC_ADC_TypeDef;

typedef struct {
  __IO uint8_t WDMOD;
  uint8_t RESERVED0[3];
  __IO uint32_t WDTC;
  __O uint8_t WDFEED;
  uint8_t RESERVED1[3];
  __IO uint32_t WDTV;
  __IO uint32_t WDCLKSEL;
} LPC_WDT_TypeDef;

typedef struct {
  __IO uint32_t CR0;
  __IO uint32_t CR1;
  __IO uint32_t DR;
  __I uint32_t SR;
  __IO uint32_t CPSR;
  __IO uint32_t IMSC;
  __IO uint32_t RIS;
  __IO uint32_t MIS;
  __IO uint32_t ICR;
  __IO uint32_t DMACR;
} LPC_SSP_TypeDef;

extern uint32_t SystemCoreClock;

#ifdef __cplusplus
#include "sim/lpc1768.hpp"

#define LPC_SC (&::sim::lpc1768::active().sc())
#define LPC_TIM0 (&::sim::lpc1768::active().timer(0))
#define LPC_TIM1 (&::sim::lpc1768::active().timer(1))
#define LPC_TIM2 (&::sim::lpc1768::active().timer(2))
#define LPC_TIM3 (&::sim::lpc1768::active().timer(3))
#define LPC_GPIO0 (&::sim::lpc1768::active().gpio_port(0))
#define LPC_GPIO1 (&::sim::lpc1768::active().gpio_port(1))
#define LPC_GPIO2 (&::sim::lpc1768::active().gpio_port(2))
#define LPC_GPIO3 (&::sim::lpc1768::active().gpio_port(3))
#define LPC_GPIO4 (&::sim::lpc1768::active().gpio_port(4))
#define LPC_PINCON (&::sim::lpc1768::active().pincon())
#define LPC_ADC (&::sim::lpc1768::active().adc())
#define LPC_WDT (&::sim::lpc1768::active().watchdog())
#define LPC_SSP1 0x40030000u
#endif

#ifdef __cplusplus
extern "C" {
#endif

void NVIC_EnableIRQ(IRQn_Type IRQn);
void NVIC_DisableIRQ(IRQn_Type IRQn);
void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority);
uint32_t NVIC_GetPriority(IRQn_Type IRQn);
void NVIC_SetPriorityGrouping(uint32_t priority_grouping);
void __disable_irq(void);
void __enable_irq(void);

#ifndef __NOP
#define __NOP() ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
