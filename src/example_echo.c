/*
 * example_echo.c - Echo protocol example
 *
 * ECHO protocol: received data is sent back as-is (loopback).
 * Ping function: sends random data for testing.
 *
 * Uses example_echo_hal.h for platform operations (send, log).
 * Does NOT depend on serial.h directly.
 *
 * This is a DEMO protocol. Replace this file with your own protocol
 * implementation for real applications.
 *
 * ============================================================
 * Extension Guide
 * ============================================================
 *
 * To implement your protocol:
 *
 * 1. Create your callback function:
 *
 *    void MyProtocol_ProcessData(void *ctx, const BYTE *data,
 *                                 DWORD len, HWND hNotify)
 *    {
 *        if (data[0] == 0x01) {
 *            BYTE resp[] = {0x01, GetSensorValue()};
 *            echo_hal_write(resp, sizeof(resp));
 *            echo_hal_log(L"MY", L"Query: value=%d", GetSensorValue());
 *        }
 *    }
 *
 * 2. Register your callback in Main_OnConnect:
 *
 *    Serial_SetReceiveCallback(&g_serial, (SERIAL_RX_CB)MyProtocol_ProcessData);
 *
 * ============================================================
 */

#include "example_echo_hal.h"
#include "utils/trace.h"
#include <stdlib.h>

#if ENABLE_TRACE
static const char *TAG = "ECHO";
#endif

#define PING_MIN_SIZE   1
#define PING_MAX_SIZE   256

void ExampleEcho_Init(void)
{
    srand((unsigned int)GetTickCount64());
    TRACE_PROTO(TAG, "Example Echo initialized");
}

/*
 * ExampleEcho_ProcessData - ECHO protocol handler
 *
 * Receives data and sends it back unchanged (loopback).
 * This is the demo callback registered via Serial_SetReceiveCallback().
 */
void ExampleEcho_ProcessData(void *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    (void)ctx;
    (void)hNotify;

    if (!data || len == 0)
        return;

    echo_hal_log(L"ECHO", L"Received %lu bytes", len);

    /* Allocate buffer for response */
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    if (!buf)
        return;

    /* Copy and send back */
    CopyMemory(buf, data, len);
    echo_hal_write(buf, len);
    HeapFree(GetProcessHeap(), 0, buf);

    echo_hal_log(L"ECHO", L"Send %lu bytes", len);
}

/*
 * ExampleEcho_SendPing - Send random data for testing
 *
 * Generates and sends 1-256 bytes of random data.
 * Useful for testing serial connection and throughput.
 */
void ExampleEcho_SendPing(void)
{
    DWORD size = PING_MIN_SIZE + (rand() % (PING_MAX_SIZE - PING_MIN_SIZE + 1));

    BYTE *data = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    if (!data)
        return;

    for (DWORD i = 0; i < size; i++)
        data[i] = (BYTE)(rand() % 256);

    echo_hal_write(data, size);
    HeapFree(GetProcessHeap(), 0, data);

    echo_hal_log(L"PING", L"Sent %lu random bytes", size);
}
