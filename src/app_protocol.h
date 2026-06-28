/*
 * app_protocol.h - Protocol signal handling
 *
 * Handles signal change notifications and configuration logging.
 */

#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include "serial.h"
#include <windows.h>

/*
 * AppProtocol_OnSignal - Handle serial signal change notification
 *
 * Logs DSR/CTS/RI/DCD changes to the log display.
 */
void AppProtocol_OnSignal(DWORD modemStatus);

/*
 * AppProtocol_OnDtrChange - Handle DTR signal change
 */
void AppProtocol_OnDtrChange(BOOL state);

/*
 * AppProtocol_OnRtsChange - Handle RTS signal change
 */
void AppProtocol_OnRtsChange(BOOL state);

/*
 * AppProtocol_OnConfigChange - Handle serial configuration change
 */
void AppProtocol_OnConfigChange(SERIAL_CTX *ctx);

#endif /* APP_PROTOCOL_H */
