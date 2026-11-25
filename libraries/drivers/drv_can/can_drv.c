/**
 * @file    can_drv.c
 * @brief   Driver CAN clasico (11 bits, 8 bytes) sobre FlexCAN_Ip del RTD de NXP.
 */
#include "can_drv.h"

/* Mascara estricta para ID estandar. El RTD escribe RXIMR tal cual, por eso
 * los 11 bits se alinean al campo ID estandar del registro (bits 28..18). */
#define CANDRV_STD_ID_SHIFT     18U
#define CANDRV_STD_ID_MASK      0x7FFUL
#define CANDRV_RX_MASK_EXACT    (CANDRV_STD_ID_MASK << CANDRV_STD_ID_SHIFT)

/* Limites de los campos de CTRL1 (en tq reales = valor de registro + 1) */
#define CANDRV_MAX_PRESC        256U
#define CANDRV_MAX_PROPSEG      8U
#define CANDRV_MAX_PSEG         8U
#define CANDRV_MAX_RJW          4U

static const CanDrv_ConfigType *s_cfg = NULL_PTR;

/* Un buffer del RTD por MB de recepcion (rxData[slot].data[] tras RX_COMPLETE) */
static Flexcan_Ip_MsgBuffType rxData[CANDRV_MAX_RX_MB];

/* Cola ISR -> main (un productor, un consumidor) */
static CanDrv_FrameType  s_queue[CANDRV_RX_QUEUE_LEN];
static volatile uint32   s_qHead = 0U;
static volatile uint32   s_qTail = 0U;

static volatile CanDrv_StatsType s_stats;

static const Flexcan_Ip_DataInfoType s_txInfo =
{
    .msg_id_type = FLEXCAN_MSG_ID_STD,
    .data_length = CANDRV_MAX_DATA_LEN,
    .is_remote   = FALSE,
    .is_polling  = FALSE        /* Interrupcion -> FLEXCAN_EVENT_TX_COMPLETE */
};

static const Flexcan_Ip_DataInfoType s_rxInfo =
{
    .msg_id_type = FLEXCAN_MSG_ID_STD,
    .data_length = CANDRV_MAX_DATA_LEN,
    .is_remote   = FALSE,
    .is_polling  = FALSE        /* Interrupcion -> flexcan_callback */
};

/* ---- Seccion critica (PRIMASK) ---- */

static inline uint32 CanDrv_EnterCritical(void)
{
    uint32 Primask;
    __asm volatile ("mrs %0, primask\n\tcpsid i" : "=r" (Primask) : : "memory");
    return Primask;
}

static inline void CanDrv_ExitCritical(uint32 Primask)
{
    __asm volatile ("msr primask, %0" : : "r" (Primask) : "memory");
}

/* ---- Temporizacion de bit ----
 * Busca N tq por bit (preferencia 16) tal que PeClockHz = BitrateHz * N * Presc
 * exacto, con punto de muestreo cercano a 80 %.
 *   Ej. 24 MHz, 500 kbps: N = 16, Presc = 3 -> SYNC 1 + PROP 6 + PS1 6 + PS2 3,
 *       muestreo 81.25 %, SJW 3.
 */
static boolean CanDrv_CalcTiming(uint32 PeClockHz, uint32 BitrateHz, Flexcan_Ip_TimeSegmentType *Ts)
{
    static const uint8 TqOptions[] = { 16U, 20U, 10U, 8U, 12U, 15U, 18U, 25U };
    uint8 i;

    if (BitrateHz == 0U)
    {
        return FALSE;
    }

    for (i = 0U; i < (uint8)sizeof(TqOptions); i++)
    {
        uint32 Ntq = TqOptions[i];
        uint32 Den = BitrateHz * Ntq;
        uint32 Presc, Ps2, Rem, Prop, Ps1, Rjw;

        if ((PeClockHz % Den) != 0U)
        {
            continue;
        }
        Presc = PeClockHz / Den;
        if ((Presc == 0U) || (Presc > CANDRV_MAX_PRESC))
        {
            continue;
        }

        Ps2  = (Ntq / 5U < 2U) ? 2U : (Ntq / 5U);   /* ~20 % despues del muestreo */
        Rem  = Ntq - 1U - Ps2;                      /* PROP + PS1 */
        Prop = (Rem + 1U) / 2U;
        if (Prop > CANDRV_MAX_PROPSEG)
        {
            Prop = CANDRV_MAX_PROPSEG;
        }
        Ps1  = Rem - Prop;
        if ((Ps1 == 0U) || (Ps1 > CANDRV_MAX_PSEG) || (Ps2 > CANDRV_MAX_PSEG))
        {
            continue;
        }
        Rjw = (Ps2 < CANDRV_MAX_RJW) ? Ps2 : CANDRV_MAX_RJW;

        /* El RTD escribe valores de registro: tq - 1 */
        Ts->preDivider = Presc - 1U;
        Ts->propSeg    = Prop - 1U;
        Ts->phaseSeg1  = Ps1 - 1U;
        Ts->phaseSeg2  = Ps2 - 1U;
        Ts->rJumpwidth = Rjw - 1U;
        return TRUE;
    }
    return FALSE;
}

