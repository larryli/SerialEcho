/*
 * app_protocol.c - Protocol signal handling
 *
 * Handles signal change notifications and configuration logging.
 * Extracted from main.c for modularity.
 */

#include "app_protocol.h"
#include "app_logview.h"
#include "utils/trace.h"
#include <stdio.h>

/* ========================================================================
 * Signal handling
 * ======================================================================== */

void AppProtocol_OnSignal(DWORD modemStatus)
{
    WCHAR buf[96];
    wsprintfW(buf, L"DSR:%s CTS:%s RI:%s DCD:%s",
              (modemStatus & MS_DSR_ON) ? L"ON" : L"OFF",
              (modemStatus & MS_CTS_ON) ? L"ON" : L"OFF",
              (modemStatus & MS_RING_ON) ? L"ON" : L"OFF",
              (modemStatus & MS_RLSD_ON) ? L"ON" : L"OFF");
    Main_AppendSignalLog(L"SIG", buf, COLOR_SIGNAL);
}

void AppProtocol_OnDtrChange(BOOL state)
{
    WCHAR buf[32];
    wsprintfW(buf, L"DTR:%s", state ? L"ON" : L"OFF");
    Main_AppendSignalLog(L"SIG", buf, COLOR_SIGNAL);
}

void AppProtocol_OnRtsChange(BOOL state)
{
    WCHAR buf[32];
    wsprintfW(buf, L"RTS:%s", state ? L"ON" : L"OFF");
    Main_AppendSignalLog(L"SIG", buf, COLOR_SIGNAL);
}

/* ========================================================================
 * Configuration handling
 * ======================================================================== */

void AppProtocol_OnConfigChange(SERIAL_CTX *ctx)
{
    DWORD baudRate = 0;
    BYTE dataBits = 0, parity = 0, stopBits = 0;

    if (!Serial_GetConfig(ctx, &baudRate, &dataBits, &parity, &stopBits))
        return;

    const WCHAR *parityStr = L"N";
    switch (parity) {
    case NOPARITY: parityStr = L"N"; break;
    case ODDPARITY: parityStr = L"O"; break;
    case EVENPARITY: parityStr = L"E"; break;
    case MARKPARITY: parityStr = L"M"; break;
    case SPACEPARITY: parityStr = L"S"; break;
    }

    const WCHAR *stopStr = L"1";
    switch (stopBits) {
    case ONESTOPBIT: stopStr = L"1"; break;
    case ONE5STOPBITS: stopStr = L"1.5"; break;
    case TWOSTOPBITS: stopStr = L"2"; break;
    }

    WCHAR buf[64];
    wsprintfW(buf, L"%lu,%d%s%s", baudRate, dataBits, parityStr, stopStr);
    Main_AppendSignalLog(L"CFG", buf, COLOR_CONFIG);
}
