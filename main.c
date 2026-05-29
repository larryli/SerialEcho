/*
 * main.c - Application entry point
 *
 * SerialEcho: Serial port loopback device simulator using com0com.
 * This file contains the WinMain entry point and message loop.
 */

#include <windows.h>
#include "gui.h"
#include "config.h"
#include "trace.h"

static const char *TAG = "MAIN";

/* Application entry point */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    TRACE_INIT();
    TRACE_LOG(TAG, "=== SerialEcho Started ===");

    /* Initialize configuration */
    Config_Init();

    /* Initialize GUI subsystem */
    if (!GUI_Init(hInstance)) {
        TRACE_LOG(TAG, "ERROR: GUI_Init failed");
        TRACE_CLOSE();
        return 1;
    }

    /* Create main application window */
    HWND hWnd = GUI_CreateMainWindow(hInstance);
    if (!hWnd) {
        TRACE_LOG(TAG, "ERROR: GUI_CreateMainWindow failed");
        TRACE_CLOSE();
        return 1;
    }

    TRACE_LOG(TAG, "Main window created: %p", hWnd);

    /* Main message loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    TRACE_LOG(TAG, "=== SerialEcho Exiting ===");
    TRACE_CLOSE();

    return (int)msg.wParam;
}
