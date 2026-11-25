/**
 * @file    can_network.h
 * @brief   Tabla de mensajes de la red CAN: la MISMA en todas las tarjetas.
 *
 * Cada nodo la filtra segun NODE_ID para saber que ID transmite y cuales
 * debe aceptar (estos ultimos se cargan en los MB de recepcion con mascara
 * 0x7FF, asi que el hardware descarta el resto del trafico).
 */
#ifndef CAN_NETWORK_H
#define CAN_NETWORK_H

#include "Std_Types.h"
#include "node_cfg.h"

/* Bit de cada nodo en la mascara de suscriptores */
#define NODE_BIT(node)          ((uint8)(1U << (node)))

/* ---- IDs estandar de 11 bits (tabla de arquitectura de la practica) ---- */
#define CAN_ID_MOTOR            0x100U  /* Nodo 1 */
#define CAN_ID_TABLERO          0x200U  /* Nodo 2 */
#define CAN_ID_PUERTAS          0x300U  /* Nodo 3 */
#define CAN_ID_CLIMA            0x400U  /* Nodo 4 */

/* ---- Formato del payload (8 bytes, igual para todos los nodos) ---- */
#define CAN_PL_LEN              8U
#define CAN_PL_NODE             0U  /* ID del nodo emisor (1..4)                     */
#define CAN_PL_BUTTON           1U  /* 0x01 presionado / 0x00 libre                  */
#define CAN_PL_COUNTER          2U  /* 0x00..0xFF, para detectar perdidas            */
#define CAN_PL_COMMAND          3U  /* CAN_CMD_xxx                                   */
#define CAN_PL_TEMP             4U  /* Temperatura simulada en grados C              */
#define CAN_PL_RX_COUNT         5U  /* Tramas aceptadas por el emisor (8 bits bajos) */
#define CAN_PL_TEC              6U  /* Transmit Error Counter del emisor             */
#define CAN_PL_CHECKSUM         7U  /* XOR de los bytes 0..6                         */

#define CAN_BUTTON_PRESSED      0x01U
#define CAN_BUTTON_RELEASED     0x00U

#define CAN_CMD_NONE            0x00U
#define CAN_CMD_SYNC            0xA5U   /* Sincronizacion global (Nodo 1)          */
#define CAN_CMD_REPLY           0xC1U   /* Respuesta del Nodo 4 a un comando 0x100 */

/** Un mensaje de la red */
typedef struct
{
    uint32      Id;
    uint8       Publisher;      /**< NODE_1 .. NODE_4 */
    uint8       Subscribers;    /**< Mascara NODE_BIT(...) */
    const char *Name;
} CanNet_MessageType;

/** Maximo de IDs que un nodo puede escuchar (un MB de recepcion por ID) */
#define CAN_NET_MAX_RX_IDS      4U

/** @brief ID que transmite ESTE nodo. */
uint32 CanNet_TxId(void);

/**
 * @brief Arma la lista de IDs que ESTE nodo acepta (en el orden de la tabla).
 * @return Numero de IDs escritos en Ids.
 */
uint8 CanNet_BuildRxIds(uint32 *Ids, uint8 MaxIds);

/** @brief Mensaje con ese ID, o NULL_PTR. */
const CanNet_MessageType *CanNet_FindMessage(uint32 Id);

/** @brief Nombre del nodo ("MOTOR", "TABLERO", ...). */
const char *CanNet_NodeName(uint8 Node);

/** @brief Checksum XOR de los bytes 0..6 del payload. */
uint8 CanNet_Checksum(const uint8 *Data);

#endif /* CAN_NETWORK_H */
