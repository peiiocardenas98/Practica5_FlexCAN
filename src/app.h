/**
 * @file    app.h
 * @brief   Aplicacion de la red CAN: el comportamiento depende de NODE_ID (node_cfg.h).
 */
#ifndef APP_H
#define APP_H

#include "Std_Types.h"

/** @brief Inicializa transceiver y driver CAN con la tabla de este nodo. */
void App_Init(void);

/** @brief Tarea periodica: boton, transmision, tramas recibidas y estadisticas. */
void App_MainFunction(uint32 NowMs);

#endif /* APP_H */
