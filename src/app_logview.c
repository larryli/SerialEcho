/*
 * app_logview.c - Log display functions
 *
 * Provides buffered, batch-updated colored log display in RichEdit control.
 * Extracted from main.c for modularity and reusability.
 */

#include "app_logview.h"
#include "serial.h"
#include <richedit.h>
#include <stdio.h>

/* Global edit control handle (defined in main.c) */
extern HWND g_hEdit;

/* Log buffer state */
static LOG_ENTRY *g_logHead = NULL;
static LOG_ENTRY *g_logTail = NULL;
static int g_logCount = 0;
static CRITICAL_SECTION g_logLock;
static BOOL g_logInitialized = FALSE;

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

static void SetEditColor(HWND hEdit, COLORREF color)
{
    CHARFORMAT2W cf = {0};
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

static void AppendColoredText(HWND hEdit, const WCHAR *text, int len,
                              COLORREF color)
{
    int textLen = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, textLen, textLen);
    SetEditColor(hEdit, color);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
}

static int FormatTimestamp(WCHAR *buf, int maxLen)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    return wsprintfW(buf, L"%04d-%02d-%02d %02d:%02d:%02d.%03d ", st.wYear,
                     st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                     st.wMilliseconds);
}

/* ========================================================================
 * Log buffer management (thread-safe)
 * ======================================================================== */

static void AddEntry(LOG_ENTRY *entry)
{
    EnterCriticalSection(&g_logLock);

    entry->next = NULL;
    if (g_logTail) {
        g_logTail->next = entry;
    } else {
        g_logHead = entry;
    }
    g_logTail = entry;
    g_logCount++;

    LeaveCriticalSection(&g_logLock);
}

static LOG_ENTRY *RemoveAllEntries(void)
{
    EnterCriticalSection(&g_logLock);

    LOG_ENTRY *head = g_logHead;
    g_logHead = NULL;
    g_logTail = NULL;
    g_logCount = 0;

    LeaveCriticalSection(&g_logLock);
    return head;
}

static void FreeEntryChain(LOG_ENTRY *head)
{
    while (head) {
        LOG_ENTRY *next = head->next;
        if (head->text)
            HeapFree(GetProcessHeap(), 0, head->text);
        HeapFree(GetProcessHeap(), 0, head);
        head = next;
    }
}

/* ========================================================================
 * Public API
 * ======================================================================== */

void LogView_Init(HWND hWnd)
{
    if (g_logInitialized)
        return;

    InitializeCriticalSection(&g_logLock);
    g_logHead = NULL;
    g_logTail = NULL;
    g_logCount = 0;
    g_logInitialized = TRUE;

    SetTimer(hWnd, LOG_FLUSH_TIMER_ID, LOG_FLUSH_INTERVAL_MS, NULL);
}

void LogView_Close(void)
{
    if (!g_logInitialized)
        return;

    if (g_hEdit)
        KillTimer(GetParent(g_hEdit), LOG_FLUSH_TIMER_ID);

    LogView_Flush();

    DeleteCriticalSection(&g_logLock);
    g_logInitialized = FALSE;
}

void LogView_FlushTimer(void)
{
    if (!g_hEdit || g_logCount == 0)
        return;

    LOG_ENTRY *head = RemoveAllEntries();
    if (!head)
        return;

    SendMessageW(g_hEdit, WM_SETREDRAW, FALSE, 0);

    LOG_ENTRY *entry = head;
    while (entry) {
        if (entry->text && entry->textLen > 0) {
            AppendColoredText(g_hEdit, entry->text, entry->textLen,
                              entry->color1);
        }
        entry = entry->next;
    }

    SendMessageW(g_hEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hEdit, NULL, TRUE);

    int textLen = GetWindowTextLengthW(g_hEdit);
    SendMessageW(g_hEdit, EM_SETSEL, textLen, textLen);
    SendMessageW(g_hEdit, EM_SCROLLCARET, 0, 0);

    FreeEntryChain(head);
}

void LogView_Flush(void) { LogView_FlushTimer(); }

