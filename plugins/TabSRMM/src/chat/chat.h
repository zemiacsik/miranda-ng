/////////////////////////////////////////////////////////////////////////////////////////
// Miranda NG: the free IM client for Microsoft* Windows*
//
// Copyright (�) 2012-16 Miranda NG project,
// Copyright (c) 2000-09 Miranda ICQ/IM project,
// all portions of this codebase are copyrighted to the people
// listed in contributors.txt.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// you should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// part of tabSRMM messaging plugin for Miranda.
//
// (C) 2005-2010 by silvercircle _at_ gmail _dot_ com and contributors

#ifndef _CHAT_H_
#define _CHAT_H_

//defines

enum TChatStatusEx
{
	CHAT_STATUS_NORMAL,
	CHAT_STATUS_AWAY,
	CHAT_STATUS_OFFLINE,
	CHAT_STATUS_MAX
};

// special service for tweaking performance
#define MS_GC_GETEVENTPTR  "GChat/GetNewEventPtr"
typedef INT_PTR(*GETEVENTFUNC)(WPARAM wParam, LPARAM lParam);
typedef struct  {
	GETEVENTFUNC pfnAddEvent;
}GCPTRS;

class CMUCHighlight;

//structs

struct MODULEINFO : public GCModuleInfoBase
{
	DWORD          idleTimeStamp;
	DWORD          lastIdleCheck;
	wchar_t          tszIdleMsg[60];
};

struct SESSION_INFO : public GCSessionInfoBase
{
	TWindowData    *dat;
	TContainerData *pContainer;
	int             iLogTrayFlags, iLogPopupFlags, iDiskLogFlags;

	int             iSearchItem;
	wchar_t         szSearch[255];
};

struct LOGSTREAMDATA : public GCLogStreamDataBase
{
	int           crCount;
	TWindowData  *dat;
};

struct TMUCSettings : public GlobalLogSettingsBase
{
	HICON       hIconOverlay;
	DWORD       dwIconFlags;
	LONG        iNickListFontHeight;
	int         iEventLimitThreshold;

	HFONT       UserListFonts[CHAT_STATUS_MAX];
	COLORREF    UserListColors[CHAT_STATUS_MAX];

	COLORREF    nickColors[8];
	HBRUSH      SelectionBGBrush;
	bool        bOpenInDefault, bBBCodeInPopups;
	bool        bDoubleClick4Privat, bShowContactStatus, bContactStatusFirst;

	bool        bLogClassicIndicators, bAlternativeSorting, bAnnoyingHighlight, bCreateWindowOnHighlight;
	bool        bLogSymbols, bClassicIndicators, bClickableNicks, bColorizeNicks, bColorizeNicksInLog;
	bool        bScaleIcons, bUseDividers, bDividersUsePopupConfig, bUseCommaAsColon, bNewLineAfterNames;

	CMUCHighlight* Highlight;
};

struct FLASH_PARAMS
{
	MCONTACT hContact;
	const char* sound;
	int   iEvent;
	HICON hNotifyIcon;
	bool  bActiveTab, bHighlight, bInactive, bMustFlash, bMustAutoswitch;
	HWND  hWnd;
};

extern TMUCSettings g_Settings;
extern CHAT_MANAGER saveCI;

#pragma comment(lib,"comctl32.lib")

//////////////////////////////////////////////////////////////////////////////////

// log.c
void   Log_StreamInEvent(HWND hwndDlg, LOGINFO* lin, SESSION_INFO *si, bool bRedraw);
char*  Log_CreateRtfHeader(MODULEINFO *mi);

// window.c
INT_PTR CALLBACK RoomWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
int GetTextPixelSize(wchar_t* pszText, HFONT hFont, bool bWidth);

// options.c
enum { FONTSECTION_AUTO, FONTSECTION_IM, FONTSECTION_IP };
void  LoadMsgDlgFont(int section, int i, LOGFONT * lf, COLORREF * colour, char* szMod);
void  AddIcons(void);
HICON LoadIconEx(char *pszIcoLibName);

// services.c
void ShowRoom(SESSION_INFO *si);

HWND CreateNewRoom(TContainerData *pContainer, SESSION_INFO *si, BOOL bActivateTab, BOOL bPopupContainer, BOOL bWantPopup);

// manager.c
SESSION_INFO* SM_FindSessionByHWND(HWND h);
SESSION_INFO* SM_FindSessionByHCONTACT(MCONTACT h);
SESSION_INFO* SM_FindSessionAutoComplete(const char* pszModule, SESSION_INFO* currSession, SESSION_INFO* prevSession, const wchar_t* pszOriginal, const wchar_t* pszCurrent);

void SM_RemoveContainer(TContainerData *pContainer);
BOOL SM_ReconfigureFilters();

int UM_CompareItem(USERINFO *u1, const wchar_t* pszNick, WORD wStatus);

// tools.c
BOOL     DoSoundsFlashPopupTrayStuff(SESSION_INFO *si, GCEVENT *gce, BOOL bHighlight, int bManyFix);
int      Chat_GetColorIndex(const char* pszModule, COLORREF cr);
wchar_t* my_strstri(const wchar_t* s1, const wchar_t* s2);
int      GetRichTextLength(HWND hwnd);
bool     IsHighlighted(SESSION_INFO *si, GCEVENT *pszText);
char     GetIndicator(SESSION_INFO *si, LPCTSTR ptszNick, int *iNickIndex);
UINT     CreateGCMenu(HWND hwndDlg, HMENU *hMenu, int iIndex, POINT pt, SESSION_INFO *si, wchar_t* pszUID, wchar_t* pszWordText);
void     DestroyGCMenu(HMENU *hMenu, int iIndex);
void     Chat_SetFilters(SESSION_INFO *si);
void     DoFlashAndSoundWorker(FLASH_PARAMS* p);
BOOL     DoPopup(SESSION_INFO *si, GCEVENT* gce);
int      ShowPopup(MCONTACT hContact, SESSION_INFO *si, HICON hIcon, char* pszProtoName, wchar_t* pszRoomName, COLORREF crBkg, const wchar_t* fmt, ...);
BOOL     LogToFile(SESSION_INFO *si, GCEVENT *gce);

#include "chat_resource.h"

extern char szIndicators[];

#define DEFLOGFILENAME L"%miranda_logpath%\\%proto%\\%userid%.log"

#endif
