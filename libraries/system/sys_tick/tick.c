/**
 * @file    tick.c
 * @brief   Base de tiempo de 1 ms con el SysTick del Cortex-M7.
 *
 * SysTick_Handler sustituye al handler debil de Project_Settings/Startup_Code/exceptions.c.
 * OsIf no usa el SysTick en este proyecto (OSIF_USE_SYSTEM_TIMER = STD_OFF).
 */
#include "tick.h"
#include "S32K312_SYSTICK.h"

static volatile uint32 s_ms = 0U;

void SysTick_Handler(void);

void SysTick_Handler(void)
{
    s_ms++;
}

void Tick_Init(void)
{
    S32_SysTick->CSRr = 0U;
    S32_SysTick->RVR  = (TICK_CORE_CLOCK_HZ / 1000U) - 1U;
    S32_SysTick->CVR  = 0U;
    /* Reloj del core, con interrupcion */
    S32_SysTick->CSRr = S32_SysTick_CSR_CLKSOURCE_MASK | S32_SysTick_CSR_TICKINT_MASK |
                        S32_SysTick_CSR_ENABLE_MASK;
}

uint32 Tick_GetMs(void)
{
    return s_ms;
}
