/*
 * resource.h - Resource ID definitions
 */

#ifndef RESOURCE_H
#define RESOURCE_H

/* Main window control IDs */
#define IDC_MAIN_EDIT       1001
#define IDC_MAIN_TOOLBAR    1002
#define IDC_MAIN_STATUSBAR  1003

/* Menu command IDs */
#define IDM_CONNECT         2001
#define IDM_DISCONNECT      2002
#define IDM_PING            2003
#define IDM_LOG_CLEAR       2004
#define IDM_LOG_SAVEAS      2005
#define IDM_LOG_FONT        2006
#define IDM_EXIT            2007
#define IDM_ABOUT           2008

/* Dialog IDs */
#define IDD_PORT_SELECT     3001
#define IDC_PORT_COMBO      3002
#define IDD_ABOUT           3003

/* Icon and menu resource IDs */
#define IDI_APP             5001
#define IDR_MAIN_MENU       6001

/* Toolbar bitmap resources */
#define IDB_TOOLBAR         7001

/* String IDs */
#define IDS_APP_NAME        10001
#define IDS_DISCONNECTED    10002
#define IDS_TITLE_FORMAT    10004  /* "SerialEcho - %s" */

/* Menu strings */
#define IDS_MENU_FILE       10010
#define IDS_MENU_EXIT       10011
#define IDS_MENU_SERIAL     10012
#define IDS_MENU_CONNECT    10013
#define IDS_MENU_DISCONNECT 10014
#define IDS_MENU_PING       10015
#define IDS_MENU_LOG        10016
#define IDS_MENU_CLEAR      10017
#define IDS_MENU_SAVEAS     10018
#define IDS_MENU_FONT       10019
#define IDS_MENU_HELP       10020
#define IDS_MENU_ABOUT      10021

/* Tooltip strings */
#define IDS_TIP_CONNECT     10030
#define IDS_TIP_DISCONNECT  10031
#define IDS_TIP_PING        10032
#define IDS_TIP_CLEAR       10033
#define IDS_TIP_SAVEAS      10034

/* Dialog strings */
#define IDS_DLG_SELECT_PORT 10040
#define IDS_DLG_PORT        10041
#define IDS_DLG_BAUD_RATE   10042
#define IDS_DLG_DATA_PARITY 10043
#define IDS_DLG_OK          10044
#define IDS_DLG_CANCEL      10045
#define IDS_DLG_ABOUT_TITLE 10046
#define IDS_DLG_ABOUT_NAME  10047
#define IDS_DLG_ABOUT_DESC  10048
#define IDS_DLG_ABOUT_COM0  10049
#define IDS_DLG_ABOUT_COPY  10050

/* Message strings */
#define IDS_MSG_NOT_CONN    10060
#define IDS_MSG_CONN_TITLE  10061
#define IDS_MSG_CONFIRM_EXIT 10062
#define IDS_MSG_CONFIRM_CAP 10063
#define IDS_MSG_PORT_ERROR  10064
#define IDS_MSG_ERROR       10065
#define IDS_MSG_SELECT_PORT 10066
#define IDS_MSG_WARNING     10067
#define IDS_MSG_INVALID_PORT 10068
#define IDS_MSG_SAVE_ERROR  10069
#define IDS_MSG_DEV_REMOVED 10070
#define IDS_MSG_DEV_TITLE   10071
#define IDS_LOG_SAVE_FILTER 10072
#define IDS_FONT_TITLE      10073
#define IDS_MSG_CONN_LOST   10074

#endif /* RESOURCE_H */