void Main_AppendLog(HWND hMainWnd, const BYTE *data, DWORD len, int dir)
{
    (void)hMainWnd;
    if (!g_logInitialized || len == 0)
        return;

    DWORD numLines = (len + 15) / 16;
    DWORD maxLineSize = 64 + 16 + (len * 4) + (numLines * 64) + 64;
    WCHAR *buf = (WCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                    maxLineSize * sizeof(WCHAR));
    if (!buf)
        return;

    int pos = 0;
    pos += FormatTimestamp(buf + pos, maxLineSize - pos);
    pos += wsprintfW(buf + pos, L"[%s] ", (dir == DIR_RX) ? L"RX" : L"TX");

    int prefixLen = pos;
    for (DWORD i = 0; i < len; i++) {
        if (i > 0 && i % 16 == 0) {
            buf[pos++] = L'\r';
            buf[pos++] = L'\n';
            for (int j = 0; j < prefixLen; j++)
                buf[pos++] = L' ';
        } else if (i > 0 && i % 8 == 0) {
            buf[pos++] = L' ';
        }
        pos += wsprintfW(buf + pos, L"%02X ", data[i]);
    }
    buf[pos++] = L'\r';
    buf[pos++] = L'\n';
    buf[pos] = L'\0';

    LOG_ENTRY *entry =
        (LOG_ENTRY *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                               sizeof(LOG_ENTRY));
    if (entry) {
        entry->type = LOG_TYPE_DATA;
        entry->color1 = (dir == DIR_RX) ? COLOR_RX : COLOR_TX;
        entry->text = buf;
        entry->textLen = pos;
        AddEntry(entry);
    } else {
        HeapFree(GetProcessHeap(), 0, buf);
    }
}

void Main_AppendCustomLog(HWND hMainWnd, const WCHAR *tag, const WCHAR *text)
{
    (void)hMainWnd;
    if (!g_logInitialized || !tag || !text)
        return;

    int textLen = lstrlenW(text);
    int maxLen = 64 + 64 + textLen + 4;
    WCHAR *buf = (WCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                    maxLen * sizeof(WCHAR));
    if (!buf)
        return;

    int pos = 0;
    pos += FormatTimestamp(buf + pos, maxLen - pos);
    pos += wsprintfW(buf + pos, L"[%s] ", tag);
    CopyMemory(buf + pos, text, textLen * sizeof(WCHAR));
    pos += textLen;
    buf[pos++] = L'\r';
    buf[pos++] = L'\n';
    buf[pos] = L'\0';

    LOG_ENTRY *entry =
        (LOG_ENTRY *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                               sizeof(LOG_ENTRY));
    if (entry) {
        entry->type = LOG_TYPE_CUSTOM;
        entry->color1 = COLOR_CUSTOM;
        entry->text = buf;
        entry->textLen = pos;
        AddEntry(entry);
    } else {
        HeapFree(GetProcessHeap(), 0, buf);
    }
}

void Main_AppendSignalLog(const WCHAR *tag, const WCHAR *text,
                          COLORREF tagColor)
{
    if (!g_logInitialized || !tag || !text)
        return;

    int textLen = lstrlenW(text);
    int maxLen = 64 + 64 + textLen + 4;
    WCHAR *buf = (WCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                    maxLen * sizeof(WCHAR));
    if (!buf)
        return;

    int pos = 0;
    pos += FormatTimestamp(buf + pos, maxLen - pos);
    pos += wsprintfW(buf + pos, L"[%s] ", tag);
    CopyMemory(buf + pos, text, textLen * sizeof(WCHAR));
    pos += textLen;
    buf[pos++] = L'\r';
    buf[pos++] = L'\n';
    buf[pos] = L'\0';

    LOG_ENTRY *entry =
        (LOG_ENTRY *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                               sizeof(LOG_ENTRY));
    if (entry) {
        entry->type = LOG_TYPE_SIGNAL;
        entry->color1 = tagColor;
        entry->text = buf;
        entry->textLen = pos;
        AddEntry(entry);
    } else {
        HeapFree(GetProcessHeap(), 0, buf);
    }
}
