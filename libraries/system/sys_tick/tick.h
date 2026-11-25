/**
 * @file    tick.h
 * @brief   Base de tiempo de 1 ms con el SysTick del Cortex-M7.
 */
#ifndef TICK_H
#define TICK_H

#include "Std_Types.h"

/** Frecuencia de CORE_CLK segun Config Tools (Clocks). Cambiarla si cambia el reloj. */
#define TICK_CORE_CLOCK_HZ  120000000U

/** @brief Arranca el SysTick a 1 kHz. Llamar despues de Clock_Ip_Init. */
void Tick_Init(void);

/** @brief Milisegundos desde Tick_Init (se desborda a los ~49 dias). */
uint32 Tick_GetMs(void);

#endif /* TICK_H */
