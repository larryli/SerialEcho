/*
 * serial.h - Serial port communication module interface
 *
 * Provides functions for enumerating, opening, closing serial ports,
 * and reading/writing data using WaitCommEvent-based event-driven I/O.
 *
 * The receive callback allows decoupling the protocol handler from
 * the serial module. Register a callback via Serial_SetReceiveCallback()
 * to process received data.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <windows.h>

/* Data direction for log display */
typedef enum { DIR_RX, DIR_TX } DATA_DIR;

/* Receive callback type - called when data is received
 * @ctx: Serial context
 * @data: Received data buffer
 * @len: Data length
 * @hNotify: Window handle for UI notifications
 */
typedef void (*SERIAL_RX_CB)(void *ctx, const BYTE *data, DWORD len, HWND hNotify);

/* Serial port context */
typedef struct {
    HANDLE hPort;           /* Serial port handle */
    HANDLE hThread;         /* Listener thread handle */
    HANDLE hStartEvent;     /* Thread start synchronization event */
    HANDLE hIOEvent;        /* I/O completion event */
    HWND hNotify;           /* Window to receive WM_USER+1/+2/+3 */
    volatile BOOL bRunning; /* Thread running flag */
    volatile DWORD dwRxBytes; /* Total bytes received */
    volatile DWORD dwTxBytes; /* Total bytes sent */
    SERIAL_RX_CB onReceive; /* Receive data callback */
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
 * @hNotify: Window to receive data/error notifications
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

/*
 * Serial_WriteData - Write data to serial port
 * @ctx: Serial context
 * @data: Data to write
 * @len: Length of data
 * @hNotify: Window to receive TX notification (can be NULL)
 * Returns: Number of bytes written
 */
DWORD Serial_WriteData(SERIAL_CTX *ctx, const BYTE *data, DWORD len, HWND hNotify);

/*
 * Serial_SetReceiveCallback - Set the receive data callback
 * @ctx: Serial context
 * @cb: Callback function (NULL to disable)
 */
void Serial_SetReceiveCallback(SERIAL_CTX *ctx, SERIAL_RX_CB cb);

/*
 * Serial_PostLog - Post a custom log message to the UI
 *
 * Thread-safe function for protocol layer to display log messages.
 * Allocates copies of tag and text, caller does not need to keep them.
 *
 * @hNotify: Window handle (from SERIAL_CTX.hNotify)
 * @tag: Tag text (e.g. "PING", "ERR")
 * @text: Log message text
 */
void Serial_PostLog(HWND hNotify, const WCHAR *tag, const WCHAR *text);

#endif /* SERIAL_H */
