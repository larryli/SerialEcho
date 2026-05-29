/*
 * config.h - Configuration file interface
 *
 * Provides functions for reading/writing INI configuration file.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

/*
 * Config_Init - Initialize config module, load config file path
 */
void Config_Init(void);

/*
 * Config_GetFont - Get saved font settings
 * @lf: Pointer to LOGFONTW structure to receive settings
 * Returns: TRUE if settings were loaded
 */
BOOL Config_GetFont(LOGFONTW *lf);

/*
 * Config_SetFont - Save font settings
 * @lf: Pointer to LOGFONTW structure with settings
 */
void Config_SetFont(const LOGFONTW *lf);

/*
 * Config_GetLastPort - Get last connected port name
 * @portName: Buffer to receive port name
 * @maxLen: Buffer size in characters
 * Returns: TRUE if port name was loaded
 */
BOOL Config_GetLastPort(WCHAR *portName, int maxLen);

/*
 * Config_SetLastPort - Save last connected port name
 * @portName: Port name to save
 */
void Config_SetLastPort(const WCHAR *portName);

#endif /* CONFIG_H */
