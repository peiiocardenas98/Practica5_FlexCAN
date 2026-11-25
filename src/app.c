/**
 * @file    app.c
 * @brief   Aplicacion de la red CAN: el comportamiento depende de NODE_ID (node_cfg.h).
 *
 * Todos los nodos:
 *   - Transmiten su ID al cambiar SW2 (evento) y cada CAN_HEARTBEAT_MS
 *     (contador vivo). Payload de 8 bytes definido en can_network.h.
 *   - Aceptan solo los IDs de la tabla (MB1, MB2 con mascara 0x7FF) y
 *     encienden su LED mientras el emisor tenga el boton presionado.
 * Nodo 1 : incluye el comando de sincronizacion 0xA5 en sus tramas periodicas.
 * Nodo 4 : telemetria simulada (rampa de temperatura) y responde a cada 0x100.
 *
 * ARBITRATION_TEST = 1:
 *   Nodo 2 envia 0x200 cada CAN_ARB_PERIOD_MS. Nodo 1 y Nodo 3 lo reciben y,
 *   desde la interrupcion, encolan 0x100 y 0x300 a la vez -> arbitraje.
 */
#include "app.h"
#include "node_cfg.h"
#include "can_network.h"
#include "can_drv.h"
#include "board_io.h"
#include "log.h"
#include "tick.h"

#define APP_CAN_PE_CLOCK_HZ     24000000U   /* FLEXCAN0_CLK en Config Tools (Clocks) */
#define APP_CAN_BITRATE         500000U     /* Igual en todos los nodos */
#define APP_STATS_PERIOD_MS     5000U

/* LED que enciende cada nodo segun el ID recibido */
typedef struct
{
    uint8         Node;
    uint32        RxId;
    Board_LedType Led;
} App_ReactionType;

static const App_ReactionType s_reactions[] =
{
    { NODE_1, CAN_ID_TABLERO, BOARD_LED_GREEN },    /* Alerta de Nodo 2 */
    { NODE_1, CAN_ID_PUERTAS, BOARD_LED_GREEN },    /* Alerta de Nodo 3 */
    { NODE_2, CAN_ID_MOTOR,   BOARD_LED_BLUE  },    /* Boton de Nodo 1  */
    { NODE_2, CAN_ID_CLIMA,   BOARD_LED_BLUE  },    /* Boton de Nodo 4  */
    { NODE_3, CAN_ID_MOTOR,   BOARD_LED_RED   },    /* Evento de Nodo 1 */
    { NODE_3, CAN_ID_TABLERO, BOARD_LED_RED   },    /* Evento de Nodo 2 */
    { NODE_4, CAN_ID_MOTOR,   BOARD_LED_RED   },    /* Comando 0x100    */
    { NODE_4, CAN_ID_PUERTAS, BOARD_LED_BLUE  },    /* Evento de Nodo 3 */
};

static uint32            s_rxIds[CAN_NET_MAX_RX_IDS];
static Board_LedType     s_rxLed[CAN_NET_MAX_RX_IDS];
static CanDrv_ConfigType s_canCfg;
static boolean           s_canOk = FALSE;

/* Compartidos con la interrupcion */
static volatile boolean  s_remotePressed[CAN_NET_MAX_RX_IDS];
static volatile uint8    s_button = CAN_BUTTON_RELEASED;
static volatile uint8    s_txCounter = 0U;
static volatile uint8    s_rxTotal = 0U;

/* Solo main */
static uint8   s_temperature = 20U;
static uint8   s_lastRxCounter[CAN_NET_MAX_RX_IDS];
static boolean s_haveRxCounter[CAN_NET_MAX_RX_IDS];
static uint32  s_rxLost[CAN_NET_MAX_RX_IDS];
static uint32  s_rxBadChecksum = 0U;
static uint8   s_btnRaw = CAN_BUTTON_RELEASED;
static uint32  s_btnChangeMs = 0U;
static uint32  s_lastTxMs = 0U;
static uint32  s_lastStatsMs = 0U;
#if (NODE_ID == NODE_4) && (ARBITRATION_TEST == 0U)
static boolean s_replyPending = FALSE;
#endif

