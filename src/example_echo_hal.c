/*
 * example_echo_hal.c - Platform contract implementation (echo demo)
 *
 * Implements example_echo_hal.h by forwarding to serial.c functions.
 * This is the only file that knows about serial.c.
 *
 * This is a DEMO HAL. Replace with your own HAL for real applications.
 */

#include "example_echo_hal.h"
#include "serial.h"
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

/* Serial context (set via EchoHalInit) */
static SERIAL_CTX *s_serial = NULL;

void EchoHalInit(void *serial)
{
    s_serial = (SERIAL_CTX *)serial;
}

DWORD echo_hal_write(const BYTE *data, DWORD len)
{
    if (!s_serial)
        return 0;
    return Serial_WriteData(s_serial, data, len, s_serial->hNotify);
}

void echo_hal_log(const WCHAR *tag, const WCHAR *fmt, ...)
{
    if (!s_serial)
        return;
    va_list ap;
    va_start(ap, fmt);
    WCHAR buf[1024];
    _vsnwprintf(buf, sizeof(buf) / sizeof(WCHAR), fmt, ap);
    buf[sizeof(buf) / sizeof(WCHAR) - 1] = L'\0';
    va_end(ap);
    Serial_PostLog(s_serial->hNotify, tag, buf);
}
