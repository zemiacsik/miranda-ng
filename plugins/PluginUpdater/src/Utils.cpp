/*
Copyright (C) 2010 Mataes

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

HNETLIBUSER hNetlibUser = nullptr;
HANDLE hPipe = nullptr;

/////////////////////////////////////////////////////////////////////////////////////
void LoadOptions()
{
	PopupOptions.DefColors = db_get_b(NULL, MODNAME, "DefColors", DEFAULT_COLORS);
	PopupOptions.LeftClickAction= db_get_b(NULL, MODNAME, "LeftClickAction", DEFAULT_POPUP_LCLICK);
	PopupOptions.RightClickAction = db_get_b(NULL, MODNAME, "RightClickAction", DEFAULT_POPUP_RCLICK);
	PopupOptions.Timeout = db_get_dw(NULL, MODNAME, "Timeout", DEFAULT_TIMEOUT_VALUE);

	opts.bUpdateOnStartup = db_get_b(NULL, MODNAME, "UpdateOnStartup", DEFAULT_UPDATEONSTARTUP);
	opts.bOnlyOnceADay = db_get_b(NULL, MODNAME, "OnlyOnceADay", DEFAULT_ONLYONCEADAY);
	opts.bUpdateOnPeriod = db_get_b(NULL, MODNAME, "UpdateOnPeriod", DEFAULT_UPDATEONPERIOD);
	opts.Period = db_get_dw(NULL, MODNAME, "Period", DEFAULT_PERIOD);
	opts.bPeriodMeasure = db_get_b(NULL, MODNAME, "PeriodMeasure", DEFAULT_PERIODMEASURE);
	opts.bForceRedownload = db_get_b(NULL, MODNAME, DB_SETTING_REDOWNLOAD, 0);
	opts.bSilentMode = db_get_b(NULL, MODNAME, "SilentMode", 0);
	opts.bBackup = db_get_b(NULL, MODNAME, "Backup", 0);
	opts.bChangePlatform = db_get_b(NULL, MODNAME, DB_SETTING_CHANGEPLATFORM, 0);
}

#if MIRANDA_VER >= 0x0A00
IconItemT iconList[] =
{
	{ LPGENW("Check for updates"),"check_update", IDI_MENU },
	{ LPGENW("Plugin info"), "info", IDI_INFO },
	{ LPGENW("Component list"),"plg_list", IDI_PLGLIST }
};

void InitIcoLib()
{
	Icon_RegisterT(hInst,MODULE,iconList, _countof(iconList));
}
#endif

void InitNetlib()
{
	NETLIBUSER nlu = {};
	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_HTTPCONNS | NUF_UNICODE;
	#if MIRANDA_VER >= 0x0A00
		nlu.szDescriptiveName.w = TranslateT("Plugin Updater HTTP connections");
	#else
		nlu.cbSize = sizeof(nlu);
		nlu.ptszDescriptiveName = TranslateT("Plugin Updater HTTP connections");
	#endif
	nlu.szSettingsModule = MODNAME;
	hNetlibUser = Netlib_RegisterUser(&nlu);
}

void UnloadNetlib()
{
	Netlib_CloseHandle(hNetlibUser);
	hNetlibUser = nullptr;
}

ULONG crc32_table[256];
ULONG ulPolynomial = 0x04c11db7;

//////////////////////////////////////////////////////////////
// Reflection is a requirement for the official CRC-32 standard.
// You can create CRCs without it, but they won't conform to the standard.
//////////////////////////////////////////////////////////////////////////

ULONG Reflect(ULONG ref, char ch)
{
	// Used only by Init_CRC32_Table()
	ULONG value(0);

	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for(int i = 1; i < (ch + 1); i++)
	{
		if(ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}

void InitCrcTable()
{
	// 256 values representing ASCII character codes.
	for(int i = 0; i <= 0xFF; i++)
	{
		crc32_table[i] = Reflect(i, 8) << 24;
		for (int j = 0; j < 8; j++)
			crc32_table[i] = (crc32_table[i] << 1) ^ (crc32_table[i] & (1 << 31) ? ulPolynomial : 0);
		crc32_table[i] = Reflect(crc32_table[i], 32);
	}
}

int Get_CRC(unsigned char* buffer, ULONG bufsize)
{
	ULONG  crc(0xffffffff);
	int len;
	len = bufsize;
	// Save the text in the buffer.

	// Perform the algorithm on each character
	// in the string, using the lookup table values.

	for(int i = 0; i < len; i++)
		crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buffer[i]];

	// Exclusive OR the result with the beginning value.
	return crc^0xffffffff;
}

int CompareHashes(const ServListEntry *p1, const ServListEntry *p2)
{
	return _wcsicmp(p1->m_name, p2->m_name);
}

bool ParseHashes(const wchar_t *ptszUrl, ptrW &baseUrl, SERVLIST &arHashes)
{
	REPLACEVARSARRAY vars[2];
#if MIRANDA_VER >=0x0A00
	vars[0].key.w = L"platform";
#ifdef _WIN64
	vars[0].value.w = L"64";
#else
	vars[0].value.w = L"32";
#endif
	vars[1].key.w = vars[1].value.w = nullptr;
#else
	vars[0].lptzKey = L"platform";
#ifdef _WIN64
	vars[0].lptzValue = L"64";
#else
	vars[0].lptzValue = L"32";
#endif
	vars[1].lptzKey = vars[1].lptzValue = 0;
#endif
	baseUrl = Utils_ReplaceVarsW(ptszUrl, 0, vars);

	// Download version info
	FILEURL pFileUrl;
	mir_snwprintf(pFileUrl.tszDownloadURL, L"%s/hashes.zip", baseUrl);
	mir_snwprintf(pFileUrl.tszDiskPath, L"%s\\hashes.zip", g_tszTempPath);
	pFileUrl.CRCsum = 0;

	HNETLIBCONN nlc = nullptr;
	bool ret = DownloadFile(&pFileUrl, nlc);
	Netlib_CloseHandle(nlc);

	if (!ret) {
		Netlib_LogfW(hNetlibUser,L"Downloading list of available updates from %s failed",baseUrl);
		ShowPopup(TranslateT("Plugin Updater"), TranslateT("An error occurred while checking for new updates."), POPUP_TYPE_ERROR);
		Skin_PlaySound("updatefailed");
		return false;
	}

	if(!unzip(pFileUrl.tszDiskPath, g_tszTempPath, nullptr,true)) {
		Netlib_LogfW(hNetlibUser,L"Unzipping list of available updates from %s failed",baseUrl);
		ShowPopup(TranslateT("Plugin Updater"), TranslateT("An error occurred while checking for new updates."), POPUP_TYPE_ERROR);
		Skin_PlaySound("updatefailed");
		return false;
	}
	
	DeleteFile(pFileUrl.tszDiskPath);

	wchar_t tszTmpIni[MAX_PATH] = {0};
	mir_snwprintf(tszTmpIni, L"%s\\hashes.txt", g_tszTempPath);
	FILE *fp = _wfopen(tszTmpIni, L"r");
	if (!fp) {
		Netlib_LogfW(hNetlibUser,L"Opening %s failed", g_tszTempPath);
		ShowPopup(TranslateT("Plugin Updater"), TranslateT("An error occurred while checking for new updates."), POPUP_TYPE_ERROR);
		return false;
	}

	bool bDoNotSwitchToStable = false;
	char str[200];
	while(fgets(str, _countof(str), fp) != nullptr) {
		rtrim(str);
		// Do not allow the user to switch back to stable
		if (!strcmp(str, "DoNotSwitchToStable")) {
			bDoNotSwitchToStable = true;
		}
		else if (str[0] != ';') { // ';' marks a comment
			Netlib_Logf(hNetlibUser, "Update: %s", str);
			char *p = strchr(str, ' ');
			if (p != nullptr) {
				*p++ = 0;
				_strlwr(p);

				int dwCrc32;
				char *p1 = strchr(p, ' ');
				if (p1 == nullptr)
					dwCrc32 = 0;
				else {
					*p1++ = 0;
					sscanf(p1, "%08x", &dwCrc32);
				}
				arHashes.insert(new ServListEntry(str, p, dwCrc32));
			}
		}
	}
	fclose(fp);
	DeleteFile(tszTmpIni);

	if (bDoNotSwitchToStable) {
		db_set_b(NULL, MODNAME, DB_SETTING_DONT_SWITCH_TO_STABLE, 1);
		// Reset setting if needed
		int UpdateMode = db_get_b(NULL, MODNAME, DB_SETTING_UPDATE_MODE, UPDATE_MODE_STABLE);
		if (UpdateMode == UPDATE_MODE_STABLE)
			db_set_b(NULL, MODNAME, DB_SETTING_UPDATE_MODE, UPDATE_MODE_TRUNK);
	}
	else
		db_set_b(NULL, MODNAME, DB_SETTING_DONT_SWITCH_TO_STABLE, 0);

	return true;
}


bool DownloadFile(FILEURL *pFileURL, HNETLIBCONN &nlc)
{
	NETLIBHTTPREQUEST nlhr = {0};
#if MIRANDA_VER < 0x0A00
	nlhr.cbSize = NETLIBHTTPREQUEST_V1_SIZE;
	nlhr.flags = NLHRF_DUMPASTEXT | NLHRF_HTTP11;
	if (g_mirandaVersion >= PLUGIN_MAKE_VERSION(0, 9, 0, 0))
		nlhr.flags |= NLHRF_PERSISTENT;
#else
	nlhr.cbSize = sizeof(nlhr);
	nlhr.flags = NLHRF_DUMPASTEXT | NLHRF_HTTP11 | NLHRF_PERSISTENT;
#endif
	nlhr.requestType = REQUEST_GET;
	nlhr.nlc = nlc;
	char *szUrl = mir_u2a(pFileURL->tszDownloadURL);
	nlhr.szUrl = szUrl;
	nlhr.headersCount = 4;
	nlhr.headers=(NETLIBHTTPHEADER*)mir_alloc(sizeof(NETLIBHTTPHEADER)*nlhr.headersCount);
	nlhr.headers[0].szName   = "User-Agent";
	nlhr.headers[0].szValue = NETLIB_USER_AGENT;
	nlhr.headers[1].szName  = "Connection";
	nlhr.headers[1].szValue = "close";
	nlhr.headers[2].szName  = "Cache-Control";
	nlhr.headers[2].szValue = "no-cache";
	nlhr.headers[3].szName  = "Pragma";
	nlhr.headers[3].szValue = "no-cache";

	bool ret = false;
	for (int i = 0; !ret && i < MAX_RETRIES; i++) {
		Netlib_LogfW(hNetlibUser,L"Downloading file %s to %s (attempt %d)",pFileURL->tszDownloadURL,pFileURL->tszDiskPath, i+1);
		NETLIBHTTPREQUEST *pReply = Netlib_HttpTransaction(hNetlibUser, &nlhr);
		if (pReply) {
			nlc = pReply->nlc;
			if ((200 == pReply->resultCode) && (pReply->dataLength > 0)) {
				// Check CRC sum
				if (pFileURL->CRCsum) {
					InitCrcTable();
					int crc = Get_CRC((unsigned char*)pReply->pData, pReply->dataLength);
					if (crc != pFileURL->CRCsum) {
						// crc check failed, try again
						Netlib_LogfW(hNetlibUser,L"crc check failed for file %s",pFileURL->tszDiskPath);
						Netlib_FreeHttpRequest(pReply);
						continue;
					}
				}

				HANDLE hFile = CreateFile(pFileURL->tszDiskPath, GENERIC_READ | GENERIC_WRITE, NULL, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (hFile != INVALID_HANDLE_VALUE) {
					DWORD dwBytes;
					// write the downloaded file directly
					WriteFile(hFile, pReply->pData, (DWORD)pReply->dataLength, &dwBytes, nullptr);
					CloseHandle(hFile);
				}
				else {
					// try to write it via PU stub
					wchar_t tszTempFile[MAX_PATH];
					mir_snwprintf(tszTempFile, L"%s\\pulocal.tmp", g_tszTempPath);
					hFile = CreateFile(tszTempFile, GENERIC_READ | GENERIC_WRITE, NULL, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
					if (hFile != INVALID_HANDLE_VALUE) {
						DWORD dwBytes;
						WriteFile(hFile, pReply->pData, (DWORD)pReply->dataLength, &dwBytes, nullptr);
						CloseHandle(hFile);
						SafeMoveFile(tszTempFile, pFileURL->tszDiskPath);
					}
				}
				ret = true;
			}
			else Netlib_LogfW(hNetlibUser,L"Downloading file %s failed with error %d",pFileURL->tszDownloadURL,pReply->resultCode);
			
			Netlib_FreeHttpRequest(pReply);
		}
		else {
			Netlib_LogfW(hNetlibUser,L"Downloading file %s failed, host is propably temporary down.",pFileURL->tszDownloadURL);
			nlc = nullptr;
		}
	}
	if(!ret)
		Netlib_LogfW(hNetlibUser,L"Downloading file %s failed, giving up",pFileURL->tszDownloadURL);

	mir_free(szUrl);
	mir_free(nlhr.headers);

	return ret;
}

void __stdcall OpenPluginOptions(void*)
{
	#if MIRANDA_VER >= 0x0A00
		Options_Open(nullptr, L"Plugins");
	#endif
}

//   FUNCTION: IsRunAsAdmin()
//
//   PURPOSE: The function checks whether the current process is run as
//   administrator. In other words, it dictates whether the primary access
//   token of the process belongs to user account that is a member of the
//   local Administrators group and it is elevated.
//
//   RETURN VALUE: Returns TRUE if the primary access token of the process
//   belongs to user account that is a member of the local Administrators
//   group and it is elevated. Returns FALSE if the token does not.
//
//   EXCEPTION: If this function fails, it throws a C++ DWORD exception which
//   contains the Win32 error code of the failure.
//
//   EXAMPLE CALL:
//     try
//     {
//         if (IsRunAsAdmin())
//             wprintf (L"Process is run as administrator\n");
//         else
//             wprintf (L"Process is not run as administrator\n");
//     }
//     catch (DWORD dwError)
//     {
//         wprintf(L"IsRunAsAdmin failed w/err %lu\n", dwError);
//     }
//
BOOL IsRunAsAdmin()
{
	BOOL fIsRunAsAdmin = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	PSID pAdministratorsGroup = nullptr;

	// Allocate and initialize a SID of the administrators group.
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (!AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&pAdministratorsGroup))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Determine whether the SID of administrators group is bEnabled in
	// the primary access token of the process.
	if (!CheckTokenMembership(nullptr, pAdministratorsGroup, &fIsRunAsAdmin))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (pAdministratorsGroup)
	{
		FreeSid(pAdministratorsGroup);
		pAdministratorsGroup = nullptr;
	}

	// Throw the error if something failed in the function.
	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return fIsRunAsAdmin;
}

//
//   FUNCTION: IsProcessElevated()
//
//   PURPOSE: The function gets the elevation information of the current
//   process. It dictates whether the process is elevated or not. Token
//   elevation is only available on Windows Vista and newer operating
//   systems, thus IsProcessElevated throws a C++ exception if it is called
//   on systems prior to Windows Vista. It is not appropriate to use this
//   function to determine whether a process is run as administartor.
//
//   RETURN VALUE: Returns TRUE if the process is elevated. Returns FALSE if
//   it is not.
//
//   EXCEPTION: If this function fails, it throws a C++ DWORD exception
//   which contains the Win32 error code of the failure. For example, if
//   IsProcessElevated is called on systems prior to Windows Vista, the error
//   code will be ERROR_INVALID_PARAMETER.
//
//   NOTE: TOKEN_INFORMATION_CLASS provides TokenElevationType to check the
//   elevation type (TokenElevationTypeDefault / TokenElevationTypeLimited /
//   TokenElevationTypeFull) of the process. It is different from
//   TokenElevation in that, when UAC is turned off, elevation type always
//   returns TokenElevationTypeDefault even though the process is elevated
//   (Integrity Level == High). In other words, it is not safe to say if the
//   process is elevated based on elevation type. Instead, we should use
//   TokenElevation.
//
//   EXAMPLE CALL:
//     try
//     {
//         if (IsProcessElevated())
//             wprintf (L"Process is elevated\n");
//         else
//             wprintf (L"Process is not elevated\n");
//     }
//     catch (DWORD dwError)
//     {
//         wprintf(L"IsProcessElevated failed w/err %lu\n", dwError);
//     }
//
BOOL IsProcessElevated()
{
	BOOL fIsElevated = FALSE;
	DWORD dwError = ERROR_SUCCESS;
	HANDLE hToken = nullptr;

	// Open the primary access token of the process with TOKEN_QUERY.
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	// Retrieve token elevation information.
	TOKEN_ELEVATION elevation;
	DWORD dwSize;
	if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize))
	{
		// When the process is run on operating systems prior to Windows
		// Vista, GetTokenInformation returns FALSE with the
		// ERROR_INVALID_PARAMETER error code because TokenElevation is
		// not supported on those operating systems.
		dwError = GetLastError();
		goto Cleanup;
	}

	fIsElevated = elevation.TokenIsElevated;

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (hToken)
	{
		CloseHandle(hToken);
		hToken = nullptr;
	}

	// Throw the error if something failed in the function.
	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return fIsElevated;
}

bool PrepareEscalation()
{
	// First try to create a file near Miranda32.exe
	wchar_t szPath[MAX_PATH];
	GetModuleFileName(nullptr, szPath, _countof(szPath));
	wchar_t *ext = wcsrchr(szPath, '.');
	if (ext != nullptr)
		*ext = '\0';
	wcscat(szPath, L".test");
	HANDLE hFile = CreateFile(szPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile != INVALID_HANDLE_VALUE) {
		// we are admins or UAC is disable, cool
		CloseHandle(hFile);
		DeleteFile(szPath);
		return true;
	}
	else if (IsRunAsAdmin()) {
		// Check the current process's "run as administrator" status.
		return true;
	}
	else {
		// Elevate the process. Create a pipe for a stub first
		wchar_t tszPipeName[MAX_PATH];
		mir_snwprintf(tszPipeName, L"\\\\.\\pipe\\Miranda_Pu_%d", GetCurrentProcessId());
		hPipe = CreateNamedPipe(tszPipeName, PIPE_ACCESS_DUPLEX, PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1024, 1024, NMPWAIT_USE_DEFAULT_WAIT, nullptr);
		if (hPipe == INVALID_HANDLE_VALUE) {
			hPipe = nullptr;
		}
		else {
			wchar_t cmdLine[100], *p;
			GetModuleFileName(nullptr, szPath, ARRAYSIZE(szPath));
			if ((p = wcsrchr(szPath, '\\')) != nullptr)
				wcscpy(p+1, L"pu_stub.exe");
			mir_snwprintf(cmdLine, L"%d", GetCurrentProcessId());

			// Launch a stub
			SHELLEXECUTEINFO sei = { sizeof(sei) };
			sei.lpVerb = L"runas";
			sei.lpFile = szPath;
			sei.lpParameters = cmdLine;
			sei.hwnd = nullptr;
			sei.nShow = SW_NORMAL;
			if (ShellExecuteEx(&sei)) {
				if (hPipe != nullptr)
					ConnectNamedPipe(hPipe, nullptr);
				return true;
			}

			DWORD dwError = GetLastError();
			if (dwError == ERROR_CANCELLED)
			{
				// The user refused to allow privileges elevation.
				// Do nothing ...
			}
		}
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

int TransactPipe(int opcode, const wchar_t *p1, const wchar_t *p2)
{
	BYTE buf[1024];
	DWORD l1 = lstrlen(p1), l2 = lstrlen(p2);
	if (l1 > MAX_PATH || l2 > MAX_PATH)
		return 0;

	*(DWORD*)buf = opcode;
	wchar_t *dst = (wchar_t*)&buf[sizeof(DWORD)];
	lstrcpy(dst, p1);
	dst += l1+1;
	if (p2) {
		lstrcpy(dst, p2);
		dst += l2+1;
	}
	else *dst++ = 0;

	DWORD dwBytes = 0, dwError;
	if ( WriteFile(hPipe, buf, (DWORD)((BYTE*)dst - buf), &dwBytes, nullptr) == 0)
		return 0;

	dwError = 0;
	if ( ReadFile(hPipe, &dwError, sizeof(DWORD), &dwBytes, nullptr) == 0) return 0;
	if (dwBytes != sizeof(DWORD)) return 0;

	return dwError == ERROR_SUCCESS;
}

int SafeCopyFile(const wchar_t *pSrc, const wchar_t *pDst)
{
	if (hPipe == nullptr)
		return CopyFile(pSrc, pDst, FALSE);

	return TransactPipe(1, pSrc, pDst);
}

int SafeMoveFile(const wchar_t *pSrc, const wchar_t *pDst)
{
	if (hPipe == nullptr) {
		DeleteFile(pDst);
		if ( MoveFile(pSrc, pDst) == 0) // use copy on error
			CopyFile(pSrc, pDst, FALSE);
		DeleteFile(pSrc);
	}

	return TransactPipe(2, pSrc, pDst);
}

int SafeDeleteFile(const wchar_t *pFile)
{
	if (hPipe == nullptr)
		return DeleteFile(pFile);

	return TransactPipe(3, pFile, nullptr);
}

int SafeCreateDirectory(const wchar_t *pFolder)
{
	if (hPipe == nullptr)
		return CreateDirectoryTreeW(pFolder);

	return TransactPipe(4, pFolder, nullptr);
}

int SafeCreateFilePath(wchar_t *pFolder)
{
	if (hPipe == nullptr) {
		CreatePathToFileW(pFolder);
		return 0;
	}

	return TransactPipe(5, pFolder, nullptr);
}

void BackupFile(wchar_t *ptszSrcFileName, wchar_t *ptszBackFileName)
{
	SafeCreateFilePath(ptszBackFileName);
	SafeMoveFile(ptszSrcFileName, ptszBackFileName);
}

/////////////////////////////////////////////////////////////////////////////////////////

char *StrToLower(char *str)
{
	for (int i = 0; str[i]; i++)
		str[i] = tolower(str[i]);

	return str;
}