/* ---- Auxiliares ---- */

static Board_LedType App_FindLed(uint32 RxId)
{
    uint8 i;
    for (i = 0U; i < (uint8)(sizeof(s_reactions) / sizeof(s_reactions[0])); i++)
    {
        if ((s_reactions[i].Node == NODE_ID) && (s_reactions[i].RxId == RxId))
        {
            return s_reactions[i].Led;
        }
    }
    return BOARD_LED_NONE;
}

/* Un LED esta encendido si cualquier emisor asociado a el tiene el boton presionado */
static void App_UpdateLeds(void)
{
    static const Board_LedType Leds[] = { BOARD_LED_RED, BOARD_LED_GREEN, BOARD_LED_BLUE };
    uint8 l, s;

    for (l = 0U; l < (uint8)(sizeof(Leds) / sizeof(Leds[0])); l++)
    {
        boolean Used = FALSE;
        boolean On = FALSE;

        for (s = 0U; s < s_canCfg.NumRxIds; s++)
        {
            if (s_rxLed[s] == Leds[l])
            {
                Used = TRUE;
                On = (boolean)(On || s_remotePressed[s]);
            }
        }
        if (Used == TRUE)
        {
            Board_LedWrite(Leds[l], On);
        }
    }
}

static void App_LogFrame(const char *Dir, uint32 Id, const uint8 *Data, uint8 Len)
{
    const CanNet_MessageType *Msg = CanNet_FindMessage(Id);
    uint8 i;

    Log_Printf("[%6lu] %s 0x%03lX %-8s", (unsigned long)Tick_GetMs(), Dir, (unsigned long)Id,
               (Msg != NULL_PTR) ? Msg->Name : "?");
    for (i = 0U; i < Len; i++)
    {
        Log_Printf(" %02X", (unsigned)Data[i]);
    }
    Log_Printf("\r\n");
}

/* Arma y envia el payload de este nodo. Llamable desde main o desde la ISR. */
static Std_ReturnType App_SendStatus(uint8 Command, uint8 *Sent)
{
    uint8 Data[CAN_PL_LEN];

    Data[CAN_PL_NODE]     = (uint8)NODE_ID;
    Data[CAN_PL_BUTTON]   = s_button;
    Data[CAN_PL_COUNTER]  = s_txCounter;
    Data[CAN_PL_COMMAND]  = Command;
    Data[CAN_PL_TEMP]     = s_temperature;
    Data[CAN_PL_RX_COUNT] = s_rxTotal;
    Data[CAN_PL_TEC]      = CanDrv_GetTxErrorCounter();
    Data[CAN_PL_CHECKSUM] = CanNet_Checksum(Data);

    if (CanDrv_Send(Data, CAN_PL_LEN) != E_OK)
    {
        return E_NOT_OK;
    }
    s_txCounter++;                  /* 0x00..0xFF y da la vuelta */
    if (Sent != NULL_PTR)
    {
        uint8 i;
        for (i = 0U; i < CAN_PL_LEN; i++)
        {
            Sent[i] = Data[i];
        }
    }
    return E_OK;
}

/* Envio desde main con registro en el log */
static void App_SendAndLog(uint8 Command, uint32 NowMs)
{
    uint8 Sent[CAN_PL_LEN];

#if (NODE_ID == NODE_4)
    s_temperature = (s_temperature >= 35U) ? 20U : (uint8)(s_temperature + 1U);
#else
    s_temperature = 25U;
#endif

    if (App_SendStatus(Command, Sent) == E_OK)
    {
        s_lastTxMs = NowMs;
#if (ARBITRATION_TEST == 0U)
        App_LogFrame("TX", s_canCfg.TxId, Sent, CAN_PL_LEN);
#endif
    }
}

/* ---- Callback del driver: contexto de INTERRUPCION (sin log) ---- */

