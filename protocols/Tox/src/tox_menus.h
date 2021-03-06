#ifndef _TOX_MENUS_H_
#define _TOX_MENUS_H_

#define CMI_POSITION -201001000

enum CMI_MENU_ITEMS
{
	CMI_AUTH_REQUEST,
	CMI_AUTH_GRANT,
	CMI_MAX // this item shall be the last one
};

#define SMI_POSITION 200000

enum SMI_MENU_ITEMS
{
	SMI_TOXID_COPY,
	SMI_PASSWORD,
	SMI_PASSWORD_CREATE,
	SMI_PASSWORD_CHANGE,
	SMI_PASSWORD_REMOVE,
	SMI_MAX // this item shall be the last one
};

#endif //_TOX_MENUS_H_