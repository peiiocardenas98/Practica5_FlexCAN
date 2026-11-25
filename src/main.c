/*==================================================================================================
* Project : RTD AUTOSAR 4.9
* Platform : CORTEXM
* Peripheral : S32K3XX
* Dependencies : none
*
* Autosar Version : 4.9.0
* Autosar Revision : ASR_REL_4_9_REV_0000
* Autosar Conf.Variant :
* SW Version : 7.0.1
* Build Version : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
* Copyright 2020 - 2026 NXP
*
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

#ifdef __cplusplus
extern "C"{
#endif

/* Including necessary configuration files. */
#include "Mcal.h"
#include "Clock_Ip.h"
#include "Siul2_Port_Ip.h"
#include "IntCtrl_Ip.h"

volatile int exit_code = 0;
/* User includes */
#include "node_cfg.h"
#include "tick.h"
#include "log.h"
#include "app.h"

/*!
  \brief Nodo de la red CAN. El rol (Nodo 1..4) se elige en node_cfg.h.
  \details Inicializa la plataforma (reloj, pines, interrupciones), la consola por
           LPUART6 y el driver CAN sobre FlexCAN0 + TJA1043; despues corre las
           tareas en un lazo.
*/
int main(void)
{
    /* Reloj: FXOSC 16 MHz + PLL (CORE 120 MHz, FLEXCAN0_CLK 24 MHz) */
    Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);
#if defined(FEATURE_CLOCK_IP_HAS_SPLL_CLK)
    while (CLOCK_IP_PLL_LOCKED != Clock_Ip_GetPllStatus()) { }
    Clock_Ip_DistributePll();
#endif

    /* Pines: CAN0 (PTA7/PTA6), EN/STB_N/ERR_N del TJA1043, LEDs, SW2 y LPUART6 (consola) */
    Siul2_Port_Ip_Init(NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
                       g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals);

    /* Interrupciones: FlexCAN0 MB 0-31 y LPUART6 */
    IntCtrl_Ip_Init(&IntCtrlConfig_0);

    Tick_Init();
    Log_Init();
    App_Init();
    Log_Flush();

    for(;;)
    {
        uint32 Now = Tick_GetMs();

        App_MainFunction(Now);
        Log_Process();

        if(exit_code != 0)
        {
            break;
        }
    }
    return exit_code;
}

#ifdef __cplusplus
}
#endif

/** @} */