static void App_RxIsr(uint8 Slot, const CanDrv_FrameType *Frame)
{
    s_rxTotal++;
    s_remotePressed[Slot] = (Frame->Data[CAN_PL_BUTTON] == CAN_BUTTON_PRESSED) ? TRUE : FALSE;
    App_UpdateLeds();

#if (ARBITRATION_TEST == 1U) && ((NODE_ID == NODE_1) || (NODE_ID == NODE_3))
    /* Nodo 1 y Nodo 3 escuchan 0x200: ambos encolan su trama al mismo tiempo */
    if (Frame->Id == CAN_ID_TABLERO)
    {
        (void)App_SendStatus(CAN_CMD_NONE, NULL_PTR);
    }
#endif
}

/* ---- Procesamiento en main de lo recibido ---- */

static void App_ProcessRx(void)
{
    CanDrv_FrameType Frame;

    while (CanDrv_GetRxFrame(&Frame) == TRUE)
    {
        uint8 Slot = Frame.Slot;

        if (CanNet_Checksum(Frame.Data) != Frame.Data[CAN_PL_CHECKSUM])
        {
            s_rxBadChecksum++;
        }

        /* Perdida de paquetes: el contador del emisor avanza de 1 en 1 */
        if (s_haveRxCounter[Slot] == TRUE)
        {
            uint8 Expected = (uint8)(s_lastRxCounter[Slot] + 1U);
            if (Frame.Data[CAN_PL_COUNTER] != Expected)
            {
                s_rxLost[Slot] += (uint8)(Frame.Data[CAN_PL_COUNTER] - Expected);
            }
        }
        s_lastRxCounter[Slot] = Frame.Data[CAN_PL_COUNTER];
        s_haveRxCounter[Slot] = TRUE;

#if (ARBITRATION_TEST == 0U)
        App_LogFrame("RX", Frame.Id, Frame.Data, Frame.Len);
    #if (NODE_ID == NODE_4)
        if (Frame.Id == CAN_ID_MOTOR)
        {
            s_replyPending = TRUE;
        }
    #endif
#endif
    }
}

static void App_StartupBlink(void)
{
    static const Board_LedType Color[] = { BOARD_LED_NONE, BOARD_LED_GREEN, BOARD_LED_BLUE,
                                           BOARD_LED_RED,  BOARD_LED_GREEN };
    uint8 n;

    for (n = 0U; n < (uint8)NODE_ID; n++)
    {
        uint32 t0;

        Board_LedWrite(Color[NODE_ID], TRUE);
        t0 = Tick_GetMs();
        while ((Tick_GetMs() - t0) < 150U) { }
        Board_LedWrite(Color[NODE_ID], FALSE);
        t0 = Tick_GetMs();
        while ((Tick_GetMs() - t0) < 150U) { }
    }
}

/* ---- API de la aplicacion ---- */

void App_Init(void)
{
    uint8 i;

    Board_Init();
    App_StartupBlink();

    s_canCfg.Instance    = INST_FLEXCAN_0;
    s_canCfg.State       = &FlexCAN_State0;
    s_canCfg.HwConfig    = &FlexCAN_Config0;
    s_canCfg.PeClockHz   = APP_CAN_PE_CLOCK_HZ;
    s_canCfg.BitrateHz   = APP_CAN_BITRATE;
    s_canCfg.TxId        = CanNet_TxId();
    s_canCfg.RxIds       = s_rxIds;
    s_canCfg.NumRxIds    = CanNet_BuildRxIds(s_rxIds, CAN_NET_MAX_RX_IDS);
    s_canCfg.RxIsrNotify = App_RxIsr;

    for (i = 0U; i < s_canCfg.NumRxIds; i++)
    {
        s_rxLed[i] = App_FindLed(s_rxIds[i]);
    }

    Log_Printf("\r\n=== Practica 5 - Red CAN (FlexCAN) S32K312 ===\r\n");
    Log_Printf("Nodo %u: %s | TX 0x%03lX | %lu bps | arbitraje: %s\r\n",
               (unsigned)NODE_ID, CanNet_NodeName(NODE_ID), (unsigned long)s_canCfg.TxId,
               (unsigned long)APP_CAN_BITRATE, (ARBITRATION_TEST != 0U) ? "SI" : "no");
    for (i = 0U; i < s_canCfg.NumRxIds; i++)
    {
        const CanNet_MessageType *Msg = CanNet_FindMessage(s_rxIds[i]);
        static const char * const LedName[] = { "-", "rojo", "verde", "azul" };

        Log_Printf("  MB%u acepta 0x%03lX %-8s mascara 0x7FF -> LED %s\r\n",
                   (unsigned)(CANDRV_FIRST_RX_MB + i), (unsigned long)s_rxIds[i],
                   (Msg != NULL_PTR) ? Msg->Name : "?", LedName[s_rxLed[i]]);
    }

    /* TJA1043 en modo Normal antes de integrarse al bus */
    Board_CanTrcvNormal();

    if (CanDrv_Init(&s_canCfg) != E_OK)
    {
        Log_Printf("ERROR: CanDrv_Init fallo (revisar modo, IRQ y callback en Config Tools)\r\n");
        Board_LedWrite(BOARD_LED_RED, TRUE);
        s_canOk = FALSE;
        return;
    }
    s_canOk = TRUE;
}

