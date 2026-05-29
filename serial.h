/*
 * serial.h - Serial port communication module interface
 *
 * Provides functions for enumerating, opening, closing serial ports,
 * and reading/writing data using WaitCommEvent-based event-driven I/O.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <windows.h>

/* Data direction for log display */
typedef enum { DIR_RX, DIR_TX } DATA_DIR;

/* Serial port context */
typedef struct {
    HANDLE hPort;           /* Serial port handle */
    HANDLE hThread;         /* Listener thread handle */
    HANDLE hStartEvent;     /* Thread start synchronization event */
    HANDLE hIOEvent;        /* I/O completion event */
    HWND hNotify;           /* Window to receive WM_USER+1 on data arrival */
    volatile BOOL bRunning; /* Thread running flag */
    volatile DWORD dwRxBytes; /* Total bytes received */
    volatile DWORD dwTxBytes; /* Total bytes sent */
} SERIAL_CTX;

/*
 * Serial_EnumPorts - Enumerate available serial ports with friendly names
 * @hCombo: Handle to combo box to populate
 * Returns: TRUE if any ports found
 */
BOOL Serial_EnumPorts(HWND hCombo);

/*
 * Serial_Open - Open a serial port and start listener thread
 * @ctx: Serial context to initialize
 * @portName: Port name (e.g. L"COM10")
 * @hNotify: Window to receive WM_USER+1 on data arrival
 * Returns: TRUE on success
 */
BOOL Serial_Open(SERIAL_CTX *ctx, const WCHAR *portName, HWND hNotify);

/*
 * Serial_Close - Close serial port and stop listener thread
 * @ctx: Serial context to close
 */
void Serial_Close(SERIAL_CTX *ctx);

/*
 * Serial_IsOpen - Check if port is open and running
 * @ctx: Serial context
 * Returns: TRUE if port is open
 */
BOOL Serial_IsOpen(const SERIAL_CTX *ctx);

/*
 * Serial_GetPortName - Get port name by index
 * @index: Port index from combo box item data
 * @portName: Buffer to receive port name
 * @maxLen: Buffer size in characters
 * Returns: TRUE if successful
 */
BOOL Serial_GetPortName(int index, WCHAR *portName, int maxLen);

/*
 * Serial_GetRxBytes - Get total received byte count
 * @ctx: Serial context
 * Returns: Bytes received
 */
DWORD Serial_GetRxBytes(const SERIAL_CTX *ctx);

/*
 * Serial_GetTxBytes - Get total sent byte count
 * @ctx: Serial context
 * Returns: Bytes sent
 */
DWORD Serial_GetTxBytes(const SERIAL_CTX *ctx);

#endif /* SERIAL_H */
