/*
 * serial.c - Serial port communication module
 *
 * Implements serial port enumeration, open/close, and data loopback
 * using WaitCommEvent-based event-driven I/O.
 *
 * Based on SerialPort implementation by David MacDermot.
 */

#include "serial.h"
#include "resource.h"
#include "trace.h"
#include <setupapi.h>
#include <devguid.h>
#include <stdio.h>

#pragma comment(lib, "setupapi.lib")

static const char *TAG = "SER";

#define READ_BUFFER_SIZE 4096
#define MAX_PORTS 64

/* Port info structure for friendly name display */
typedef struct {
    WCHAR portName[32];
    WCHAR friendlyName[128];
} PORT_INFO;

static PORT_INFO g_portInfo[MAX_PORTS];
static int g_portCount = 0;

/*
 * Serial_EnumPorts - Enumerate available serial ports with friendly names
 *
 * Uses SetupDi API to get device friendly names from registry.
 */
BOOL Serial_EnumPorts(HWND hCombo)
{
    g_portCount = 0;
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);

    HDEVINFO devInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE)
        return FALSE;

    SP_DEVINFO_DATA devInfoData = { .cbSize = sizeof(SP_DEVINFO_DATA) };
    BOOL found = FALSE;

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData) && g_portCount < MAX_PORTS; i++) {
        /* Get port name from registry */
        HKEY hKey = SetupDiOpenDevRegKey(devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hKey == INVALID_HANDLE_VALUE)
            continue;

        WCHAR portName[32] = {0};
        DWORD size = sizeof(portName);
        DWORD type = 0;
        LONG ret = RegQueryValueExW(hKey, L"PortName", NULL, &type, (LPBYTE)portName, &size);
        RegCloseKey(hKey);

        if (ret != ERROR_SUCCESS || type != REG_SZ)
            continue;

        /* Get friendly name */
        WCHAR friendlyName[128] = {0};
        DWORD friendlySize = sizeof(friendlyName);
        if (!SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData, SPDRP_FRIENDLYNAME,
                                               NULL, (PBYTE)friendlyName, friendlySize, NULL)) {
            /* Fallback to port name if no friendly name */
            lstrcpyW(friendlyName, portName);
        }

        /* Store port info */
        lstrcpyW(g_portInfo[g_portCount].portName, portName);
        lstrcpyW(g_portInfo[g_portCount].friendlyName, friendlyName);

        /* Add to combo box: friendly name only */
        int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)friendlyName);
        if (idx >= 0) {
            SendMessageW(hCombo, CB_SETITEMDATA, idx, (LPARAM)g_portCount);
            found = TRUE;
        }
        g_portCount++;
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    if (found)
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);

    return found;
}

/*
 * GetPortNameFromCombo - Get port name from combo box selection
 *
 * Returns port name (e.g. "COM10") from the selected combo item.
 */
static BOOL GetPortNameFromCombo(HWND hCombo, int sel, WCHAR *portName, int maxLen)
{
    if (sel < 0)
        return FALSE;

    int portIdx = (int)SendMessageW(hCombo, CB_GETITEMDATA, sel, 0);
    if (portIdx < 0 || portIdx >= g_portCount)
        return FALSE;

    lstrcpynW(portName, g_portInfo[portIdx].portName, maxLen);
    return TRUE;
}

/*
 * Listener_Proc - Monitor serial port using WaitCommEvent
 *
 * This thread waits for comm events (EV_RXCHAR, etc.) and posts
 * notifications to the UI thread.
 */
