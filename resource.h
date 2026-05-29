/*
 * resource.h - Resource ID definitions
 *
 * Defines control IDs, command IDs, dialog IDs, string IDs,
 * and icon/menu resource IDs for the SerialEcho application.
 */

#ifndef RESOURCE_H
#define RESOURCE_H

/* Main window control IDs */
#define IDC_MAIN_EDIT       1001    /* RichEdit log display */
#define IDC_MAIN_TOOLBAR    1002    /* Toolbar */
#define IDC_MAIN_STATUSBAR  1003    /* Status bar */

/* Menu command IDs */
#define IDM_CONNECT         2001    /* Serial > Connect */
#define IDM_DISCONNECT      2002    /* Serial > Disconnect */
#define IDM_LOG_CLEAR       2003    /* Log > Clear */
#define IDM_LOG_SAVEAS      2004    /* Log > Save as... */
#define IDM_EXIT            2005    /* File > Exit */
#define IDM_ABOUT           2006    /* Help > About */

/* Dialog IDs */
#define IDD_PORT_SELECT     3001    /* Port selection dialog */
#define IDC_PORT_COMBO      3002    /* Port combo box */
#define IDD_ABOUT           3003    /* About dialog */

/* Icon and menu resource IDs */
#define IDI_APP             5001    /* Application icon */
#define IDR_MAIN_MENU       6001    /* Main menu */

/* Toolbar bitmap resources */
#define IDB_TOOLBAR         7001

#endif /* RESOURCE_H */
