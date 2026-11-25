/**
 * @file    board_io.h
 * @brief   E/S de la S32K312MINI-EVB usadas en la practica (esquematico SCH-94288 Rev C).
 *
 *   Senal          Pin MCU  QFP172  Hoja  Nota
 *   -------------  -------  ------  ----  ---------------------------------
 *   LED RGB rojo   PTA29    32      12    Activo en BAJO (anodo a VDD)
 *   LED RGB verde  PTA30    33      12    Activo en BAJO
 *   LED RGB azul   PTA31    39      12    Activo en BAJO
 *   Boton SW2      PTB19    43      12    Activo en ALTO (pull-down 4.7k)
 *   CAN_EN         PTC21    86      9     -> TJA1043 EN    (1 = Normal)
 *   CAN_STB_N      PTC20    85      9     -> TJA1043 STB_N (1 = Normal)
 *   CAN_ERR_N      PTC23    87      9     <- TJA1043 ERR_N (activo en bajo)
 *
 * SW1 de la MINI-EVB es el boton de RESET (hoja 8): el boton del nodo es SW2.
 * El MUX y la direccion de cada pin los configura el Pins Tool
 * (Siul2_Port_Ip_Init); aqui solo se leen/escriben niveles con GPDOn/GPDIn.
 */
#ifndef BOARD_IO_H
#define BOARD_IO_H

#include "Std_Types.h"

typedef enum
{
    BOARD_LED_NONE  = 0U,
    BOARD_LED_RED   = 1U,
    BOARD_LED_GREEN = 2U,
    BOARD_LED_BLUE  = 3U
} Board_LedType;

/** @brief LEDs apagados y transceiver en Standby. Llamar despues de Siul2_Port_Ip_Init. */
void Board_Init(void);

/** @brief Enciende/apaga un LED del RGB. */
void Board_LedWrite(Board_LedType Led, boolean On);

/** @brief Apaga los tres LEDs. */
void Board_LedsOff(void);

/** @brief Nivel crudo de SW2 (TRUE = presionado, sin antirrebote). */
boolean Board_ButtonRaw(void);

/** @brief TJA1043 en modo Normal: STB_N = 1, EN = 1. */
void Board_CanTrcvNormal(void);

/** @brief TJA1043 en Standby: EN = 0, STB_N = 0. */
void Board_CanTrcvStandby(void);

/** @brief Nivel de ERR_N del TJA1043 (FALSE = indica falla). */
boolean Board_CanTrcvErrN(void);

#endif /* BOARD_IO_H */