void App_MainFunction(uint32 NowMs)
{
    if (s_canOk == FALSE)
    {
        return;
    }

    /* Boton SW2 con antirrebote; transmite en cada cambio */
    {
        uint8 Raw = (Board_ButtonRaw() == TRUE) ? CAN_BUTTON_PRESSED : CAN_BUTTON_RELEASED;

        if (Raw != s_btnRaw)
        {
            s_btnRaw = Raw;
            s_btnChangeMs = NowMs;
        }
        if ((Raw != s_button) && ((NowMs - s_btnChangeMs) >= BUTTON_DEBOUNCE_MS))
        {
            s_button = Raw;
#if (ARBITRATION_TEST == 0U)
            App_SendAndLog(CAN_CMD_NONE, NowMs);
#endif
        }
    }

    App_ProcessRx();

#if (ARBITRATION_TEST == 0U)
    /* Trama periodica ("contador vivo"); el Nodo 1 manda la sincronizacion */
    if ((NowMs - s_lastTxMs) >= CAN_HEARTBEAT_MS)
    {
        App_SendAndLog((NODE_ID == NODE_1) ? CAN_CMD_SYNC : CAN_CMD_NONE, NowMs);
        s_lastTxMs = NowMs;     /* Aunque MB0 este ocupado, reintentar en el siguiente periodo */
    }
    #if (NODE_ID == NODE_4)
    if (s_replyPending == TRUE)
    {
        App_SendAndLog(CAN_CMD_REPLY, NowMs);
        s_replyPending = FALSE;
    }
    #endif
#elif (NODE_ID == NODE_2)
    /* Disparador de la prueba de arbitraje */
    if ((NowMs - s_lastTxMs) >= CAN_ARB_PERIOD_MS)
    {
        App_SendAndLog(CAN_CMD_SYNC, NowMs);
        s_lastTxMs = NowMs;
    }
#endif

    /* Estadisticas */
    if ((NowMs - s_lastStatsMs) >= APP_STATS_PERIOD_MS)
    {
        CanDrv_StatsType St;
        uint8 i;

        s_lastStatsMs = NowMs;
        CanDrv_GetStats(&St);
        Log_Printf("[%6lu] stats: TX %lu/%lu (ocupado %lu) | TEC %u REC %u | ERR_N %u | checksum mal %lu\r\n",
                   (unsigned long)NowMs, (unsigned long)St.TxCompleted, (unsigned long)St.TxRequested,
                   (unsigned long)St.TxBusy, (unsigned)CanDrv_GetTxErrorCounter(),
                   (unsigned)CanDrv_GetRxErrorCounter(), (unsigned)Board_CanTrcvErrN(),
                   (unsigned long)s_rxBadChecksum);
        for (i = 0U; i < s_canCfg.NumRxIds; i++)
        {
            Log_Printf("          MB%u 0x%03lX: RX %lu, perdidos %lu\r\n",
                       (unsigned)(CANDRV_FIRST_RX_MB + i), (unsigned long)s_rxIds[i],
                       (unsigned long)St.RxFrames[i], (unsigned long)s_rxLost[i]);
        }
    }
}
