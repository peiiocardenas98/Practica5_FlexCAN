/**
 * @file    node_cfg.h
 * @brief   Rol de esta tarjeta dentro de la red CAN.
 *
 * Es el UNICO archivo que cambia entre tarjetas: antes de compilar y grabar
 * cada S32K312MINI-EVB, ajustar NODE_ID (y ARBITRATION_TEST igual en todas).
 *
 * Ambos valores tambien se pueden pasar desde el compilador
 * (-DNODE_ID=NODE_3), por eso van dentro de #ifndef.
 */
#ifndef NODE_CFG_H
#define NODE_CFG_H

/* Identificadores de nodo (tambien son el indice en la tabla de la red) */
#define NODE_1          1U      /* ECU Motor / Master  -> TX 0x100 */
#define NODE_2          2U      /* ECU Tablero         -> TX 0x200 */
#define NODE_3          3U      /* ECU Puertas         -> TX 0x300 */
#define NODE_4          4U      /* ECU Clima (opcional)-> TX 0x400 */

/* -------------------------------------------------------------------------
 * Rol de ESTA tarjeta: NODE_1, NODE_2, NODE_3 o NODE_4
 * ------------------------------------------------------------------------- */
#ifndef NODE_ID
#define NODE_ID         NODE_1
#endif

/* -------------------------------------------------------------------------
 * Prueba de arbitraje (0 = normal, 1 = prueba). Debe ser igual en todas.
 *   Nodo 2 envia 0x200 cada CAN_ARB_PERIOD_MS; Nodo 1 y Nodo 3 lo reciben y
 *   encolan 0x100 y 0x300 en el mismo instante -> compiten por el bus.
 * ------------------------------------------------------------------------- */
#ifndef ARBITRATION_TEST
#define ARBITRATION_TEST    0U
#endif

/* Tiempos de la aplicacion, en ms */
#define CAN_HEARTBEAT_MS    500U    /* Trama periodica con el "contador vivo" */
#define CAN_ARB_PERIOD_MS   10U     /* Periodo del disparador en la prueba de arbitraje */
#define BUTTON_DEBOUNCE_MS  20U

/* Validaciones en tiempo de compilacion */
#if (NODE_ID < NODE_1) || (NODE_ID > NODE_4)
#error "NODE_ID invalido: usar NODE_1, NODE_2, NODE_3 o NODE_4"
#endif
#if (ARBITRATION_TEST != 0U) && (ARBITRATION_TEST != 1U)
#error "ARBITRATION_TEST debe ser 0 o 1"
#endif

#endif /* NODE_CFG_H */
