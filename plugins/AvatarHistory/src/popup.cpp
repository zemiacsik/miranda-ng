/*
Copyright (C) 2005 Ricardo Pescuma Domenecci

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*/

#include "stdafx.h"

// Prototypes /////////////////////////////////////////////////////////////////////////////////////

#define WMU_ACTION	(WM_USER + 1)


LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND hPopupWindow = nullptr;


static LRESULT CALLBACK PopupDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK DumbPopupDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);



// Functions //////////////////////////////////////////////////////////////////////////////////////


// Initializations needed by popups
void InitPopups()
{
	// window needed for popup commands
	hPopupWindow = CreateWindowEx(WS_EX_TOOLWINDOW, L"static", _A2W(MODULE_NAME) L"_PopupWindow",
		0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_DESKTOP,
		nullptr, hInst, nullptr);
	SetWindowLongPtr(hPopupWindow, GWLP_WNDPROC, (LONG_PTR)PopupWndProc);
}


// Deinitializations needed by popups
void DeInitPopups()
{}


// Show an error popup
void ShowErrPopup(const wchar_t *description, const wchar_t *title)
{
	ShowPopupEx(NULL, title == nullptr ? _A2W(MODULE_NAME) L" Error" : title, description,
		nullptr, POPUP_TYPE_ERROR, nullptr);
}


void ShowTestPopup(MCONTACT hContact, const wchar_t *title, const wchar_t *description, const Options *op)
{
	ShowPopupEx(hContact, title, description, nullptr, POPUP_TYPE_TEST, op);
}


void ShowPopup(MCONTACT hContact, const wchar_t *title, const wchar_t *description)
{
	ShowPopupEx(hContact, title, description, (void*)hContact, POPUP_TYPE_NORMAL, &opts);
}

void ShowDebugPopup(MCONTACT hContact, const wchar_t *title, const wchar_t *description)
{
	if (db_get_b(NULL, MODULE_NAME, "Debug", 0)) {
		ShowPopup(hContact, title, description);
	}
}

typedef struct
{
	void* plugin_data;
	HICON hIcon;
}
PopupDataType;

// Show an popup
void ShowPopupEx(MCONTACT hContact, const wchar_t *title, const wchar_t *description,
	void *plugin_data, int type, const Options *op)
{
	if (ServiceExists(MS_POPUP_ADDPOPUPT)) {
		// Make popup
		POPUPDATAT ppd = { 0 };

		ppd.lchContact = hContact;
		ppd.lchIcon = createProtoOverlayedIcon(hContact);

		ppd.PluginData = mir_alloc(sizeof(PopupDataType));
		((PopupDataType*)ppd.PluginData)->plugin_data = plugin_data;
		((PopupDataType*)ppd.PluginData)->hIcon = ppd.lchIcon;

		if (title != nullptr)
			mir_wstrncpy(ppd.lptzContactName, title, _countof(ppd.lptzContactName));
		else if (hContact != NULL)
			mir_wstrncpy(ppd.lptzContactName, (wchar_t *)pcli->pfnGetContactDisplayName(hContact, 0),
				_countof(ppd.lptzContactName));

		if (description != nullptr)
			mir_wstrncpy(ppd.lptzText, description, _countof(ppd.lptzText));

		if (type == POPUP_TYPE_NORMAL || type == POPUP_TYPE_TEST) {
			if (op->popup_use_default_colors) {
				ppd.colorBack = 0;
				ppd.colorText = 0;
			}
			else if (op->popup_use_win_colors) {
				ppd.colorBack = GetSysColor(COLOR_BTNFACE);
				ppd.colorText = GetSysColor(COLOR_WINDOWTEXT);
			}
			else {
				ppd.colorBack = op->popup_bkg_color;
				ppd.colorText = op->popup_text_color;
			}
		}
		else // if (type == POPUP_TYPE_ERROR)
		{
			ppd.colorBack = RGB(200, 0, 0);
			ppd.colorText = RGB(255, 255, 255);
		}

		if (type == POPUP_TYPE_NORMAL) {
			ppd.PluginWindowProc = PopupDlgProc;
		}
		else // if (type == POPUP_TYPE_TEST || type == POPUP_TYPE_ERROR)
		{
			ppd.PluginWindowProc = DumbPopupDlgProc;
		}

		if (type == POPUP_TYPE_NORMAL || type == POPUP_TYPE_TEST) {
			switch (op->popup_delay_type) {
			case POPUP_DELAY_CUSTOM:
				ppd.iSeconds = opts.popup_timeout;
				break;

			case POPUP_DELAY_PERMANENT:
				ppd.iSeconds = -1;
				break;

			case POPUP_DELAY_DEFAULT:
			default:
				ppd.iSeconds = 0;
				break;
			}
		}
		else // if (type == POPUP_TYPE_ERROR)
		{
			ppd.iSeconds = 0;
		}

		// Now that every field has been filled, we want to see the popup.
		PUAddPopupT(&ppd);
	}
	else {
		MessageBox(nullptr, description, title ? title : (wchar_t *)pcli->pfnGetContactDisplayName(hContact, 0),
			MB_OK);
	}

}


// Handle to the hidden windows to handle actions for popup clicks
// wParam has the number of MOTD in case of WMU_SHOW_MOTD_DETAILS
LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WMU_ACTION) {
		if (lParam == POPUP_ACTION_OPENAVATARHISTORY) {
			CallService("AvatarHistory/ShowDialog", wParam, 0);
		}
		else if (lParam == POPUP_ACTION_OPENHISTORY) {
			CallService(MS_HISTORY_SHOWCONTACTHISTORY, wParam, 0);
		}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Handle to popup events
static LRESULT CALLBACK PopupDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PopupDataType *popup = (PopupDataType*)PUGetPluginData(hWnd);

	switch (message) {
	case WM_COMMAND:
		PostMessage(hPopupWindow, WMU_ACTION, (WPARAM)popup->plugin_data, opts.popup_left_click_action);

		if (opts.popup_left_click_action != POPUP_ACTION_DONOTHING)
			PUDeletePopup(hWnd);

		return TRUE;

	case WM_CONTEXTMENU:
		PostMessage(hPopupWindow, WMU_ACTION, (WPARAM)popup->plugin_data, opts.popup_right_click_action);

		if (opts.popup_right_click_action != POPUP_ACTION_DONOTHING)
			PUDeletePopup(hWnd);

		return TRUE;

	case UM_FREEPLUGINDATA:
		if ((INT_PTR)popup != CALLSERVICE_NOTFOUND) {
			DestroyIcon(popup->hIcon);
			mir_free(popup);
		}
		return FALSE; //the return value is ignored
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


// Handle to popup events
static LRESULT CALLBACK DumbPopupDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_COMMAND:
		PUDeletePopup(hWnd);
		return TRUE;

	case WM_CONTEXTMENU:
		PUDeletePopup(hWnd);
		return TRUE;

	case UM_FREEPLUGINDATA:
		PopupDataType* popup = (PopupDataType*)PUGetPluginData(hWnd);
		if ((INT_PTR)popup != CALLSERVICE_NOTFOUND) {
			DestroyIcon(popup->hIcon);
			mir_free(popup);
		}
		return FALSE; //the return value is ignored
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
