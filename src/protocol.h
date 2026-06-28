/*
 * protocol.h - Protocol handler interface
 *
 * Protocol layer uses echo_hal.h for platform operations.
 * Does NOT depend on serial.h.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <windows.h>

/*
 * Protocol_Init - Initialize protocol module
 */
void Protocol_Init(void);

/*
 * Protocol_ProcessData - ECHO callback: send received data back
 *
 * Matches SERIAL_RX_CB signature (registered via Serial_SetReceiveCallback).
 *
 * @ctx: Opaque context (SERIAL_CTX* cast to void*)
 * @data: Received data
 * @len: Length of received data
 * @hNotify: Window for UI notifications
 */
void Protocol_ProcessData(void *ctx, const BYTE *data, DWORD len, HWND hNotify);

/*
 * Protocol_SendPing - Send random test data (1-256 bytes)
 */
void Protocol_SendPing(void);

#endif /* PROTOCOL_H */
