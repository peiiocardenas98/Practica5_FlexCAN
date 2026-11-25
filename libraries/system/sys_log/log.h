/**
 * @file    log.h
 * @brief   Consola de depuracion por LPUART6 (OpenSDA, 115200 8N1), no bloqueante.
 *
 * Log_Printf solo copia el texto a un buffer circular; Log_Process lo envia con
 * Lpuart_Uart_Ip_AsyncSend. Asi imprimir no retrasa las respuestas del LIN.
 * Usar solo desde el contexto de main (no desde interrupciones).
 */
#ifndef LOG_H
#define LOG_H

#include "Std_Types.h"

/** @brief Inicializa LPUART6 con la configuracion de Config Tools. */
void Log_Init(void);

/** @brief Formatea como printf y encola. Si el buffer se llena, se descarta el texto que no cabe. */
void Log_Printf(const char *Format, ...) __attribute__((format(printf, 1, 2)));

/** @brief Envia lo pendiente; llamar en cada vuelta del lazo principal. */
void Log_Process(void);

/** @brief Espera a que se envie todo lo pendiente (solo al arrancar). */
void Log_Flush(void);

#endif /* LOG_H */
