# Práctica 5: Red automotriz multi-nodo y arbitraje con bus CAN (FlexCAN)

**Tarjeta:** NXP S32K312MINI-EVB · **IDE:** S32 Design Studio 3.6.5 · **Drivers:** RTD 7.0.1 (`FlexCAN_Ip`)

Red CAN de 3 a 4 nodos a **500 kbps** con IDs estándar de 11 bits. Un solo firmware; el rol se elige con `NODE_ID` en `src/node_cfg.h`.

| Nodo | TX (MB0) | RX MB1 / MB2 (máscara 0x7FF) | LED | Función |
|---|---|---|---|---|
| 1 Motor / Master | 0x100 | 0x200 / 0x300 | Verde | SW2 + comando de sincronización 0xA5 |
| 2 Tablero | 0x200 | 0x100 / 0x400 | Azul | SW2 + contador vivo |
| 3 Puertas | 0x300 | 0x100 / 0x200 | Rojo | SW2 (sensor de puerta) |
| 4 Clima (opc.) | 0x400 | 0x100 / 0x300 | Rojo / Azul | Telemetría simulada; responde a 0x100 |

**Payload (8 bytes):** `[0]` nodo emisor · `[1]` SW2 (0x01/0x00) · `[2]` contador 0x00–0xFF · `[3]` comando · `[4]` temperatura simulada · `[5]` tramas recibidas · `[6]` TEC · `[7]` XOR de 0..6.

## Estructura

```
src/
  main.c            Plataforma (reloj, pines, IRQ) + lazo principal
  node_cfg.h        NODE_ID y ARBITRATION_TEST
  can_network.c/h   Tabla de la red: quién publica y quién recibe cada ID
  app.c/h           Botón, payload, LEDs por emisor, log
libraries/
  drivers/drv_can/    can_drv.c/h    FlexCAN_Ip: 500 kbps, máscaras individuales, MBs, flexcan_callback
  drivers/drv_board/  board_io.c/h   LEDs RGB, SW2, EN/STB_N/ERR_N del TJA1043
  system/sys_tick/    tick.c/h       SysTick 1 ms
  system/sys_log/     log.c/h        Consola LPUART6 (OpenSDA, 115200 8N1)
```

## Hardware

- CAN0: PTA7 (TX) / PTA6 (RX) → TJA1043. EN = PTC21 y STB_N = PTC20 en alto (modo Normal).
- Bus lineal: CAN_H con CAN_H, CAN_L con CAN_L y GND común. 60 Ω entre CAN_H y CAN_L con todo apagado.
- El TJA1043 necesita `S_VBAT` ≈ 12 V (boost del FS26 o jack de 12 V, J19/J20).
- SW1 de la MINI-EVB es RESET: el botón del nodo es **SW2 (PTB19)**.

## Prueba de arbitraje

`ARBITRATION_TEST = 1` en todas las tarjetas. El Nodo 2 envía 0x200 cada 10 ms. Los nodos 1 y 3 lo reciben y encolan 0x100 y 0x300 en el mismo instante. 0x100 gana el arbitraje sin corromperse y 0x300 se retransmite justo después.
