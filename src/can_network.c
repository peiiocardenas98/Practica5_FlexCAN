/**
 * @file    can_network.c
 * @brief   Tabla de mensajes de la red CAN y filtros por nodo.
 */
#include "can_network.h"

/*
 * Tabla de la red (arquitectura de la practica).
 *
 *  ID     Publica   Reciben          Funcion
 *  0x100  Nodo 1    Nodo 2, 3, 4     Boton SW2 + comando de sincronizacion
 *  0x200  Nodo 2    Nodo 1, 3        Boton SW2 + contador vivo
 *  0x300  Nodo 3    Nodo 1, 4        Boton SW2 (sensor de puerta)
 *  0x400  Nodo 4    Nodo 2           Telemetria simulada / respuesta a 0x100
 */
static const CanNet_MessageType s_messages[] =
{
    { CAN_ID_MOTOR,   NODE_1, (uint8)(NODE_BIT(NODE_2) | NODE_BIT(NODE_3) | NODE_BIT(NODE_4)), "MOTOR"   },
    { CAN_ID_TABLERO, NODE_2, (uint8)(NODE_BIT(NODE_1) | NODE_BIT(NODE_3)),                    "TABLERO" },
    { CAN_ID_PUERTAS, NODE_3, (uint8)(NODE_BIT(NODE_1) | NODE_BIT(NODE_4)),                    "PUERTAS" },
    { CAN_ID_CLIMA,   NODE_4, (uint8)(NODE_BIT(NODE_2)),                                       "CLIMA"   },
};

#define CAN_NET_NUM_MESSAGES    ((uint8)(sizeof(s_messages) / sizeof(s_messages[0])))

uint32 CanNet_TxId(void)
{
    uint8 i;

    for (i = 0U; i < CAN_NET_NUM_MESSAGES; i++)
    {
        if (s_messages[i].Publisher == NODE_ID)
        {
            return s_messages[i].Id;
        }
    }
    return 0U;
}

uint8 CanNet_BuildRxIds(uint32 *Ids, uint8 MaxIds)
{
    uint8 Count = 0U;
    uint8 i;

    for (i = 0U; (i < CAN_NET_NUM_MESSAGES) && (Count < MaxIds); i++)
    {
        if ((s_messages[i].Subscribers & NODE_BIT(NODE_ID)) != 0U)
        {
            Ids[Count] = s_messages[i].Id;
            Count++;
        }
    }
    return Count;
}

const CanNet_MessageType *CanNet_FindMessage(uint32 Id)
{
    uint8 i;

    for (i = 0U; i < CAN_NET_NUM_MESSAGES; i++)
    {
        if (s_messages[i].Id == Id)
        {
            return &s_messages[i];
        }
    }
    return NULL_PTR;
}

const char *CanNet_NodeName(uint8 Node)
{
    static const char * const Names[] = { "?", "MOTOR", "TABLERO", "PUERTAS", "CLIMA" };
    return (Node < (sizeof(Names) / sizeof(Names[0]))) ? Names[Node] : "?";
}

uint8 CanNet_Checksum(const uint8 *Data)
{
    uint8 Xor = 0U;
    uint8 i;

    for (i = 0U; i < CAN_PL_CHECKSUM; i++)
    {
        Xor ^= Data[i];
    }
    return Xor;
}
