/**
 * @file    board_io.c
 * @brief   E/S de la S32K312MINI-EVB: LEDs, SW2 y pines de control del TJA1043.
 *
 * Registros de byte GPDOn / GPDIn del SIUL2 (igual que en practicas anteriores).
 * Indice SIUL2: PTAx = x, PTBx = 32 + x, PTCx = 64 + x.
 */
#include "board_io.h"
#include "S32K312_SIUL2.h"

#define LED_RED_GPDO        (IP_SIUL2->GPDO29)  /* PTA29 */
#define LED_GREEN_GPDO      (IP_SIUL2->GPDO30)  /* PTA30 */
#define LED_BLUE_GPDO       (IP_SIUL2->GPDO31)  /* PTA31 */
#define SW2_GPDI            (IP_SIUL2->GPDI51)  /* PTB19 = 32 + 19 */
#define CAN_STB_N_GPDO      (IP_SIUL2->GPDO84)  /* PTC20 = 64 + 20 */
#define CAN_EN_GPDO         (IP_SIUL2->GPDO85)  /* PTC21 = 64 + 21 */
#define CAN_ERR_N_GPDI      (IP_SIUL2->GPDI87)  /* PTC23 = 64 + 23 */

#define LED_ON              0U                  /* Activos en bajo */
#define LED_OFF             1U

void Board_Init(void)
{
    Board_LedsOff();
    Board_CanTrcvStandby();
}

void Board_LedWrite(Board_LedType Led, boolean On)
{
    uint8 Level = (On == TRUE) ? LED_ON : LED_OFF;

    switch (Led)
    {
        case BOARD_LED_RED:   LED_RED_GPDO   = Level; break;
        case BOARD_LED_GREEN: LED_GREEN_GPDO = Level; break;
        case BOARD_LED_BLUE:  LED_BLUE_GPDO  = Level; break;
        default:              /* BOARD_LED_NONE */    break;
    }
}

void Board_LedsOff(void)
{
    LED_RED_GPDO   = LED_OFF;
    LED_GREEN_GPDO = LED_OFF;
    LED_BLUE_GPDO  = LED_OFF;
}

boolean Board_ButtonRaw(void)
{
    return (SW2_GPDI != 0U) ? TRUE : FALSE;
}

void Board_CanTrcvNormal(void)
{
    CAN_STB_N_GPDO = 1U;
    CAN_EN_GPDO    = 1U;
}

void Board_CanTrcvStandby(void)
{
    CAN_EN_GPDO    = 0U;
    CAN_STB_N_GPDO = 0U;
}

boolean Board_CanTrcvErrN(void)
{
    return (CAN_ERR_N_GPDI != 0U) ? TRUE : FALSE;
}
