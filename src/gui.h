/*
 * gui.h - GUI module interface
 *
 * Provides functions for creating the main window, managing toolbar,
 * status bar, and log display.
 */

#ifndef GUI_H
#define GUI_H

#include <windows.h>

/*
 * GUI_Init - Register window class and initialize common controls
 * @hInstance: Application instance handle
 * Returns: TRUE on success
 */
BOOL GUI_Init(HINSTANCE hInstance);

/*
 * GUI_CreateMainWindow - Create and show the main application window
 * @hInstance: Application instance handle
 * Returns: Window handle, or NULL on failure
 */
HWND GUI_CreateMainWindow(HINSTANCE hInstance);

/*
 * GUI_AppendLog - Append a data entry to the log display
 * @hMainWnd: Main window handle
 * @data: Pointer to data bytes
 * @len: Number of bytes
 * @dir: Data direction (DIR_RX or DIR_TX)
 */
void GUI_AppendLog(HWND hMainWnd, const BYTE *data, DWORD len, int dir);

/*
 * GUI_OnConnect - Handle connect command (show port dialog, open port)
 * @hMainWnd: Main window handle
 */
void GUI_OnConnect(HWND hMainWnd);

/*
 * GUI_OnDisconnect - Handle disconnect command
 * @hMainWnd: Main window handle
 */
void GUI_OnDisconnect(HWND hMainWnd);

/*
 * GUI_OnPing - Handle ping command (send random data)
 * @hMainWnd: Main window handle
 */
void GUI_OnPing(HWND hMainWnd);

/*
 * GUI_OnLogClear - Clear all log content
 * @hMainWnd: Main window handle
 */
void GUI_OnLogClear(HWND hMainWnd);

/*
 * GUI_OnLogSaveAs - Save log content to file
 * @hMainWnd: Main window handle
 */
void GUI_OnLogSaveAs(HWND hMainWnd);

/*
 * GUI_OnLogFont - Show font selection dialog
 * @hMainWnd: Main window handle
 */
void GUI_OnLogFont(HWND hMainWnd);

/*
 * GUI_OnExit - Handle exit command with confirmation if connected
 * @hMainWnd: Main window handle
 */
void GUI_OnExit(HWND hMainWnd);

#endif /* GUI_H */
