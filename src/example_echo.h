/*
 * example_echo.h - Echo protocol example interface
 *
 * Protocol layer uses example_echo_hal.h for platform operations.
 * Does NOT depend on serial.h.
 *
 * This is a DEMO header. Replace with your own protocol header
 * for real applications.
 */

#ifndef EXAMPLE_ECHO_H
#define EXAMPLE_ECHO_H

#include <windows.h>

/*
 * ExampleEcho_Init - Initialize echo example module
 */
void ExampleEcho_Init(void);

/*
 * ExampleEcho_ProcessData - ECHO callback: send received data back
 *
 * Matches SERIAL_RX_CB signature (registered via Serial_SetReceiveCallback).
 *
 * @ctx: Opaque context (SERIAL_CTX* cast to void*)
 * @data: Received data
 * @len: Length of received data
 * @hNotify: Window for UI notifications
 */
void ExampleEcho_ProcessData(void *ctx, const BYTE *data, DWORD len, HWND hNotify);

/*
 * ExampleEcho_SendPing - Send random test data (1-256 bytes)
 */
void ExampleEcho_SendPing(void);

#endif /* EXAMPLE_ECHO_H */
