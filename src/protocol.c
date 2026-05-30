/*
 * protocol.c - Protocol handler implementation
 *
 * ============================================================
 * Extension Guide
 * ============================================================
 *
 * To implement your protocol:
 *
 * 1. Create your callback function:
 *
 *    void MyProtocol_ProcessData(SERIAL_CTX *ctx, const BYTE *data,
 *                                 DWORD len, HWND hNotify)
 *    {
 *        if (data[0] == 0x01) {
 *            BYTE resp[] = {0x01, GetSensorValue()};
 *            Serial_WriteData(ctx, resp, sizeof(resp), hNotify);
 *        }
 *    }
 *
 * 2. Register your callback in Main_OnConnect:
 *
 *    Serial_SetReceiveCallback(&g_serial, (SERIAL_RX_CB)MyProtocol_ProcessData);
 *
 * ============================================================
 */

#include "protocol.h"
#include "utils/trace.h"
#include <stdlib.h>

static const char *TAG = "PROTO";

#define PING_MIN_SIZE   1
#define PING_MAX_SIZE   256

void Protocol_Init(void)
{
    srand((unsigned int)GetTickCount64());
    TRACE_PROTO(TAG, "Protocol module initialized");
}

void Protocol_ProcessData(SERIAL_CTX *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    if (!ctx || !data || len == 0)
        return;

    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    if (!buf)
        return;

    CopyMemory(buf, data, len);
    Serial_WriteData(ctx, buf, len, hNotify);
    HeapFree(GetProcessHeap(), 0, buf);
}

void Protocol_SendPing(SERIAL_CTX *ctx, HWND hNotify)
{
    if (!ctx)
        return;

    DWORD size = PING_MIN_SIZE + (rand() % (PING_MAX_SIZE - PING_MIN_SIZE + 1));

    BYTE *data = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    if (!data)
        return;

    for (DWORD i = 0; i < size; i++)
        data[i] = (BYTE)(rand() % 256);

    Serial_WriteData(ctx, data, size, hNotify);

    /* Log ping via message mechanism */
    WCHAR logBuf[64];
    wsprintfW(logBuf, L"Sent %lu random bytes", size);
    Serial_PostLog(hNotify, L"PING", logBuf);

    HeapFree(GetProcessHeap(), 0, data);
}