static DWORD WINAPI Listener_Proc(LPVOID param)
{
    SERIAL_CTX *ctx = (SERIAL_CTX *)param;
    BYTE buffer[READ_BUFFER_SIZE];
    DWORD dwEvtMask = 0;
    OVERLAPPED ov = {0};
    HANDLE hReadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE hWriteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    TRACE_LOG(TAG, "Listener started");

    if (!hReadEvent || !hWriteEvent) {
        TRACE_LOG(TAG, "ERROR: Failed to create events");
        if (hReadEvent) CloseHandle(hReadEvent);
        if (hWriteEvent) CloseHandle(hWriteEvent);
        return 1;
    }

    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent) {
        TRACE_LOG(TAG, "ERROR: Failed to create ov.hEvent");
        CloseHandle(hReadEvent);
        CloseHandle(hWriteEvent);
        return 1;
    }

    /* Signal that thread has started */
    SetEvent(ctx->hStartEvent);

    while (ctx->bRunning) {
        /* Set comm mask to listen for receive events */
        if (!SetCommMask(ctx->hPort, EV_RXCHAR | EV_ERR))
            break;

        /* Wait for comm event */
        ResetEvent(ov.hEvent);

        if (!WaitCommEvent(ctx->hPort, &dwEvtMask, &ov)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                DWORD waitResult = WaitForSingleObject(ov.hEvent, 100);
                if (waitResult == WAIT_TIMEOUT)
                    continue;
                if (waitResult != WAIT_OBJECT_0)
                    break;
                if (!GetOverlappedResult(ctx->hPort, &ov, &(DWORD){0}, FALSE))
                    break;
            } else if (GetLastError() == ERROR_OPERATION_ABORTED) {
                break;
            } else {
                TRACE_LOG(TAG, "ERROR: WaitCommEvent failed: %lu", GetLastError());
                break;
            }
        }

        if (!ctx->bRunning)
            break;

        /* Handle received data */
        if (dwEvtMask & EV_RXCHAR) {
            DWORD bytesRead = 0;
            COMSTAT comStat;
            DWORD dwErrors;

            if (!ClearCommError(ctx->hPort, &dwErrors, &comStat))
                break;

            if (comStat.cbInQue > 0 && comStat.cbInQue < READ_BUFFER_SIZE) {
                DWORD toRead = comStat.cbInQue;
                OVERLAPPED ovRead = {0};
                ovRead.hEvent = hReadEvent;
                ResetEvent(hReadEvent);

                if (ReadFile(ctx->hPort, buffer, toRead, &bytesRead, &ovRead)) {
                    /* Read completed synchronously */
                } else if (GetLastError() == ERROR_IO_PENDING) {
                    if (WaitForSingleObject(hReadEvent, 1000) != WAIT_OBJECT_0)
                        continue;
                    if (!GetOverlappedResult(ctx->hPort, &ovRead, &bytesRead, FALSE))
                        continue;
                } else {
                    continue;
                }

                if (bytesRead > 0 && ctx->bRunning) {
                    /* Post RX data to UI thread for logging */
                    if (ctx->hNotify && IsWindow(ctx->hNotify)) {
                        size_t allocSize = (size_t)bytesRead + 1;
                        void *copy = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, allocSize);
                        if (copy) {
                            CopyMemory(copy, buffer, bytesRead);
                            if (!PostMessage(ctx->hNotify, WM_USER + 1, (WPARAM)bytesRead, (LPARAM)copy)) {
                                TRACE_LOG(TAG, "ERROR: PostMessage RX failed: %lu", GetLastError());
                                HeapFree(GetProcessHeap(), 0, copy);
                            }
                        }
                    }

                    /* Echo received data back (TX) */
                    DWORD bytesWritten = 0;
                    OVERLAPPED ovWrite = {0};
                    ovWrite.hEvent = hWriteEvent;
                    ResetEvent(hWriteEvent);

                    if (WriteFile(ctx->hPort, buffer, bytesRead, &bytesWritten, &ovWrite)) {
                        /* Write completed synchronously */
                    } else if (GetLastError() == ERROR_IO_PENDING) {
                        WaitForSingleObject(hWriteEvent, 1000);
                        GetOverlappedResult(ctx->hPort, &ovWrite, &bytesWritten, FALSE);
                    }

                    /* Post TX data to UI thread for logging */
                    if (bytesWritten > 0 && ctx->hNotify && IsWindow(ctx->hNotify)) {
                        size_t allocSize = (size_t)bytesWritten + 1;
                        void *copy = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, allocSize);
                        if (copy) {
                            CopyMemory(copy, buffer, bytesWritten);
                            if (!PostMessage(ctx->hNotify, WM_USER + 2, (WPARAM)bytesWritten, (LPARAM)copy)) {
                                TRACE_LOG(TAG, "ERROR: PostMessage TX failed: %lu", GetLastError());
                                HeapFree(GetProcessHeap(), 0, copy);
                            }
                        }
                    }

                    ctx->dwRxBytes += bytesRead;
                    ctx->dwTxBytes += bytesWritten;
                }
            }
        }

        /* Handle errors */
        if (dwEvtMask & EV_ERR) {
            DWORD dwErrors;
            ClearCommError(ctx->hPort, &dwErrors, NULL);
            TRACE_LOG(TAG, "Comm error: 0x%08lX", dwErrors);
        }
    }

    TRACE_LOG(TAG, "Listener exiting");
    CloseHandle(ov.hEvent);
    CloseHandle(hReadEvent);
    CloseHandle(hWriteEvent);
    return 0;
}

/*
 * Serial_Open - Open a serial port with 115200,8N1 configuration
 */
