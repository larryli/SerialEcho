/*
 * gui.c - GUI module implementation
 *
 * Implements the main window with toolbar, status bar, and RichEdit
 * log display. Handles menu commands and serial port events.
 */

#include "gui.h"
#include "serial.h"
#include "resource.h"
#include "trace.h"
#include <richedit.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")

static const char *TAG = "GUI";

/* Global state */
static SERIAL_CTX g_serial = { .hPort = NULL, .hThread = NULL, .hStartEvent = NULL, .hIOEvent = NULL, .hNotify = NULL, .bRunning = FALSE };
static HWND g_hToolbar = NULL;
static HWND g_hStatusbar = NULL;
static HWND g_hEdit = NULL;
static WCHAR g_szPort[32] = {0};
static WCHAR g_szSelectedPort[32] = {0};

/* Update menu and toolbar button states based on connection status */
static void UpdateMenuState(HWND hWnd)
{
    HMENU hMenu = GetMenu(hWnd);
    BOOL connected = Serial_IsOpen(&g_serial);

    EnableMenuItem(hMenu, IDM_CONNECT, connected ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(hMenu, IDM_DISCONNECT, connected ? MF_ENABLED : MF_GRAYED);

    SendMessageW(g_hToolbar, TB_ENABLEBUTTON, IDM_CONNECT, !connected);
    SendMessageW(g_hToolbar, TB_ENABLEBUTTON, IDM_DISCONNECT, connected);
}

/* Update window title with port name */
static void UpdateTitle(HWND hWnd)
{
    WCHAR title[128];
    if (Serial_IsOpen(&g_serial))
        wsprintfW(title, L"SerialEcho - %s", g_szPort);
    else
        lstrcpyW(title, L"SerialEcho");
    SetWindowTextW(hWnd, title);
}

/* Update status bar content and part widths */
static void UpdateStatusBar(void)
{
    if (!g_hStatusbar)
        return;

    int parts[3];
    RECT rc;
    GetClientRect(GetParent(g_hStatusbar), &rc);
    parts[0] = rc.right - 280;
    parts[1] = rc.right - 140;
    parts[2] = -1;
    SendMessageW(g_hStatusbar, SB_SETPARTS, 3, (LPARAM)parts);

    SendMessageW(g_hStatusbar, SB_SETTEXT, 0, (LPARAM)L"");

    if (Serial_IsOpen(&g_serial))
        SendMessageW(g_hStatusbar, SB_SETTEXT, 1, (LPARAM)g_szPort);
    else
        SendMessageW(g_hStatusbar, SB_SETTEXT, 1, (LPARAM)L"Disconnected");

    SendMessageW(g_hStatusbar, SB_SETTEXT, 2, (LPARAM)L"115200,8N1");
}

/* Port selection dialog procedure */
static INT_PTR CALLBACK PortSelectDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg) {
    case WM_INITDIALOG:
        {
            HWND hCombo = GetDlgItem(hDlg, IDC_PORT_COMBO);
            if (!Serial_EnumPorts(hCombo)) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            SetFocus(hCombo);
        }
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            {
                HWND hCombo = GetDlgItem(hDlg, IDC_PORT_COMBO);
                int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
                if (sel < 0) {
                    MessageBoxW(hDlg, L"Please select a port", L"Warning", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                /* Get port index from item data */
                int portIdx = (int)SendMessageW(hCombo, CB_GETITEMDATA, sel, 0);
                if (!Serial_GetPortName(portIdx, g_szSelectedPort, 32)) {
                    MessageBoxW(hDlg, L"Invalid port selection", L"Error", MB_OK | MB_ICONERROR);
                    return TRUE;
                }
                EndDialog(hDlg, IDOK);
            }
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* Show port selection dialog, returns TRUE if user selected a port */
static BOOL ShowPortSelectDialog(HWND hWnd)
{
    INT_PTR ret = DialogBoxW(GetModuleHandle(NULL),
                             MAKEINTRESOURCEW(IDD_PORT_SELECT), hWnd, PortSelectDlgProc);
    if (ret == IDOK) {
        lstrcpyW(g_szPort, g_szSelectedPort);
        return TRUE;
    }
    return FALSE;
}

/* Format and append data to log display */
void GUI_AppendLog(HWND hMainWnd, const BYTE *data, DWORD len, int dir)
{
    (void)hMainWnd;
    if (!g_hEdit || len == 0)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    /* Build prefix: "YYYY-MM-DD HH:MM:SS.mmm [RX] " */
    WCHAR prefix[64];
    int prefixLen = wsprintfW(prefix, L"%04d-%02d-%02d %02d:%02d:%02d.%03d [%s] ",
                              st.wYear, st.wMonth, st.wDay,
                              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                              (dir == DIR_RX) ? L"RX" : L"TX");

    /* Calculate max line size generously */
    /* Each byte: "XX " (3 chars), extra space every 8 bytes, CRLF every 16 bytes */
    DWORD numLines = (len + 15) / 16;
    DWORD maxLineSize = (prefixLen + 50) * (numLines + 1) + (len * 4) + 64;
    WCHAR *line = (WCHAR *)malloc(maxLineSize * sizeof(WCHAR));
    if (!line)
        return;

    int pos = 0;
    int maxPos = maxLineSize - 8; /* Reserve for final CRLF + null + safety margin */

    /* Write prefix first */
    if (prefixLen < maxPos) {
        CopyMemory(line + pos, prefix, prefixLen * sizeof(WCHAR));
        pos += prefixLen;
    }

    /* Format HEX data with grouping and line wrapping */
    for (DWORD i = 0; i < len && pos < maxPos; i++) {
        if (i > 0 && i % 16 == 0) {
            /* New line every 16 bytes, align with prefix */
            if (pos + 2 < maxPos) {
                line[pos++] = L'\r';
                line[pos++] = L'\n';
            }
            for (int j = 0; j < prefixLen && pos < maxPos; j++) {
                line[pos++] = L' ';
            }
        } else if (i > 0 && i % 8 == 0) {
            /* Extra space every 8 bytes for visual grouping */
            if (pos < maxPos)
                line[pos++] = L' ';
        }
        if (pos + 4 < maxPos)
            pos += wsprintfW(line + pos, L"%02X ", data[i]);
    }

    /* Add final newline */
    if (pos + 2 < maxLineSize) {
        line[pos++] = L'\r';
        line[pos++] = L'\n';
    }
    line[pos] = L'\0';

    /* Append to RichEdit control */
    int textLen = GetWindowTextLengthW(g_hEdit);
    SendMessageW(g_hEdit, EM_SETSEL, textLen, textLen);
    SendMessageW(g_hEdit, EM_REPLACESEL, FALSE, (LPARAM)line);
    SendMessageW(g_hEdit, EM_SCROLLCARET, 0, 0);

    free(line);
}

/* Handle Connect command */
void GUI_OnConnect(HWND hMainWnd)
{
    TRACE_LOG(TAG, "GUI_OnConnect called");

    if (Serial_IsOpen(&g_serial)) {
        TRACE_LOG(TAG, "Port already open");
        return;
    }

    if (!ShowPortSelectDialog(hMainWnd)) {
        TRACE_LOG(TAG, "Port selection cancelled");
        return;
    }

    TRACE_LOG(TAG, "Selected port: %s", g_szPort);
    TRACE_LOG(TAG, "Calling Serial_Open...");

    if (!Serial_Open(&g_serial, g_szPort, hMainWnd)) {
        TRACE_LOG(TAG, "ERROR: Serial_Open failed");
        MessageBoxW(hMainWnd, L"Failed to open serial port", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    TRACE_LOG(TAG, "Serial_Open succeeded");

    /* Clear log on new connection */
    SetWindowTextW(g_hEdit, L"");

    UpdateTitle(hMainWnd);
    UpdateMenuState(hMainWnd);
    UpdateStatusBar();

    TRACE_LOG(TAG, "GUI_OnConnect completed");
}

/* Handle Disconnect command */
void GUI_OnDisconnect(HWND hMainWnd)
{
    if (!Serial_IsOpen(&g_serial))
        return;

    Serial_Close(&g_serial);

    UpdateTitle(hMainWnd);
    UpdateMenuState(hMainWnd);
    UpdateStatusBar();
}

/* Handle Log > Clear command */
void GUI_OnLogClear(HWND hMainWnd)
{
    (void)hMainWnd;
    if (g_hEdit)
        SetWindowTextW(g_hEdit, L"");
}

/* Handle Log > Save As command - save log to UTF-8 file */
void GUI_OnLogSaveAs(HWND hMainWnd)
{
    OPENFILENAMEW ofn = {0};
    WCHAR szFile[MAX_PATH] = {0};

    /* Generate default filename with timestamp */
    SYSTEMTIME st;
    GetLocalTime(&st);
    wsprintfW(szFile, L"SerialEcho_%04d%02d%02d_%02d%02d%02d.log",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMainWnd;
    ofn.lpstrFilter = L"Log Files (*.log)\0*.log\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"log";

    if (!GetSaveFileNameW(&ofn))
        return;

    HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hMainWnd, L"Failed to create file", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    int textLen = GetWindowTextLengthW(g_hEdit);
    if (textLen > 0) {
        WCHAR *buf = (WCHAR *)malloc((textLen + 1) * sizeof(WCHAR));
        if (buf) {
            GetWindowTextW(g_hEdit, buf, textLen + 1);

            /* Convert to UTF-8 (no BOM) */
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buf, textLen + 1, NULL, 0, NULL, NULL);
            if (utf8Len > 0) {
                char *utf8Buf = (char *)malloc(utf8Len);
                if (utf8Buf) {
                    WideCharToMultiByte(CP_UTF8, 0, buf, textLen + 1, utf8Buf, utf8Len, NULL, NULL);
                    DWORD written;
                    /* Exclude null terminator from output */
                    WriteFile(hFile, utf8Buf, utf8Len - 1, &written, NULL);
                    free(utf8Buf);
                }
            }
            free(buf);
        }
    }

    CloseHandle(hFile);
}

/* Handle Exit command with confirmation if connected */
void GUI_OnExit(HWND hWnd)
{
    if (Serial_IsOpen(&g_serial)) {
        int ret = MessageBoxW(hWnd,
                              L"Serial port is connected. Are you sure you want to exit?",
                              L"Confirm Exit",
                              MB_YESNO | MB_ICONQUESTION);
        if (ret != IDYES)
            return;
    }
    DestroyWindow(hWnd);
}

/* About dialog procedure */
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* Main window procedure */
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        {
            HINSTANCE hInst = ((CREATESTRUCT *)lParam)->hInstance;

            /* Create toolbar */
            g_hToolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL,
                WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_TOP | TBSTYLE_TOOLTIPS,
                0, 0, 0, 0, hWnd, (HMENU)IDC_MAIN_TOOLBAR, hInst, NULL);

            SendMessageW(g_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
            SendMessageW(g_hToolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(16, 16));

            /* Load merged toolbar bitmap (4 icons: Connect, Disconnect, Clear, Save) */
            TBADDBITMAP tbab = {0};
            tbab.hInst = hInst;
            tbab.nID = IDB_TOOLBAR;
            int iBase = (int)SendMessageW(g_hToolbar, TB_ADDBITMAP, 4, (LPARAM)&tbab);

            /* Toolbar buttons: Connect, Disconnect, separator, Clear, Save */
            TBBUTTON buttons[5] = {0};

            buttons[0].iBitmap = iBase + 0;  /* Connect icon */
            buttons[0].idCommand = IDM_CONNECT;
            buttons[0].fsState = TBSTATE_ENABLED;
            buttons[0].fsStyle = BTNS_BUTTON;
            buttons[0].iString = -1;

            buttons[1].iBitmap = iBase + 1;  /* Disconnect icon */
            buttons[1].idCommand = IDM_DISCONNECT;
            buttons[1].fsState = 0;  /* Disabled initially */
            buttons[1].fsStyle = BTNS_BUTTON;
            buttons[1].iString = -1;

            buttons[2].iBitmap = 0;
            buttons[2].idCommand = 0;
            buttons[2].fsState = 0;
            buttons[2].fsStyle = BTNS_SEP;
            buttons[2].iString = -1;

            buttons[3].iBitmap = iBase + 2;  /* Clear icon */
            buttons[3].idCommand = IDM_LOG_CLEAR;
            buttons[3].fsState = TBSTATE_ENABLED;
            buttons[3].fsStyle = BTNS_BUTTON;
            buttons[3].iString = -1;

            buttons[4].iBitmap = iBase + 3;  /* Save icon */
            buttons[4].idCommand = IDM_LOG_SAVEAS;
            buttons[4].fsState = TBSTATE_ENABLED;
            buttons[4].fsStyle = BTNS_BUTTON;
            buttons[4].iString = -1;

            SendMessageW(g_hToolbar, TB_ADDBUTTONS, 5, (LPARAM)buttons);

            /* Create status bar */
            g_hStatusbar = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0, hWnd, (HMENU)IDC_MAIN_STATUSBAR, hInst, NULL);

            /* Create RichEdit log display */
            HMODULE hRichEdit = LoadLibraryW(L"riched20.dll");
            (void)hRichEdit;
            g_hEdit = CreateWindowExW(0, RICHEDIT_CLASSW, NULL,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                0, 0, 0, 0, hWnd, (HMENU)IDC_MAIN_EDIT, hInst, NULL);

            /* Configure RichEdit: unlimited text, light gray background, black Consolas font */
            SendMessageW(g_hEdit, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_hEdit, EM_SETBKGNDCOLOR, 0, RGB(240, 240, 240));

            CHARFORMAT2W cf = {0};
            cf.cbSize = sizeof(CHARFORMAT2W);
            cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
            cf.crTextColor = RGB(0, 0, 0);
            cf.dwEffects = 0;
            cf.yHeight = 180;
            lstrcpyW(cf.szFaceName, L"Consolas");
            SendMessageW(g_hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            UpdateTitle(hWnd);
            UpdateMenuState(hWnd);
            UpdateStatusBar();
        }
        return 0;

    case WM_SIZE:
        {
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            SendMessageW(g_hToolbar, WM_SIZE, 0, 0);
            SendMessageW(g_hStatusbar, WM_SIZE, 0, 0);

            RECT rcToolbar, rcStatus;
            GetWindowRect(g_hToolbar, &rcToolbar);
            GetWindowRect(g_hStatusbar, &rcStatus);

            int toolbarH = rcToolbar.bottom - rcToolbar.top;
            int statusH = rcStatus.bottom - rcStatus.top;

            SetWindowPos(g_hEdit, NULL, 0, toolbarH,
                        rcClient.right, rcClient.bottom - toolbarH - statusH,
                        SWP_NOZORDER);

            UpdateStatusBar();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_CONNECT:
            GUI_OnConnect(hWnd);
            SetFocus(g_hEdit);
            return 0;
        case IDM_DISCONNECT:
            GUI_OnDisconnect(hWnd);
            SetFocus(g_hEdit);
            return 0;
        case IDM_LOG_CLEAR:
            GUI_OnLogClear(hWnd);
            return 0;
        case IDM_LOG_SAVEAS:
            GUI_OnLogSaveAs(hWnd);
            SetFocus(g_hEdit);
            return 0;
        case IDM_EXIT:
            GUI_OnExit(hWnd);
            return 0;
        case IDM_ABOUT:
            DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_ABOUT), hWnd, AboutDlgProc);
            SetFocus(g_hEdit);
            return 0;
        }
        break;

    case WM_NOTIFY:
        /* Handle toolbar tooltip requests */
        if (((NMHDR *)lParam)->hwndFrom == g_hToolbar) {
            LPNMTTDISPINFOW ttt = (LPNMTTDISPINFOW)lParam;
            if (ttt->hdr.code == TTN_GETDISPINFOW) {
                switch (ttt->hdr.idFrom) {
                case IDM_CONNECT:
                    lstrcpyW(ttt->szText, L"Connect");
                    break;
                case IDM_DISCONNECT:
                    lstrcpyW(ttt->szText, L"Disconnect");
                    break;
                case IDM_LOG_CLEAR:
                    lstrcpyW(ttt->szText, L"Clear Log");
                    break;
                case IDM_LOG_SAVEAS:
                    lstrcpyW(ttt->szText, L"Save Log");
                    break;
                }
            }
        }
        break;

    case WM_USER + 1:
        /* RX Data received from serial port */
        {
            DWORD len = (DWORD)wParam;
            BYTE *data = (BYTE *)lParam;
            if (data != NULL && len > 0 && len < 65536) {
                GUI_AppendLog(hWnd, data, len, DIR_RX);
            }
            if (data != NULL) {
                HeapFree(GetProcessHeap(), 0, data);
            }
        }
        return 0;

    case WM_USER + 2:
        /* TX Data sent to serial port */
        {
            DWORD len = (DWORD)wParam;
            BYTE *data = (BYTE *)lParam;
            if (data != NULL && len > 0 && len < 65536) {
                GUI_AppendLog(hWnd, data, len, DIR_TX);
            }
            if (data != NULL) {
                HeapFree(GetProcessHeap(), 0, data);
            }
        }
        return 0;

    case WM_CLOSE:
        /* Handle close button (X) with confirmation if connected */
        if (Serial_IsOpen(&g_serial)) {
            int ret = MessageBoxW(hWnd,
                                  L"Serial port is connected. Are you sure you want to exit?",
                                  L"Confirm Exit",
                                  MB_YESNO | MB_ICONQUESTION);
            if (ret != IDYES)
                return 0;
        }
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        Serial_Close(&g_serial);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

/* Initialize GUI: register window class, init common controls */
BOOL GUI_Init(HINSTANCE hInstance)
{
    INITCOMMONCONTROLSEX icex = { .dwSize = sizeof(icex), .dwICC = ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = MainWndProc,
        .hInstance = hInstance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = L"SerialEchoClass",
        .hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP)),
    };
    RegisterClassExW(&wc);
    return TRUE;
}

/* Create and show the main application window */
HWND GUI_CreateMainWindow(HINSTANCE hInstance)
{
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);
    int width = 800;
    int height = 600;

    HMENU hMenu = LoadMenuW(hInstance, MAKEINTRESOURCEW(IDR_MAIN_MENU));

    HWND hWnd = CreateWindowExW(0, L"SerialEchoClass", L"SerialEcho",
        WS_OVERLAPPEDWINDOW,
        (cx - width) / 2, (cy - height) / 2, width, height,
        NULL, hMenu, hInstance, NULL);

    if (hWnd) {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
        /* Set focus to log control for auto-scroll */
        if (g_hEdit)
            SetFocus(g_hEdit);
    }

    return hWnd;
}
