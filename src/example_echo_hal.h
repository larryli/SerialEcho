/*
 * example_echo_hal.h - Protocol layer platform contract (echo demo)
 *
 * Defines the interface that protocol implementations must use
 * for platform-specific operations (serial I/O, logging).
 *
 * This is a DEMO HAL. Replace with your own HAL for real applications.
 */

#ifndef EXAMPLE_ECHO_HAL_H
#define EXAMPLE_ECHO_HAL_H

#include <windows.h>

/*
 * EchoHalInit - Initialize HAL with serial context
 * @serial: Opaque pointer to serial context (SERIAL_CTX*)
 */
void EchoHalInit(void *serial);

/*
 * echo_hal_write - Send data through serial port
 */
DWORD echo_hal_write(const BYTE *data, DWORD len);

/*
 * echo_hal_log - Post formatted log message to UI
 */
void echo_hal_log(const WCHAR *tag, const WCHAR *fmt, ...);

#endif /* EXAMPLE_ECHO_HAL_H */