/* ---- API ---- */

Std_ReturnType CanDrv_Init(const CanDrv_ConfigType *Config)
{
    Flexcan_Ip_TimeSegmentType Ts;
    Flexcan_Ip_StatusType St;
    uint8 i;

    if ((Config == NULL_PTR) || (Config->RxIds == NULL_PTR) ||
        (Config->NumRxIds == 0U) || (Config->NumRxIds > CANDRV_MAX_RX_MB) ||
        (CanDrv_CalcTiming(Config->PeClockHz, Config->BitrateHz, &Ts) == FALSE))
    {
        return E_NOT_OK;
    }
    s_cfg = Config;

    /* 1) Driver del RTD (queda en Freeze) */
    St = FlexCAN_Ip_Init(Config->Instance, Config->State, Config->HwConfig);

    /* 2) Bitrate (solo en Freeze) */
    if (St == FLEXCAN_STATUS_SUCCESS)
    {
        St = FlexCAN_Ip_SetBitrate(Config->Instance, &Ts, FALSE);
    }

    /* 3) Individual Rx Masking + mascara 0x7FF en cada MB de recepcion */
    if (St == FLEXCAN_STATUS_SUCCESS)
    {
        St = FlexCAN_Ip_SetRxMaskType(Config->Instance, FLEXCAN_RX_MASK_INDIVIDUAL);
    }
    for (i = 0U; (i < Config->NumRxIds) && (St == FLEXCAN_STATUS_SUCCESS); i++)
    {
        St = FlexCAN_Ip_SetRxIndividualMask(Config->Instance, (uint8)(CANDRV_FIRST_RX_MB + i),
                                            CANDRV_RX_MASK_EXACT);
    }

    /* 4) Salir de Freeze: el controlador se integra al bus */
    if (St == FLEXCAN_STATUS_SUCCESS)
    {
        St = FlexCAN_Ip_SetStartMode(Config->Instance);
    }

    /* 5) Un MB por ID aceptado y armado de la recepcion */
    for (i = 0U; (i < Config->NumRxIds) && (St == FLEXCAN_STATUS_SUCCESS); i++)
    {
        uint8 Mb = (uint8)(CANDRV_FIRST_RX_MB + i);

        St = FlexCAN_Ip_ConfigRxMb(Config->Instance, Mb, &s_rxInfo, Config->RxIds[i]);
        if (St == FLEXCAN_STATUS_SUCCESS)
        {
            St = FlexCAN_Ip_Receive(Config->Instance, Mb, &rxData[i], FALSE);
        }
    }

    return (St == FLEXCAN_STATUS_SUCCESS) ? E_OK : E_NOT_OK;
}

Std_ReturnType CanDrv_Send(const uint8 *Data, uint8 Len)
{
    Flexcan_Ip_DataInfoType Info = s_txInfo;
    Flexcan_Ip_StatusType St;
    uint32 Pm;

    if ((s_cfg == NULL_PTR) || (Data == NULL_PTR) || (Len > CANDRV_MAX_DATA_LEN))
    {
        return E_NOT_OK;
    }
    Info.data_length = Len;

    /* FlexCAN_Ip_Send revisa y marca el MB sin seccion critica propia:
     * se protege para poder llamarlo desde main y desde la ISR. */
    Pm = CanDrv_EnterCritical();
    St = FlexCAN_Ip_Send(s_cfg->Instance, CANDRV_TX_MB, &Info, s_cfg->TxId, Data);
    CanDrv_ExitCritical(Pm);

    if (St == FLEXCAN_STATUS_SUCCESS)
    {
        s_stats.TxRequested++;
        return E_OK;
    }
    if (St == FLEXCAN_STATUS_BUSY)
    {
        s_stats.TxBusy++;
    }
    return E_NOT_OK;
}

