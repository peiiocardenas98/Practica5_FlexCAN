/**
 * @file    can_drv.h
 * @brief   Driver CAN clasico (11 bits, 8 bytes) sobre FlexCAN_Ip del RTD de NXP.
 *
 * Capa delgada sobre FlexCAN_Ip:
 *  - FlexCAN_Ip_Init carga la configuracion generada por Config Tools.
 *  - FlexCAN_Ip_SetBitrate fija la temporizacion de bit a partir de
 *    PeClockHz y BitrateHz (no depende de como se capture en el GUI).
 *  - Mascaras individuales por MB (MCR[IRMQ]): un MB de recepcion por ID
 *    aceptado, mascara 0x7FF. El hardware descarta los demas IDs, asi que
 *    la interrupcion RX solo llega con tramas suscritas.
 *
 * Message Buffers:
 *  MB0          -> transmision (FlexCAN_Ip_Send, por interrupcion)
 *  MB1..MBn     -> recepcion, uno por ID de RxIds[] (FlexCAN_Ip_ConfigRxMb)
 *
 * flexcan_callback (contexto de interrupcion) lee la trama, llama a
 * RxIsrNotify (para reaccionar rapido, p. ej. LEDs), la encola para la
 * aplicacion y rearma el MB con FlexCAN_Ip_Receive. La aplicacion saca las
 * tramas con CanDrv_GetRxFrame desde main (ahi si puede imprimir en el log).
 */
#ifndef CAN_DRV_H
#define CAN_DRV_H

#include "Std_Types.h"
#include "FlexCAN_Ip.h"

#define CANDRV_MAX_DATA_LEN     8U
#define CANDRV_MAX_RX_MB        4U      /* MB1..MB4 */
#define CANDRV_TX_MB            0U
#define CANDRV_FIRST_RX_MB      1U
#define CANDRV_RX_QUEUE_LEN     16U     /* Potencia de 2 */

/** Una trama recibida */
typedef struct
{
    uint32 Id;              /**< ID estandar (11 bits) */
    uint8  Len;             /**< DLC (0..8) */
    uint8  Data[CANDRV_MAX_DATA_LEN];
    uint8  Slot;            /**< Indice en RxIds[] (0 = MB1, 1 = MB2, ...) */
} CanDrv_FrameType;

/**
 * @brief Aviso desde la interrupcion RX. Debe ser corto (sin log).
 * @param Slot   Indice en RxIds[] del MB que recibio.
 * @param Frame  Trama recibida.
 */
typedef void (*CanDrv_RxIsrCb)(uint8 Slot, const CanDrv_FrameType *Frame);

/** Configuracion completa del driver: la recibe CanDrv_Init */
typedef struct
{
    uint8                          Instance;      /**< INST_FLEXCAN_0 */
    Flexcan_Ip_StateType          *State;         /**< FlexCAN_State0 (generado) */
    const Flexcan_Ip_ConfigType   *HwConfig;      /**< FlexCAN_Config0 (generado) */
    uint32                         PeClockHz;     /**< FLEXCAN0_CLK de Config Tools (Clocks) */
    uint32                         BitrateHz;     /**< 500000 */
    uint32                         TxId;          /**< ID que transmite este nodo */
    const uint32                  *RxIds;         /**< IDs aceptados (uno por MB) */
    uint8                          NumRxIds;      /**< 1..CANDRV_MAX_RX_MB */
    CanDrv_RxIsrCb                 RxIsrNotify;   /**< Puede ser NULL_PTR */
} CanDrv_ConfigType;

/** Contadores de diagnostico */
typedef struct
{
    uint32 TxRequested;     /**< FlexCAN_Ip_Send aceptados */
    uint32 TxCompleted;     /**< FLEXCAN_EVENT_TX_COMPLETE */
    uint32 TxBusy;          /**< MB0 seguia ocupado (tipicamente: nadie da ACK) */
    uint32 RxFrames[CANDRV_MAX_RX_MB];
    uint32 RxQueueLost;     /**< Tramas descartadas por cola llena */
} CanDrv_StatsType;

/**
 * @brief Inicializa FlexCAN: bitrate, mascaras, modo Start y MBs de recepcion.
 * @note  Llamar despues de Clock_Ip_Init, Siul2_Port_Ip_Init e IntCtrl_Ip_Init,
 *        con el transceiver ya en modo Normal.
 * @return E_NOT_OK si algun paso del RTD falla o la temporizacion no es posible.
 */
Std_ReturnType CanDrv_Init(const CanDrv_ConfigType *Config);

/**
 * @brief  Transmite TxId por MB0 (no bloqueante).
 * @return E_NOT_OK si MB0 sigue ocupado con la trama anterior.
 * @note   Se puede llamar desde main o desde RxIsrNotify.
 */
Std_ReturnType CanDrv_Send(const uint8 *Data, uint8 Len);

/** @brief TRUE si MB0 aun no termina la transmision anterior. */
boolean CanDrv_TxBusy(void);

/** @brief Saca la siguiente trama recibida. FALSE si la cola esta vacia. */
boolean CanDrv_GetRxFrame(CanDrv_FrameType *Frame);

/** @brief Copia de los contadores de diagnostico. */
void CanDrv_GetStats(CanDrv_StatsType *Stats);

/** @brief Contadores de error del controlador (TEC / REC). */
uint8 CanDrv_GetTxErrorCounter(void);
uint8 CanDrv_GetRxErrorCounter(void);

/**
 * @brief Callback del RTD. Su nombre debe coincidir con el campo
 *        "FlexCanCallback" del componente FlexCAN en Peripherals Tool.
 */
void flexcan_callback(uint8 instance,
                      Flexcan_Ip_EventType eventType,
                      uint32 buffIdx,
                      const Flexcan_Ip_StateType *flexcanState);

#endif /* CAN_DRV_H */
