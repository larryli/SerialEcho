/*
 * gui.c - GUI module implementation
 *
 * Implements the main window with toolbar, status bar, and RichEdit
 * log display. Handles menu commands and serial port events.
 */

#include "gui.h"
#include "serial.h"
#include "protocol.h"
#include "resource.h"
#include "config.h"
#include "lang.h"
#include "trace.h"
#include <richedit.h>
#include <commdlg.h>
#include <dbt.h>
#include <devguid.h>
#include <commctrl.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")

static const char *TAG = "GUI";

/* Default font settings */
#define DEFAULT_FONT_NAME L"Consolas"
#define DEFAULT_FONT_SIZE 10

/* Global state */
static SERIAL_CTX g_serial = { .hPort = NULL, .hThread = NULL, .hStartEvent = NULL, .hIOEvent = NULL, .hNotify = NULL, .bRunning = FALSE };
static HWND g_hToolbar = NULL;
static HWND g_hStatusbar = NULL;
static HWND g_hEdit = NULL;
static WCHAR g_szPort[32] = {0};
static WCHAR g_szSelectedPort[32] = {0};
static LOGFONTW g_logFont = {0};  /* Current font */

/* Update menu and toolbar button states based on connection status */
static void UpdateMenuState(HWND hWnd)
{
    HMENU hMenu = GetMenu(hWnd);
    BOOL connected = Serial_IsOpen(&g_serial);

    EnableMenuItem(hMenu, IDM_CONNECT, connected ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(hMenu, IDM_DISCONNECT, connected ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(hMenu, IDM_PING, connected ? MF_ENABLED : MF_GRAYED);

    SendMessageW(g_hToolbar, TB_ENABLEBUTTON, IDM_CONNECT, !connected);
    SendMessageW(g_hToolbar, TB_ENABLEBUTTON, IDM_DISCONNECT, connected);
    SendMessageW(g_hToolbar, TB_ENABLEBUTTON, IDM_PING, connected);
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
        SendMessageW(g_hStatusbar, SB_SETTEXT, 1, (LPARAM)LoadStr(IDS_DISCONNECTED));

    SendMessageW(g_hStatusbar, SB_SETTEXT, 2, (LPARAM)LoadStr(IDS_PORT_CONFIG));
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

            /* Try to select last connected port */
            WCHAR lastPort[32] = {0};
            if (Config_GetLastPort(lastPort, 32)) {
                int count = (int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0);
                for (int i = 0; i < count; i++) {
                    int portIdx = (int)SendMessageW(hCombo, CB_GETITEMDATA, i, 0);
                    WCHAR portName[32] = {0};
                    if (Serial_GetPortName(portIdx, portName, 32)) {
                        if (lstrcmpiW(portName, lastPort) == 0) {
                            SendMessageW(hCombo, CB_SETCURSEL, i, 0);
                            break;
                        }
                    }
                }
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
                    MessageBoxW(hDlg, LoadStr(IDS_MSG_SELECT_PORT), LoadStr(IDS_MSG_WARNING), MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                /* Get port index from item data */
                int portIdx = (int)SendMessageW(hCombo, CB_GETITEMDATA, sel, 0);
                if (!Serial_GetPortName(portIdx, g_szSelectedPort, 32)) {
                    MessageBoxW(hDlg, LoadStr(IDS_MSG_INVALID_PORT), LoadStr(IDS_MSG_ERROR), MB_OK | MB_ICONERROR);
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

/* Color definitions for log display */
#define COLOR_TIMESTAMP RGB(128, 128, 128)  /* Gray */
#define COLOR_RX        RGB(0, 0, 200)      /* Blue */
#define COLOR_TX        RGB(0, 128, 0)      /* Green */
#define COLOR_DATA      RGB(0, 0, 0)        /* Black */
#define COLOR_BG        RGB(240, 240, 240)  /* Light gray */

/* Helper: Set text color for selection */
static void SetEditColor(HWND hEdit, COLORREF color)
{
    CHARFORMAT2W cf = {0};
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

/* Helper: Append colored text to RichEdit */
static void AppendColoredText(HWND hEdit, const WCHAR *text, int len, COLORREF color)
{
    int textLen = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, textLen, textLen);
    SetEditColor(hEdit, color);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
}

/* Format and append data to log display with colors */
void GUI_AppendLog(HWND hMainWnd, const BYTE *data, DWORD len, int dir)
{
    (void)hMainWnd;
    if (!g_hEdit || len == 0)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    /* Build timestamp: "YYYY-MM-DD HH:MM:SS.mmm " */
    WCHAR timestamp[32];
    int tsLen = wsprintfW(timestamp, L"%04d-%02d-%02d %02d:%02d:%02d.%03d ",
                          st.wYear, st.wMonth, st.wDay,
                          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    /* Build direction: "[RX] " or "[TX] " */
    WCHAR direction[8];
    int dirLen = wsprintfW(direction, L"[%s] ", (dir == DIR_RX) ? L"RX" : L"TX");
    COLORREF dirColor = (dir == DIR_RX) ? COLOR_RX : COLOR_TX;

    /* Calculate max line size for HEX data */
    DWORD numLines = (len + 15) / 16;
    DWORD maxLineSize = (tsLen + dirLen + 50) * (numLines + 1) + (len * 4) + 64;
    WCHAR *hexLine = (WCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, maxLineSize * sizeof(WCHAR));
    if (!hexLine)
        return;

    int pos = 0;
    int prefixLen = tsLen + dirLen;

    /* Format HEX data with grouping and line wrapping */
    for (DWORD i = 0; i < len; i++) {
        if (i > 0 && i % 16 == 0) {
            /* New line, align with prefix */
            hexLine[pos++] = L'\r';
            hexLine[pos++] = L'\n';
            for (int j = 0; j < prefixLen; j++)
                hexLine[pos++] = L' ';
        } else if (i > 0 && i % 8 == 0) {
            /* Extra space every 8 bytes */
            hexLine[pos++] = L' ';
        }
        pos += wsprintfW(hexLine + pos, L"%02X ", data[i]);
    }
    hexLine[pos++] = L'\r';
    hexLine[pos++] = L'\n';
    hexLine[pos] = L'\0';

    /* Append with colors: timestamp (gray) + direction (color) + data (black) */
    AppendColoredText(g_hEdit, timestamp, tsLen, COLOR_TIMESTAMP);
    AppendColoredText(g_hEdit, direction, dirLen, dirColor);
    AppendColoredText(g_hEdit, hexLine, pos, COLOR_DATA);

    /* Scroll to end */
    int textLen = GetWindowTextLengthW(g_hEdit);
    SendMessageW(g_hEdit, EM_SETSEL, textLen, textLen);
    SendMessageW(g_hEdit, EM_SCROLLCARET, 0, 0);

    HeapFree(GetProcessHeap(), 0, hexLine);
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
        MessageBoxW(hMainWnd, LoadStr(IDS_MSG_PORT_ERROR), LoadStr(IDS_MSG_ERROR), MB_OK | MB_ICONERROR);
        return;
    }

    /* Register protocol callback */
    Serial_SetReceiveCallback(&g_serial, (SERIAL_RX_CB)Protocol_ProcessData);

    TRACE_LOG(TAG, "Serial_Open succeeded");

    /* Save last connected port */
    Config_SetLastPort(g_szPort);

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

/* Handle Serial > Ping command - send random data */
void GUI_OnPing(HWND hMainWnd)
{
    if (!Serial_IsOpen(&g_serial)) {
        MessageBoxW(hMainWnd, LoadStr(IDS_MSG_NOT_CONN), LoadStr(IDS_MSG_CONN_TITLE), MB_OK | MB_ICONWARNING);
        return;
    }

    Protocol_SendPing(&g_serial, hMainWnd);
}

/* Handle Log > Clear command */
void GUI_OnLogClear(HWND hMainWnd)
{
    (void)hMainWnd;
    if (g_hEdit)
        SetWindowTextW(g_hEdit, L"");
}

/* Apply font to RichEdit control */
static void ApplyFontToEdit(HWND hEdit, LOGFONTW *plf)
{
    CHARFORMAT2W cf = {0};
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_WEIGHT;
    cf.yHeight = plf->lfHeight * 15;  /* Convert to twips (1/1440 inch) */
    cf.wWeight = (WORD)plf->lfWeight;
    lstrcpyW(cf.szFaceName, plf->lfFaceName);
    SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

/* Initialize default font (try to load from config) */
static void InitDefaultFont(void)
{
    /* Try to load from config first */
    if (!Config_GetFont(&g_logFont)) {
        /* Use default if not in config */
        HDC hdc = GetDC(NULL);
        g_logFont.lfHeight = -MulDiv(DEFAULT_FONT_SIZE, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(NULL, hdc);
        g_logFont.lfWeight = FW_NORMAL;
        g_logFont.lfCharSet = DEFAULT_CHARSET;
        g_logFont.lfOutPrecision = OUT_TT_PRECIS;
        g_logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        g_logFont.lfQuality = CLEARTYPE_QUALITY;
        g_logFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        lstrcpyW(g_logFont.lfFaceName, DEFAULT_FONT_NAME);
    }
}

/* Handle Log > Font command - show font selection dialog */
void GUI_OnLogFont(HWND hMainWnd)
{
    CHOOSEFONTW cf = {0};
    LOGFONTW lf = g_logFont;

    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = hMainWnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT | CF_NOVERTFONTS;
    cf.nFontType = SCREEN_FONTTYPE;

    if (ChooseFontW(&cf)) {
        g_logFont = lf;
        ApplyFontToEdit(g_hEdit, &g_logFont);
        Config_SetFont(&g_logFont);  /* Save to config */
    }
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
    ofn.lpstrFilter = LoadStr(IDS_LOG_SAVE_FILTER);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"log";

    if (!GetSaveFileNameW(&ofn))
        return;

    HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hMainWnd, LoadStr(IDS_MSG_SAVE_ERROR), LoadStr(IDS_MSG_ERROR), MB_OK | MB_ICONERROR);
        return;
    }

    int textLen = GetWindowTextLengthW(g_hEdit);
    if (textLen > 0) {
        WCHAR *buf = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (textLen + 1) * sizeof(WCHAR));
        if (buf) {
            GetWindowTextW(g_hEdit, buf, textLen + 1);

            /* Convert to UTF-8 (no BOM) */
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buf, textLen + 1, NULL, 0, NULL, NULL);
            if (utf8Len > 0) {
                char *utf8Buf = (char *)HeapAlloc(GetProcessHeap(), 0, utf8Len);
                if (utf8Buf) {
                    WideCharToMultiByte(CP_UTF8, 0, buf, textLen + 1, utf8Buf, utf8Len, NULL, NULL);
                    DWORD written;
                    /* Exclude null terminator from output */
                    WriteFile(hFile, utf8Buf, utf8Len - 1, &written, NULL);
                    HeapFree(GetProcessHeap(), 0, utf8Buf);
                }
            }
            HeapFree(GetProcessHeap(), 0, buf);
        }
    }

    CloseHandle(hFile);
}

/* Handle Exit command with confirmation if connected */
void GUI_OnExit(HWND hWnd)
{
    if (Serial_IsOpen(&g_serial)) {
        int ret = MessageBoxW(hWnd,
                              LoadStr(IDS_MSG_CONFIRM_EXIT),
                              LoadStr(IDS_MSG_CONFIRM_CAP),
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

            /* Load merged toolbar bitmap (5 icons: Connect, Disconnect, Clear, Save, Ping) */
            TBADDBITMAP tbab = {0};
            tbab.hInst = hInst;
            tbab.nID = IDB_TOOLBAR;
            int iBase = (int)SendMessageW(g_hToolbar, TB_ADDBITMAP, 5, (LPARAM)&tbab);

            /* Toolbar buttons: Connect, Disconnect, separator, Ping, separator, Clear, Save */
            TBBUTTON buttons[7] = {0};

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
            buttons[2].idCommand = -1;
            buttons[2].fsState = 0;
            buttons[2].fsStyle = BTNS_SEP;
            buttons[2].iString = -1;

            buttons[3].iBitmap = iBase + 4;  /* Ping icon */
            buttons[3].idCommand = IDM_PING;
            buttons[3].fsState = 0;  /* Disabled initially */
            buttons[3].fsStyle = BTNS_BUTTON;
            buttons[3].iString = -1;

            buttons[4].iBitmap = 0;
            buttons[4].idCommand = -1;
            buttons[4].fsState = 0;
            buttons[4].fsStyle = BTNS_SEP;
            buttons[4].iString = -1;

            buttons[5].iBitmap = iBase + 2;  /* Clear icon */
            buttons[5].idCommand = IDM_LOG_CLEAR;
            buttons[5].fsState = TBSTATE_ENABLED;
            buttons[5].fsStyle = BTNS_BUTTON;
            buttons[5].iString = -1;

            buttons[6].iBitmap = iBase + 3;  /* Save icon */
            buttons[6].idCommand = IDM_LOG_SAVEAS;
            buttons[6].fsState = TBSTATE_ENABLED;
            buttons[6].fsStyle = BTNS_BUTTON;
            buttons[6].iString = -1;

            SendMessageW(g_hToolbar, TB_ADDBUTTONS, 7, (LPARAM)buttons);

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

            /* Configure RichEdit: unlimited text, light gray background */
            SendMessageW(g_hEdit, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_hEdit, EM_SETBKGNDCOLOR, 0, COLOR_BG);

            /* Initialize and apply default font */
            InitDefaultFont();
            ApplyFontToEdit(g_hEdit, &g_logFont);

            /* Register for device change notifications */
            DEV_BROADCAST_DEVICEINTERFACE dbi = {0};
            dbi.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
            dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            dbi.dbcc_classguid = GUID_DEVCLASS_PORTS;
            RegisterDeviceNotificationW(hWnd, &dbi, DEVICE_NOTIFY_WINDOW_HANDLE);

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
        case IDM_PING:
            GUI_OnPing(hWnd);
            SetFocus(g_hEdit);
            return 0;
        case IDM_LOG_CLEAR:
            GUI_OnLogClear(hWnd);
            return 0;
        case IDM_LOG_SAVEAS:
            GUI_OnLogSaveAs(hWnd);
            SetFocus(g_hEdit);
            return 0;
        case IDM_LOG_FONT:
            GUI_OnLogFont(hWnd);
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
        if (((NMHDR *)lParam)->code == TTN_GETDISPINFOW) {
            NMTTDISPINFOW *ttt = (NMTTDISPINFOW *)lParam;
            ttt->hinst = GetModuleHandleW(NULL);
            switch (ttt->hdr.idFrom) {
            case IDM_CONNECT:
                ttt->lpszText = MAKEINTRESOURCEW(IDS_TIP_CONNECT);
                return 0;
            case IDM_DISCONNECT:
                ttt->lpszText = MAKEINTRESOURCEW(IDS_TIP_DISCONNECT);
                return 0;
            case IDM_PING:
                ttt->lpszText = MAKEINTRESOURCEW(IDS_TIP_PING);
                return 0;
            case IDM_LOG_CLEAR:
                ttt->lpszText = MAKEINTRESOURCEW(IDS_TIP_CLEAR);
                return 0;
            case IDM_LOG_SAVEAS:
                ttt->lpszText = MAKEINTRESOURCEW(IDS_TIP_SAVEAS);
                return 0;
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

    case WM_USER + 3:
        /* Connection lost notification from listener thread */
        TRACE_LOG(TAG, "Connection lost, error code: %lu", (DWORD)wParam);
        Serial_Close(&g_serial);
        UpdateTitle(hWnd);
        UpdateMenuState(hWnd);
        UpdateStatusBar();
        MessageBoxW(hWnd, LoadStr(IDS_MSG_CONN_LOST), LoadStr(IDS_MSG_ERROR), MB_OK | MB_ICONERROR);
        return 0;

    case WM_DEVICECHANGE:
        /* Handle device removal */
        if (wParam == DBT_DEVICEREMOVECOMPLETE && Serial_IsOpen(&g_serial)) {
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
            if (pHdr && pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                TRACE_LOG(TAG, "Device removed, disconnecting");
                Serial_Close(&g_serial);
                UpdateTitle(hWnd);
                UpdateMenuState(hWnd);
                UpdateStatusBar();
                MessageBoxW(hWnd, LoadStr(IDS_MSG_DEV_REMOVED),
                            LoadStr(IDS_MSG_DEV_TITLE), MB_OK | MB_ICONWARNING);
            }
        }
        break;

    case WM_CLOSE:
        /* Handle close button (X) with confirmation if connected */
        if (Serial_IsOpen(&g_serial)) {
            int ret = MessageBoxW(hWnd,
                                  LoadStr(IDS_MSG_CONFIRM_EXIT),
                                  LoadStr(IDS_MSG_CONFIRM_CAP),
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

    /* Initialize protocol module */
    Protocol_Init();

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