BOOL Serial_Open(SERIAL_CTX *ctx, const WCHAR *portName, HWND hNotify)
{
    TRACE_LOG(TAG, "Opening port: %s", portName);

    if (ctx->hPort != INVALID_HANDLE_VALUE && ctx->hPort != NULL)
        return FALSE;

    /* Create events */
    ctx->hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ctx->hIOEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (!ctx->hStartEvent || !ctx->hIOEvent) {
        if (ctx->hStartEvent) CloseHandle(ctx->hStartEvent);
        if (ctx->hIOEvent) CloseHandle(ctx->hIOEvent);
        return FALSE;
    }

    /* Open port with \\.\ prefix */
    WCHAR fullPort[32];
    wsprintfW(fullPort, L"\\\\.\\%s", portName);

    ctx->hPort = CreateFileW(fullPort, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                             OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (ctx->hPort == INVALID_HANDLE_VALUE) {
        TRACE_LOG(TAG, "ERROR: CreateFileW failed: %lu", GetLastError());
        CloseHandle(ctx->hStartEvent);
        CloseHandle(ctx->hIOEvent);
        return FALSE;
    }

    /* Configure serial port: 115200 baud, 8 data bits, no parity, 1 stop bit */
    DCB dcb = { .DCBlength = sizeof(DCB) };
    if (!GetCommState(ctx->hPort, &dcb)) {
        CloseHandle(ctx->hPort);
        ctx->hPort = INVALID_HANDLE_VALUE;
        CloseHandle(ctx->hStartEvent);
        CloseHandle(ctx->hIOEvent);
        return FALSE;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(ctx->hPort, &dcb)) {
        CloseHandle(ctx->hPort);
        ctx->hPort = INVALID_HANDLE_VALUE;
        CloseHandle(ctx->hStartEvent);
        CloseHandle(ctx->hIOEvent);
        return FALSE;
    }

    /* Set timeouts */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 100;
    SetCommTimeouts(ctx->hPort, &timeouts);

    /* Setup comm buffers */
    SetupComm(ctx->hPort, READ_BUFFER_SIZE, READ_BUFFER_SIZE);

    /* Purge any existing data */
    PurgeComm(ctx->hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);

    /* Set DTR and RTS */
    EscapeCommFunction(ctx->hPort, SETDTR);
    EscapeCommFunction(ctx->hPort, SETRTS);

    /* Start listener thread */
    ctx->hNotify = hNotify;
    ctx->bRunning = TRUE;
    ctx->dwRxBytes = 0;
    ctx->dwTxBytes = 0;

    ctx->hThread = CreateThread(NULL, 0, Listener_Proc, ctx, 0, NULL);
    if (!ctx->hThread) {
        TRACE_LOG(TAG, "ERROR: CreateThread failed: %lu", GetLastError());
        ctx->bRunning = FALSE;
        CloseHandle(ctx->hPort);
        ctx->hPort = INVALID_HANDLE_VALUE;
        CloseHandle(ctx->hStartEvent);
        CloseHandle(ctx->hIOEvent);
        return FALSE;
    }

    /* Wait for thread to start */
    WaitForSingleObject(ctx->hStartEvent, 1000);
    TRACE_LOG(TAG, "Port opened successfully");

    return TRUE;
}

/*
 * Serial_Close - Close serial port and stop listener thread
 */
void Serial_Close(SERIAL_CTX *ctx)
{
    TRACE_LOG(TAG, "Closing port");

    if (ctx->bRunning) {
        ctx->bRunning = FALSE;

        /* Clear notify window to prevent new messages */
        ctx->hNotify = NULL;

        /* Unblock WaitCommEvent by clearing comm mask */
        if (ctx->hPort != INVALID_HANDLE_VALUE && ctx->hPort != NULL) {
            SetCommMask(ctx->hPort, 0);
            EscapeCommFunction(ctx->hPort, CLRDTR);
            PurgeComm(ctx->hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);
            CancelIo(ctx->hPort);
        }

        /* Wait for thread to exit */
        if (ctx->hThread) {
            WaitForSingleObject(ctx->hThread, 2000);
            CloseHandle(ctx->hThread);
            ctx->hThread = NULL;
        }
    }

    /* Close port handle */
    if (ctx->hPort != INVALID_HANDLE_VALUE && ctx->hPort != NULL) {
        CloseHandle(ctx->hPort);
        ctx->hPort = INVALID_HANDLE_VALUE;
    }

    /* Close events */
    if (ctx->hStartEvent) {
        CloseHandle(ctx->hStartEvent);
        ctx->hStartEvent = NULL;
    }
    if (ctx->hIOEvent) {
        CloseHandle(ctx->hIOEvent);
        ctx->hIOEvent = NULL;
    }
}

/* Check if serial port is open */
BOOL Serial_IsOpen(const SERIAL_CTX *ctx)
{
    return (ctx->hPort != INVALID_HANDLE_VALUE && ctx->hPort != NULL && ctx->bRunning);
}

/* Get total received bytes */
DWORD Serial_GetRxBytes(const SERIAL_CTX *ctx)
{
    return ctx->dwRxBytes;
}

/* Get port name by index */
BOOL Serial_GetPortName(int index, WCHAR *portName, int maxLen)
{
    if (index < 0 || index >= g_portCount)
        return FALSE;
    lstrcpynW(portName, g_portInfo[index].portName, maxLen);
    return TRUE;
}

/* Get total sent bytes */
DWORD Serial_GetTxBytes(const SERIAL_CTX *ctx)
{
    return ctx->dwTxBytes;
}
