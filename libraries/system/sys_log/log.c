/**
 * @file    log.c
 * @brief   Consola de depuracion por LPUART6 (OpenSDA, 115200 8N1), no bloqueante.
 */
#include "log.h"
#include "Lpuart_Uart_Ip.h"
#include "Lpuart_Uart_Ip_Irq.h"
#include <stdarg.h>
#include <stdio.h>

#define LOG_UART_INSTANCE   6U          /* LPUART6 -> OpenSDA (PTA16 TX / PTA15 RX) */
#define LOG_BUF_SIZE        2048U       /* Potencia de 2 */
#define LOG_LINE_MAX        160U

static uint8  s_buf[LOG_BUF_SIZE];
static uint32 s_head = 0U;              /* Siguiente posicion a escribir */
static uint32 s_tail = 0U;              /* Primer byte sin enviar */
static uint32 s_inFlight = 0U;          /* Bytes entregados a AsyncSend */

void Log_Init(void)
{
    Lpuart_Uart_Ip_Init(LOG_UART_INSTANCE, &Lpuart_Uart_Ip_xHwConfigPB_6);
}

void Log_Printf(const char *Format, ...)
{
    char Line[LOG_LINE_MAX];
    va_list Args;
    int Len;
    int i;

    va_start(Args, Format);
    Len = vsnprintf(Line, sizeof(Line), Format, Args);
    va_end(Args);

    if (Len <= 0)
    {
        return;
    }
    if (Len >= (int)sizeof(Line))
    {
        Len = (int)sizeof(Line) - 1;
    }

    for (i = 0; i < Len; i++)
    {
        if ((s_head - s_tail) >= LOG_BUF_SIZE)
        {
            break;                      /* Buffer lleno: se descarta el resto */
        }
        s_buf[s_head & (LOG_BUF_SIZE - 1U)] = (uint8)Line[i];
        s_head++;
    }
}

void Log_Process(void)
{
    uint32 Remaining;
    uint32 Start;
    uint32 Chunk;

    if (s_inFlight > 0U)
    {
        if (Lpuart_Uart_Ip_GetTransmitStatus(LOG_UART_INSTANCE, &Remaining) == LPUART_UART_IP_STATUS_BUSY)
        {
            return;
        }
        s_tail += s_inFlight;
        s_inFlight = 0U;
    }

    if (s_head == s_tail)
    {
        return;
    }

    /* Tramo contiguo hasta el final del buffer circular */
    Start = s_tail & (LOG_BUF_SIZE - 1U);
    Chunk = s_head - s_tail;
    if (Chunk > (LOG_BUF_SIZE - Start))
    {
        Chunk = LOG_BUF_SIZE - Start;
    }

    if (Lpuart_Uart_Ip_AsyncSend(LOG_UART_INSTANCE, &s_buf[Start], Chunk) == LPUART_UART_IP_STATUS_SUCCESS)
    {
        s_inFlight = Chunk;
    }
}

void Log_Flush(void)
{
    while ((s_head != s_tail) || (s_inFlight > 0U))
    {
        Log_Process();
    }
}
