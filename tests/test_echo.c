/*
 * test_echo.c - Unit tests for Echo protocol
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

/* Mock serial types */
typedef enum { DIR_RX, DIR_TX } DATA_DIR;
typedef void (*SERIAL_RX_CB)(void *ctx, const BYTE *data, DWORD len, HWND hNotify);
typedef void (*SERIAL_SIGNAL_CB)(void *ctx, DWORD modemStatus, HWND hNotify);
typedef struct {
    HANDLE hPort; HANDLE hThread; HANDLE hStartEvent; HWND hNotify;
    volatile BOOL bRunning; volatile DWORD dwRxBytes; volatile DWORD dwTxBytes;
    SERIAL_RX_CB onReceive; SERIAL_SIGNAL_CB onSignal;
} SERIAL_CTX;

/* Mock write capture */
static BYTE stub_write_buf[4096];
static DWORD stub_write_len = 0;

static DWORD Serial_WriteData(SERIAL_CTX *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    (void)ctx; (void)hNotify;
    if (data && len > 0 && len <= sizeof(stub_write_buf)) {
        memcpy(stub_write_buf, data, len);
        stub_write_len = len;
    }
    return len;
}

static void Serial_PostLogF(HWND hNotify, const WCHAR *tag, const WCHAR *fmt, ...)
{
    (void)hNotify; (void)tag; (void)fmt;
}

/* Protocol implementation (inlined from example_echo.c) */
static void Protocol_ProcessData(SERIAL_CTX *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    if (!ctx || !data || len == 0)
        return;
    Serial_PostLogF(hNotify, L"ECHO", L"Received %lu bytes", len);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    if (!buf) return;
    CopyMemory(buf, data, len);
    Serial_WriteData(ctx, buf, len, hNotify);
    HeapFree(GetProcessHeap(), 0, buf);
    Serial_PostLogF(hNotify, L"ECHO", L"Send %lu bytes", len);
}

/* Test helpers */
static SERIAL_CTX g_mock_serial = {0};
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { printf("  PASS: %s\n", msg); tests_passed++; } \
    else { printf("  FAIL: %s\n", msg); tests_failed++; } \
} while(0)

static void reset(void) { memset(&stub_write_buf, 0, sizeof(stub_write_buf)); stub_write_len = 0; }
extern BYTE stub_write_buf[];
extern DWORD stub_write_len;

/* Tests */
static void test_echo_simple(void) {
    printf("\nTest: Echo simple data\n");
    reset();
    BYTE input[] = {1,2,3,4,5};
    Protocol_ProcessData(&g_mock_serial, input, 5, NULL);
    TEST_ASSERT(stub_write_len == 5, "Write length matches");
    TEST_ASSERT(memcmp(stub_write_buf, input, 5) == 0, "Data matches");
}
static void test_echo_single_byte(void) {
    printf("\nTest: Echo single byte\n");
    reset();
    BYTE input[] = {0xAA};
    Protocol_ProcessData(&g_mock_serial, input, 1, NULL);
    TEST_ASSERT(stub_write_len == 1, "Length is 1");
    TEST_ASSERT(stub_write_buf[0] == 0xAA, "Byte matches");
}
static void test_echo_empty(void) {
    printf("\nTest: Echo empty (len=0)\n");
    reset();
    BYTE input[] = {1};
    Protocol_ProcessData(&g_mock_serial, input, 0, NULL);
    TEST_ASSERT(stub_write_len == 0, "Nothing written");
}
static void test_echo_null_data(void) {
    printf("\nTest: Echo NULL data\n");
    reset();
    Protocol_ProcessData(&g_mock_serial, NULL, 5, NULL);
    TEST_ASSERT(stub_write_len == 0, "Nothing written");
}
static void test_echo_large(void) {
    printf("\nTest: Echo large (4096)\n");
    reset();
    BYTE input[4096];
    for (int i = 0; i < 4096; i++) input[i] = (BYTE)(i & 0xFF);
    Protocol_ProcessData(&g_mock_serial, input, 4096, NULL);
    TEST_ASSERT(stub_write_len == 4096, "Length matches");
    TEST_ASSERT(memcmp(stub_write_buf, input, 4096) == 0, "Data matches");
}
static void test_echo_special(void) {
    printf("\nTest: Echo special bytes\n");
    reset();
    BYTE input[] = {0x00, 0xC0, 0xDB, 0xFF};
    Protocol_ProcessData(&g_mock_serial, input, 4, NULL);
    TEST_ASSERT(stub_write_len == 4, "Length matches");
    TEST_ASSERT(memcmp(stub_write_buf, input, 4) == 0, "Special bytes preserved");
}

int main(void) {
    printf("=== Echo Protocol Unit Tests ===\n");
    test_echo_simple();
    test_echo_single_byte();
    test_echo_empty();
    test_echo_null_data();
    test_echo_large();
    test_echo_special();
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