boolean CanDrv_TxBusy(void)
{
    if (s_cfg == NULL_PTR)
    {
        return FALSE;
    }
    return (FlexCAN_Ip_GetTransferStatus(s_cfg->Instance, CANDRV_TX_MB) == FLEXCAN_STATUS_BUSY) ? TRUE : FALSE;
}

boolean CanDrv_GetRxFrame(CanDrv_FrameType *Frame)
{
    uint32 Tail = s_qTail;

    if ((Frame == NULL_PTR) || (Tail == s_qHead))
    {
        return FALSE;
    }
    *Frame = s_queue[Tail & (CANDRV_RX_QUEUE_LEN - 1U)];
    s_qTail = Tail + 1U;
    return TRUE;
}

void CanDrv_GetStats(CanDrv_StatsType *Stats)
{
    uint32 Pm;
    uint8 i;

    if (Stats == NULL_PTR)
    {
        return;
    }
    Pm = CanDrv_EnterCritical();
    Stats->TxRequested = s_stats.TxRequested;
    Stats->TxCompleted = s_stats.TxCompleted;
    Stats->TxBusy      = s_stats.TxBusy;
    Stats->RxQueueLost = s_stats.RxQueueLost;
    for (i = 0U; i < CANDRV_MAX_RX_MB; i++)
    {
        Stats->RxFrames[i] = s_stats.RxFrames[i];
    }
    CanDrv_ExitCritical(Pm);
}

uint8 CanDrv_GetTxErrorCounter(void)
{
    return (s_cfg != NULL_PTR) ? FlexCAN_Ip_GetControllerTxErrorCounter(s_cfg->Instance) : 0U;
}

uint8 CanDrv_GetRxErrorCounter(void)
{
    return (s_cfg != NULL_PTR) ? FlexCAN_Ip_GetControllerRxErrorCounter(s_cfg->Instance) : 0U;
}

/* ---- Callback del RTD (interrupcion FlexCAN0_1_IRQn, MB 0..31) ---- */

void flexcan_callback(uint8 instance,
                      Flexcan_Ip_EventType eventType,
                      uint32 buffIdx,
                      const Flexcan_Ip_StateType *flexcanState)
{
    (void)flexcanState;

    if ((s_cfg == NULL_PTR) || (instance != s_cfg->Instance))
    {
        return;
    }

    if ((eventType == FLEXCAN_EVENT_RX_COMPLETE) &&
        (buffIdx >= CANDRV_FIRST_RX_MB) && (buffIdx < (uint32)(CANDRV_FIRST_RX_MB + s_cfg->NumRxIds)))
    {
        uint8 Slot = (uint8)(buffIdx - CANDRV_FIRST_RX_MB);
        CanDrv_FrameType Frame;
        uint8 i;

        /* Leer rxData[].data[] */
        Frame.Id   = rxData[Slot].msgId & CANDRV_STD_ID_MASK;
        Frame.Len  = (rxData[Slot].dataLen > CANDRV_MAX_DATA_LEN) ? CANDRV_MAX_DATA_LEN : rxData[Slot].dataLen;
        Frame.Slot = Slot;
        for (i = 0U; i < CANDRV_MAX_DATA_LEN; i++)
        {
            Frame.Data[i] = (i < Frame.Len) ? rxData[Slot].data[i] : 0U;
        }
        s_stats.RxFrames[Slot]++;

        /* Reaccion inmediata de la aplicacion (LEDs, arbitraje) */
        if (s_cfg->RxIsrNotify != NULL_PTR)
        {
            s_cfg->RxIsrNotify(Slot, &Frame);
        }

        /* Encolar para main */
        if ((s_qHead - s_qTail) < CANDRV_RX_QUEUE_LEN)
        {
            s_queue[s_qHead & (CANDRV_RX_QUEUE_LEN - 1U)] = Frame;
            s_qHead++;
        }
        else
        {
            s_stats.RxQueueLost++;
        }

        /* Rearmar el buzon */
        (void)FlexCAN_Ip_Receive(instance, (uint8)buffIdx, &rxData[Slot], FALSE);
    }
    else if ((eventType == FLEXCAN_EVENT_TX_COMPLETE) && (buffIdx == CANDRV_TX_MB))
    {
        s_stats.TxCompleted++;
    }
    else
    {
        /* Otros eventos no usados */
    }
}
