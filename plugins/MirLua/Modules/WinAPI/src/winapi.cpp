#include "stdafx.h"

typedef int(__stdcall *MSGBOXAAPI)(IN HWND hWnd, IN LPCSTR lpText, IN LPCSTR lpCaption, IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

int MessageBoxTimeoutA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType, WORD wLanguageId, DWORD dwMilliseconds)
{
	static MSGBOXAAPI MsgBoxTOA = NULL;

	if (!MsgBoxTOA)
	{
		if (HMODULE hUser32 = GetModuleHandle(_T("user32.dll")))
		{
			MsgBoxTOA = (MSGBOXAAPI)GetProcAddress(hUser32, "MessageBoxTimeoutA");
		}
	}

	if (MsgBoxTOA)
	{
		return MsgBoxTOA(hWnd, lpText, lpCaption, uType, wLanguageId, dwMilliseconds);
	}
	return MessageBoxA(hWnd, lpText, lpCaption, uType);
}

#define MB_TIMEDOUT 32000

static int lua_MessageBox(lua_State *L)
{
	HWND hwnd = (HWND)lua_touserdata(L, 1);
	ptrA text(mir_utf8decodeA(lua_tostring(L, 2)));
	ptrA caption(mir_utf8decodeA(lua_tostring(L, 3)));
	UINT flags = lua_tointeger(L, 4);
	LANGID langId = GetUserDefaultUILanguage();
	DWORD timeout = luaL_optinteger(L, 5, 0xFFFFFFFF);

	int res = ::MessageBoxTimeoutA(hwnd, text, caption, flags, langId, timeout);
	lua_pushinteger(L, res);

	return 1;
}

/***********************************************/

static int lua_ShellExecute(lua_State *L)
{
	ptrT command(mir_utf8decodeT(lua_tostring(L, 1)));
	ptrT file(mir_utf8decodeT(lua_tostring(L, 2)));
	ptrT args(mir_utf8decodeT(lua_tostring(L, 3)));
	int flags = lua_tointeger(L, 4);

	::ShellExecute(NULL, command, file, args, NULL, flags);

	return 0;
}

/***********************************************/

static int lua_FindIterator(lua_State *L)
{
	HANDLE hFind = lua_touserdata(L, lua_upvalueindex(1));
	TCHAR* path = (TCHAR*)lua_touserdata(L, lua_upvalueindex(2));

	WIN32_FIND_DATA ffd = { 0 };
	if (hFind == NULL)
	{
		hFind = FindFirstFile(path, &ffd);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			mir_free(path);
			lua_pushnil(L);
			return 1;
		}
	}
	else
	{
		if (FindNextFile(hFind, &ffd) == 0)
		{
			FindClose(hFind);
			mir_free(path);
			lua_pushnil(L);
			return 1;
		}
	}

	if (!mir_tstrcmpi(ffd.cFileName, _T(".")) ||
		!mir_tstrcmpi(ffd.cFileName, _T("..")) ||
		(ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
	{
		lua_pushlightuserdata(L, hFind);
		lua_replace(L, lua_upvalueindex(1));

		return lua_FindIterator(L);
	}

	LARGE_INTEGER size;
	size.HighPart = ffd.nFileSizeHigh;
	size.LowPart = ffd.nFileSizeLow;

	lua_newtable(L);
	lua_pushliteral(L, "Name");
	lua_pushstring(L, T2Utf(ffd.cFileName));
	lua_settable(L, -3);
	lua_pushliteral(L, "Size");
	lua_pushinteger(L, size.QuadPart);
	lua_settable(L, -3);
	lua_pushliteral(L, "IsFile");
	lua_pushboolean(L, !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
	lua_settable(L, -3);
	lua_pushliteral(L, "IsDirectory");
	lua_pushboolean(L, ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	lua_settable(L, -3);

	lua_pushlightuserdata(L, hFind);
	lua_replace(L, lua_upvalueindex(1));

	return 1;
}

static int lua_Find(lua_State *L)
{
	TCHAR *path = mir_utf8decodeT(luaL_checkstring(L, 1));

	lua_pushlightuserdata(L, NULL);
	lua_pushlightuserdata(L, path);
	lua_pushcclosure(L, lua_FindIterator, 2);

	return 1;
}

static int lua_GetKeyState(lua_State *L)
{
	int vKey = luaL_checkinteger(L, 1);

	int res = GetKeyState(vKey);
	lua_pushinteger(L, res);

	return 1;
}

/***********************************************/

static int lua_GetIniValue(lua_State *L)
{
	ptrT path(mir_utf8decodeT(luaL_checkstring(L, 1)));
	ptrT section(mir_utf8decodeT(luaL_checkstring(L, 2)));
	ptrT key(mir_utf8decodeT(luaL_checkstring(L, 3)));

	if (lua_isinteger(L, 4))
	{
		int default = lua_tointeger(L, 4);

		UINT res = ::GetPrivateProfileInt(section, key, default, path);
		lua_pushinteger(L, res);

		return 1;
	}

	ptrT default(mir_utf8decodeT(lua_tostring(L, 4)));

	TCHAR value[MAX_PATH] = { 0 };
	if (!::GetPrivateProfileString(section, key, default, value, _countof(value), path))
	{
		lua_pushvalue(L, 4);
	}

	ptrA res(mir_utf8encodeT(value));
	lua_pushstring(L, res);

	return 1;
}

static int lua_SetIniValue(lua_State *L)
{
	ptrT path(mir_utf8decodeT(luaL_checkstring(L, 1)));
	ptrT section(mir_utf8decodeT(luaL_checkstring(L, 2)));
	ptrT key(mir_utf8decodeT(luaL_checkstring(L, 3)));
	ptrT value(mir_utf8decodeT(lua_tostring(L, 4)));

	bool res = ::WritePrivateProfileString(section, key, value, path) != 0;
	lua_pushboolean(L, res);

	return 1;
}

static int lua_DeleteIniValue(lua_State *L)
{
	ptrT path(mir_utf8decodeT(luaL_checkstring(L, 1)));
	ptrT section(mir_utf8decodeT(luaL_checkstring(L, 2)));
	ptrT key(mir_utf8decodeT(luaL_checkstring(L, 3)));

	bool res = ::WritePrivateProfileString(section, key, NULL, path) != 0;
	lua_pushboolean(L, res);

	return 1;
}

/***********************************************/

static int lua_GetRegValue(lua_State *L)
{
	HKEY hRootKey = (HKEY)luaL_checkinteger(L, 1);
	ptrT path(mir_utf8decodeT(luaL_checkstring(L, 2)));
	ptrT valueName(mir_utf8decodeT(luaL_checkstring(L, 3)));

	HKEY hKey = 0;
	LSTATUS res = ::RegOpenKeyEx(hRootKey, path, NULL, KEY_READ, &hKey);
	if (res != ERROR_SUCCESS)
	{
		lua_pushvalue(L, 4);
		return 1;
	}

	DWORD type = 0;
	DWORD length = 1024;
	BYTE* value = (BYTE*)mir_alloc(length);
	res = ::RegQueryValueEx(hKey, valueName, NULL, &type, (LPBYTE)value, &length);
	while (res == ERROR_MORE_DATA)
	{
		length += length;
		value = (BYTE*)mir_realloc(value, length);
		res = ::RegQueryValueEx(hKey, valueName, NULL, &type, (LPBYTE)value, &length);
	}

	if (res == ERROR_SUCCESS)
	{
		switch (type)
		{
		case REG_DWORD:
		case REG_DWORD_BIG_ENDIAN:
			lua_pushinteger(L, (INT_PTR)value);
			break;

		case REG_QWORD:
			lua_pushnumber(L, (INT_PTR)value);
			break;

		case REG_SZ:
		case REG_LINK:
		case REG_EXPAND_SZ:
		{
			ptrA str(Utf8EncodeT((TCHAR*)value));
			lua_pushlstring(L, str, mir_strlen(str));
		}
			break;

		default:
			lua_pushvalue(L, 4);
			break;
		}
	}
	else
		lua_pushvalue(L, 4);

	::RegCloseKey(hKey);
	mir_free(value);

	return 1;
}

static int lua_SetRegValue(lua_State *L)
{
	HKEY hRootKey = (HKEY)luaL_checkinteger(L, 1);
	ptrT path(mir_utf8decodeT(luaL_checkstring(L, 2)));
	ptrT valueName(mir_utf8decodeT(luaL_checkstring(L, 3)));

	HKEY hKey = 0;
	LSTATUS res = ::RegOpenKeyEx(hRootKey, path, NULL, KEY_WRITE, &hKey);
	if (res != ERROR_SUCCESS)
	{
		lua_pushboolean(L, FALSE);
		return 1;
	}

	DWORD type = 0;
	DWORD length = 0;
	BYTE* value = NULL;
	switch (lua_type(L, 4))
	{
	case LUA_TNUMBER:
		if (lua_isinteger(L, 4) && lua_tointeger(L, 4) < UINT32_MAX)
		{
			type = REG_DWORD;
			length = sizeof(DWORD);
			value = (BYTE*)lua_tointeger(L, 4);
		}
		else
		{
			type = REG_QWORD;
			length = sizeof(DWORD) * 2;
			lua_Number num = lua_tonumber(L, 4);
			value = (BYTE*)&num;
		}
		break;

	case LUA_TSTRING:
		type = REG_SZ;
		length = mir_strlen(lua_tostring(L, 4)) * sizeof(TCHAR);
		value = (BYTE*)mir_utf8decodeT(lua_tostring(L, 4));
		break;

	default:
		lua_pushboolean(L, FALSE);
		break;
	}

	res = ::RegSetValueEx(hKey, valueName, NULL, type, value, length);
	lua_pushboolean(L, res == ERROR_SUCCESS);
	
	::RegCloseKey(hKey);
	if (lua_isstring(L, 4))
		mir_free(value);

	return 1;
}

static int lua_DeleteRegValue(lua_State *L)
{
	HKEY hRootKey = (HKEY)luaL_checkinteger(L, 1);
	ptrT path(mir_utf8decodeT(luaL_checkstring(L, 2)));
	ptrT valueName(mir_utf8decodeT(luaL_checkstring(L, 3)));

	HKEY hKey = 0;
	LSTATUS res = ::RegOpenKeyEx(hRootKey, path, NULL, KEY_WRITE, &hKey);
	if (res != ERROR_SUCCESS)
	{
		lua_pushboolean(L, FALSE);
		return 1;
	}

	res = ::RegDeleteValue(hKey, valueName);
	lua_pushboolean(L, res == ERROR_SUCCESS);

	::RegCloseKey(hKey);

	return 1;
}

/* Registered functions */

static int global_FindWindow(lua_State *L) 
{
	const char *cname = luaL_checkstring(L, 1);
	const char *wname = luaL_checkstring(L, 2);
	lua_pushnumber(L, (intptr_t)FindWindowA(cname[0] ? cname : NULL, wname[0] ? wname : NULL));
	return(1);
}

static int global_FindWindowEx(lua_State *L) 
{
	const HWND parent = (HWND)(intptr_t)luaL_checknumber(L, 1);
	const HWND childaft = (HWND)(intptr_t)luaL_checknumber(L, 2);
	const char *cname = luaL_checkstring(L, 3);
	const char *wname = luaL_checkstring(L, 4);

	long lrc = (long)FindWindowExA(parent, childaft,
		cname[0] ? cname : NULL,
		wname[0] ? wname : NULL);

	lua_pushnumber(L, lrc);

	return(1);
}

static int global_SetWindowText(lua_State *L) 
{
	const HWND hwnd = (HWND)(intptr_t)luaL_checknumber(L, 1);
	const char *text = luaL_checkstring(L, 2);
	lua_pushboolean(L, SetWindowTextA(hwnd, text));
	return(1);
}

static int global_SetFocus(lua_State *L) 
{
	const HWND hWnd = (HWND)(intptr_t)luaL_checknumber(L, 1);
	lua_pushinteger(L, (intptr_t)SetFocus(hWnd));
	return 1;
}

// Lua:  returns nil when error occurred
static int global_GetWindowText(lua_State *L) 
{
	const HWND hWnd = (HWND)(intptr_t)luaL_checknumber(L, 1);
	char buf[2048] = { 0 };

	int rc = GetWindowTextA(hWnd, buf, sizeof(buf));

	if (rc > 0 || GetLastError() == ERROR_SUCCESS)
		lua_pushstring(L, buf);
	else
		lua_pushnil(L);

	return 1;
}

// Lua:  returns left,top,right,bottom when successfully
//         or
//       returns nil when error occurred
static int global_GetWindowRect(lua_State *L) {
	const HWND hwnd = (HWND)(intptr_t)luaL_checknumber(L, 1);
	RECT rect;

	BOOL rc = GetWindowRect(hwnd, &rect);

	if (!rc) {
		lua_pushnil(L);
		return(1);
	}
	else {
		lua_pushinteger(L, rect.left);
		lua_pushinteger(L, rect.top);
		lua_pushinteger(L, rect.right);
		lua_pushinteger(L, rect.bottom);
		return(4);
	}
}

static int global_SetForegroundWindow(lua_State *L) 
{
	HWND hWnd = (HWND)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, SetForegroundWindow(hWnd));
	return(1);
}

static int global_PostMessage(lua_State *L) 
{
	HWND hWnd = (HWND)(intptr_t)luaL_checknumber(L, 1);
	UINT msg = (UINT)luaL_checknumber(L, 2);
	WPARAM wparam = (WPARAM)luaL_checknumber(L, 3);
	LPARAM lparam = (LPARAM)luaL_checknumber(L, 4);

	lua_pushboolean(L, PostMessage(hWnd, msg, wparam, lparam));

	return(1);
}

static int global_PostThreadMessage(lua_State *L) 
{
	DWORD tid = (DWORD)luaL_checknumber(L, 1);
	UINT msg = (UINT)luaL_checknumber(L, 2);
	WPARAM wparam = (WPARAM)luaL_checknumber(L, 3);
	LPARAM lparam = (LPARAM)luaL_checknumber(L, 4);

	lua_pushboolean(L, PostThreadMessage(tid, msg, wparam, lparam));

	return(1);
}

static int global_GetMessage(lua_State *L) 
{
	MSG msg;
	HWND hWnd = (HWND)(intptr_t)luaL_optinteger(L, 1, 0);
	UINT mfmin = (UINT)luaL_optinteger(L, 2, 0);
	UINT mfmax = (UINT)luaL_optinteger(L, 3, 0);

	BOOL rc = GetMessage(&msg, hWnd, mfmin, mfmax);
	lua_pushboolean(L, rc);
	if (rc) 
	{
		lua_pushnumber(L, (intptr_t)msg.hwnd);
		lua_pushnumber(L, msg.message);
		lua_pushnumber(L, msg.wParam);
		lua_pushnumber(L, msg.lParam);
		lua_pushnumber(L, msg.time);
		lua_pushnumber(L, msg.pt.x);
		lua_pushnumber(L, msg.pt.y);
	}
	else 
	{
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
	}

	return(8);
}

static int global_PeekMessage(lua_State *L) 
{
	MSG msg;
	BOOL rc;
	HWND hWnd = (HWND)(intptr_t)luaL_optinteger(L, 1, 0);
	UINT mfmin = (UINT)luaL_optinteger(L, 2, 0);
	UINT mfmax = (UINT)luaL_optinteger(L, 3, 0);
	UINT rmmsg = (UINT)luaL_optinteger(L, 4, PM_NOREMOVE);

	rc = PeekMessage(&msg, hWnd, mfmin, mfmax, rmmsg);

	lua_pushboolean(L, rc);
	if (rc) 
	{
		lua_pushnumber(L, (long)msg.hwnd);
		lua_pushnumber(L, msg.message);
		lua_pushnumber(L, msg.wParam);
		lua_pushnumber(L, msg.lParam);
		lua_pushnumber(L, msg.time);
		lua_pushnumber(L, msg.pt.x);
		lua_pushnumber(L, msg.pt.y);
	}
	else 
	{
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
	}

	return(8);
}

static int global_ReplyMessage(lua_State *L) 
{
	LRESULT result = (LRESULT)luaL_checknumber(L, 1);
	lua_pushboolean(L, ReplyMessage(result));

	return(1);
}

static int global_DispatchMessage(lua_State *L) {
	MSG msg;
	LRESULT rc;
	HWND hWnd = (HWND)(intptr_t)luaL_checknumber(L, 1);

	msg.hwnd = hWnd;
	msg.message = (UINT)luaL_checknumber(L, 2);
	msg.wParam = (WPARAM)luaL_checknumber(L, 3);
	msg.lParam = (LPARAM)luaL_checknumber(L, 4);
	msg.time = (DWORD)luaL_checknumber(L, 5);
	msg.pt.x = (LONG)luaL_checknumber(L, 6);
	msg.pt.y = (LONG)luaL_checknumber(L, 7);

	rc = DispatchMessage(&msg);

	lua_pushnumber(L, rc);

	return(1);
}

static int global_SetTopmost(lua_State *L) 
{
	HWND hWnd = (HWND)(intptr_t)luaL_checknumber(L, 1);
	lua_pushnumber(L, SetWindowPos((HWND)hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE));

	return(1);
}

/****if* luaw32/global_GetLastError
* NAME
*  global_GetLastError
* FUNCTION
*  Win32 GetLastError.
* RESULT
*  stack[1]: The calling thread's last-error code value
* SOURCE
*/

static int global_GetLastError(lua_State *L) 
{
	lua_pushnumber(L, GetLastError());
	return(1);
}
/***/

/****if* luaw32/global_CloseHandle
* NAME
*  global_CloseHandle
* FUNCTION
*  Win32 CloseHandle.
* INPUTS
*  L: Lua state
*  stack[1]: Handle to an open object
* RESULT
*  stack[1]: True if ok.
* SOURCE
*/

static int global_CloseHandle(lua_State *L) 
{
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, CloseHandle(h));
	return(1);
}

static int global_CreateEvent(lua_State *L) 
{
	SECURITY_ATTRIBUTES sa;
	BOOL mr = (BOOL)luaL_checknumber(L, 2);
	BOOL is = (BOOL)luaL_checknumber(L, 3);
	const char *name;

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE;

	if (lua_istable(L, 1)) {
		lua_pushstring(L, "bInheritHandle");
		lua_gettable(L, 1);
		if (!lua_isnil(L, -1))
			sa.bInheritHandle = (BOOL)luaL_checknumber(L, -1);
		lua_pop(L, 1);
	}
	name = lua_tostring(L, 4);

	luaM_CheckPushNumber(L, CreateEventA(&sa, mr, is, name));
	return(1);
}

static int global_OpenEvent(lua_State *L) {
	long h;
	DWORD da = (DWORD)luaL_checknumber(L, 1);
	BOOL ih = (BOOL)luaL_checknumber(L, 2);
	const char *name = luaL_checkstring(L, 3);

	h = (long)OpenEventA(da, ih, name);

	if (h)
		lua_pushnumber(L, h);
	else
		lua_pushnil(L);

	return(1);
}

static int global_PulseEvent(lua_State *L) 
{
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, PulseEvent(h));
	return(1);
}

static int global_ResetEvent(lua_State *L) 
{
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, ResetEvent(h));
	return(1);
}

static int global_SetEvent(lua_State *L) 
{
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, SetEvent(h));
	return(1);
}

static int global_CreateMutex(lua_State *L) 
{
	SECURITY_ATTRIBUTES sa;
	BOOL io = (BOOL)luaL_checknumber(L, 2);
	const char *name;

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE;

	if (lua_istable(L, 1)) {
		lua_pushstring(L, "bInheritHandle");
		lua_gettable(L, 1);
		if (!lua_isnil(L, -1))
			sa.bInheritHandle = (BOOL)luaL_checknumber(L, -1);
		lua_pop(L, 1);
	}
	name = lua_tostring(L, 3);
	HANDLE h = (HANDLE)(intptr_t)CreateMutexA(&sa, io, name);
	luaM_CheckPushNumber(L, h);
	return(1);
}

static int global_OpenMutex(lua_State *L) 
{
	DWORD da = (DWORD)luaL_checknumber(L, 1);
	BOOL ih = (BOOL)luaL_checknumber(L, 2);
	const char *name = luaL_checkstring(L, 3);

	HANDLE h = (HANDLE)(intptr_t)OpenMutexA(da, ih, name);
	luaM_CheckPushNumber(L, h);
	return(1);
}

static int global_ReleaseMutex(lua_State *L) {
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushnumber(L, ReleaseMutex(h));
	return(1);
}

static int global_CreateSemaphore(lua_State *L) 
{
	SECURITY_ATTRIBUTES sa;
	long ic = (long)luaL_checknumber(L, 2);
	long mc = (long)luaL_checknumber(L, 3);
	const char *name;

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE;

	if (lua_istable(L, 1)) {
		lua_pushstring(L, "bInheritHandle");
		lua_gettable(L, 1);
		if (!lua_isnil(L, -1))
			sa.bInheritHandle = (BOOL)luaL_checknumber(L, -1);
		lua_pop(L, 1);
	}
	name = lua_tostring(L, 4);

	HANDLE h = (HANDLE)(intptr_t)CreateSemaphoreA(&sa, ic, mc, name);
	luaM_CheckPushNumber(L, h);
	return(1);
}

static int global_OpenSemaphore(lua_State *L) 
{
	DWORD da = (DWORD)luaL_checknumber(L, 1);
	BOOL ih = (BOOL)luaL_checknumber(L, 2);
	const char *name = luaL_checkstring(L, 3);

	HANDLE h = (HANDLE)(intptr_t)OpenSemaphoreA(da, ih, name);
	luaM_CheckPushNumber(L, h);
	return(1);
}

static int global_ReleaseSemaphore(lua_State *L)
{
	long pc;
	BOOL brc;
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	long rc = (long)luaL_checknumber(L, 2);

	brc = ReleaseSemaphore(h, rc, &pc);
	lua_pushnumber(L, brc);
	luaM_PushNumberIf(L, brc, rc);
	return(2);
}

static int global_CreateProcess(lua_State *L) 
{
	SECURITY_ATTRIBUTES psa;
	SECURITY_ATTRIBUTES tsa;
	PROCESS_INFORMATION pi;
	STARTUPINFOA si;
	BOOL brc;
	char *env;
	const char *an = luaL_optstring(L, 1, NULL);
	const char *cl = luaL_optstring(L, 2, NULL);
	BOOL ih = (BOOL)luaL_checknumber(L, 5);
	DWORD cf = (DWORD)luaL_checknumber(L, 6);
	const char *cd = lua_tostring(L, 8);

	psa.nLength = sizeof(psa);
	psa.lpSecurityDescriptor = NULL;
	psa.bInheritHandle = FALSE;
	if (lua_istable(L, 3)) {
		lua_pushstring(L, "bInheritHandle");
		lua_gettable(L, 3);
		if (!lua_isnil(L, -1))
			psa.bInheritHandle = (BOOL)luaL_checknumber(L, -1);
		lua_pop(L, 3);
	}

	tsa.nLength = sizeof(tsa);
	tsa.lpSecurityDescriptor = NULL;
	tsa.bInheritHandle = FALSE;
	if (lua_istable(L, 4)) {
		lua_pushstring(L, "bInheritHandle");
		lua_gettable(L, 4);
		if (!lua_isnil(L, -1))
			tsa.bInheritHandle = (BOOL)luaL_checknumber(L, -1);
		lua_pop(L, 4);
	}

	env = NULL;

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	if (lua_istable(L, 7)) {
		lua_pushnil(L);
		while (lua_next(L, 7) != 0) {
			const char *key = luaL_checkstring(L, -2);
			if (!mir_strcmp(key, "lpDesktop")) {
				si.lpDesktop = mir_strdup(luaL_checkstring(L, -1));
			}
			else if (!mir_strcmp(key, "lpTitle")) {
				si.lpTitle = mir_strdup(luaL_checkstring(L, -1));
			}
			else if (!mir_strcmp(key, "dwX")) {
				si.dwX = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "dwY")) {
				si.dwY = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "dwXSize")) {
				si.dwXSize = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "dwYSize")) {
				si.dwYSize = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "dwXCountChars")) {
				si.dwXCountChars = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "dwYCountChars")) {
				si.dwYCountChars = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "dwFillAttribute")) {
				si.dwFillAttribute = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "dwFlags")) {
				si.dwFlags = (DWORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "wShowWindow")) {
				si.wShowWindow = (WORD)luaL_checknumber(L, -1);
			}
			else if (!mir_strcmp(key, "hStdInput")) {
				long h = (long)luaL_checknumber(L, -1);
				si.hStdInput = (HANDLE)h;
			}
			else if (!mir_strcmp(key, "hStdOutput")) {
				long h = (long)luaL_checknumber(L, -1);
				si.hStdOutput = (HANDLE)h;
			}
			else if (!mir_strcmp(key, "hStdError")) {
				long h = (long)luaL_checknumber(L, -1);
				si.hStdError = (HANDLE)h;
			}
			lua_pop(L, 1);
		}
	}


	brc = CreateProcessA(an, (char *)cl, &psa, &tsa, ih, cf, env, cd, &si, &pi);

	if (si.lpDesktop != NULL)
		mir_free(si.lpDesktop);
	if (si.lpTitle != NULL)
		mir_free(si.lpTitle);

	lua_pushnumber(L, brc);
	if (brc) 
	{
		lua_pushnumber(L, (intptr_t)(pi.hProcess));
		lua_pushnumber(L, (intptr_t)(pi.hThread));
		lua_pushnumber(L, pi.dwProcessId);
		lua_pushnumber(L, pi.dwThreadId);
	}
	else 
	{
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return(5);
}

/****if* luaw32/global_GetTempFileName
* NAME
*  global_GetTempFileName
* FUNCTION
*  Win32 GetTempFileName.
* INPUTS
*  L: Lua state
*  stack[1]: specifies the directory path for the file name
*  stack[2]: Specifies the type of access to the object
*  stack[3]: Specifies an unsigned integer that the function
*            converts to a hexadecimal string for use in creating
*            the temporary file name
* RESULT
*  stack[1]: Unique numeric value or 0 if error
*  stack[2]: Temporary file name or nil if error
* SOURCE
*/

static int global_GetTempFileName(lua_State *L) 
{
	char tfn[MAX_PATH];
	const char *path = luaL_checkstring(L, 1);
	const char *pfx = luaL_checkstring(L, 2);
	UINT unique = (UINT)luaL_checknumber(L, 3);
	UINT rc = GetTempFileNameA(path, pfx, unique, tfn);

	lua_pushnumber(L, rc);
	luaM_PushStringIf(L, tfn, rc);
	return(2);
}
/***/

/****if* luaw32/global_GetTempPath
* NAME
*  global_GetTempPath
* FUNCTION
*  Win32 GetTempPath.
* INPUTS
*  L: Lua state
* RESULT
*  stack[1]: Unique numeric value or 0 if error
*  stack[2]: Path of the directory designated for temporary files
* SOURCE
*/

static int global_GetTempPath(lua_State *L) 
{
	char tfn[MAX_PATH];
	DWORD rc = GetTempPathA(MAX_PATH, tfn);
	lua_pushnumber(L, rc);
	luaM_PushStringIf(L, tfn, rc);
	return(2);
}
/***/

static int global_CreateNamedPipe(lua_State *L)
{
	LPCSTR lpName = luaL_checkstring(L, 1);
	DWORD dwOpenMode = luaL_checknumber(L, 2);
	DWORD dwPipeMode = luaL_checknumber(L, 3);
	DWORD nMaxInstances = luaL_checknumber(L, 4);
	DWORD nOutBufferSize = luaL_checknumber(L, 5);
	DWORD nInBufferSize = luaL_checknumber(L, 6);
	DWORD nDefaultTimeOut = luaL_checknumber(L, 7);
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, false };

	HANDLE hPipe = CreateNamedPipeA(lpName, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize, nDefaultTimeOut, &sa);
	lua_pushnumber(L, (intptr_t)hPipe);
	return 1;
}

/****if* luaw32/global_CreateFile
* NAME
*  global_CreateFile
* FUNCTION
*  Win32 CreateFile.
* INPUTS
*  L: Lua state
*  stack[1]: Name of the object to create or open
*  stack[2]: Specifies the type of access to the object
*  stack[3]: Specifies how the object can be shared
*  stack[4]: Table:
*            .bInheritHandle: determines whether the returned handle
*                             can be inherited by child processes
*  stack[5]: Specifies which action to take on files that exist
*  stack[6]: Specifies the file attributes and flags for the file
*  stack[7]: Specifies a handle with GENERIC_READ access to a
*            template file
* RESULT
*  stack[1]: Handle
* SOURCE
*/

static int global_CreateFile(lua_State *L) 
{
	SECURITY_ATTRIBUTES sa;
	HANDLE hFile;
	const char *name = luaL_checkstring(L, 1);
	DWORD da = (DWORD)luaL_checknumber(L, 2);
	DWORD sm = (DWORD)luaL_checknumber(L, 3);
	DWORD cd = (DWORD)luaL_checknumber(L, 5);
	DWORD fa = (DWORD)luaL_checknumber(L, 6);
	intptr_t th = 0;

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE;
	if (lua_istable(L, 4)) {
		lua_pushstring(L, "bInheritHandle");
		lua_gettable(L, 4);
		if (!lua_isnil(L, -1))
			sa.bInheritHandle = (BOOL)luaL_checknumber(L, -1);
		lua_pop(L, 1);
	}
	if (!lua_isnil(L, 7))
		th = luaL_checknumber(L, 7);

	hFile = CreateFileA(name, da, sm, &sa, cd, fa, (HANDLE)th);
	lua_pushnumber(L, (intptr_t)hFile);

	return(1);
}
/***/

/****if* luaw32/global_ReadFile
* NAME
*  global_ReadFile
* FUNCTION
*  Win32 ReadFile.
* INPUTS
*  L: Lua state
*  stack[1]: Handle to the file to be read
*  stack[2]: Specifies the number of bytes to be read from the file
* RESULT
*  stack[1]: True if ok.
*  stack[2]: Buffer read or nil if error
* SOURCE
*/

static int global_ReadFile(lua_State *L) {
	DWORD bread;
	char *buf;
	BOOL brc = FALSE;
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	DWORD btoread = (DWORD)luaL_checknumber(L, 2);

	buf = (char*)mir_alloc(btoread);
	if (buf != NULL) 
	{
		brc = ReadFile(h, buf, btoread, &bread, NULL);
		lua_pushboolean(L, TRUE);
		lua_pushlstring(L, buf, bread);
		mir_free(buf);
	}
	else 
	{
		lua_pushboolean(L, FALSE);
		lua_pushnil(L);
	}

	return(2);
}
/***/

/****if* luaw32/global_WriteFile
* NAME
*  global_WriteFile
* FUNCTION
*  Win32 WriteFile.
* INPUTS
*  L: Lua state
*  stack[1]: Handle to the file to be written to
*  stack[2]: Buffer containing the data to be written to the file
* RESULT
*  stack[1]: True if ok.
*  stack[2]: Number of bytes written or nil if error
* SOURCE
*/

static int global_WriteFile(lua_State *L) {
	DWORD bwrite;
	DWORD btowrite;
	BOOL brc;
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	const char *buf = luaL_checklstring(L, 2, (size_t*)&btowrite);

	brc = WriteFile(h, buf, btowrite, &bwrite, NULL);
	lua_pushboolean(L, brc);
	luaM_PushNumberIf(L, bwrite, brc);
	return(2);
}
/***/

static int global_DeleteFile(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	BOOL result = DeleteFileA(path);
	lua_pushboolean(L, result);
	return 1;
}

static int global_WaitForSingleObject(lua_State *L) {
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	DWORD t = (DWORD)luaL_checknumber(L, 2);

	lua_pushnumber(L, WaitForSingleObject(h, t));
	return(1);
}

static int global_WaitForMultipleObjects(lua_State *L) 
{
	HANDLE ha[64];
	DWORD c = 0;
	BOOL wa = (BOOL)luaL_checknumber(L, 2);
	DWORD t = (DWORD)luaL_checknumber(L, 3);

	if (lua_istable(L, 1)) 
	{
		for (; c < 64; c++) 
		{
			lua_pushnumber(L, c + 1);
			lua_gettable(L, 1);
			if (lua_isnil(L, -1))
				break;
			HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, -1);
			ha[c] = h;
		}
	}
	lua_pushnumber(L, WaitForMultipleObjects(c, ha, wa, t));
	return(1);
}

static int global_TerminateProcess(lua_State *L) {
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	DWORD ec = (DWORD)luaL_checknumber(L, 2);

	lua_pushboolean(L, TerminateProcess(h, ec));
	return(1);
}

static int global_GetExitCodeProcess(lua_State *L) 
{
	BOOL ok;
	DWORD ec;
	HANDLE h = (HANDLE)(intptr_t)luaL_checknumber(L, 1);

	ok = GetExitCodeProcess(h, &ec);
	lua_pushnumber(L, ok);
	lua_pushnumber(L, ec);

	return(2);
}

static int global_GetCurrentThreadId(lua_State *L) 
{
	lua_pushnumber(L, GetCurrentThreadId());
	return(1);
}

static int global_RegisterWindowMessage(lua_State *L) 
{
	const char *msg = luaL_checkstring(L, 1);
	lua_pushnumber(L, RegisterWindowMessageA(msg));
	return(1);
}

static int global_RegQueryValueEx(lua_State *L) 
{
	long rv;
	HKEY hsk;
	DWORD type;
	DWORD dwdata;
	DWORD len;
	char *szdata;
	HKEY hkey = (HKEY)(intptr_t)luaL_checknumber(L, 1);
	const char *subkey = luaL_checkstring(L, 2);
	const char *valuename = luaL_checkstring(L, 3);

	rv = RegOpenKeyExA(hkey, subkey, 0, KEY_QUERY_VALUE, &hsk);
	if (rv == ERROR_SUCCESS) {
		len = sizeof(dwdata);
		rv = RegQueryValueExA(hsk, valuename, NULL, &type, (LPBYTE)&dwdata, &len);
		if ((rv == ERROR_SUCCESS) || (rv == ERROR_MORE_DATA)) {
			switch (type) {
			case REG_DWORD_BIG_ENDIAN:
			case REG_DWORD:
				lua_pushnumber(L, dwdata);
				break;
			case REG_EXPAND_SZ:
			case REG_BINARY:
			case REG_MULTI_SZ:
			case REG_SZ:
				if (rv == ERROR_MORE_DATA) {
					szdata = (char*)mir_alloc(len);
					if (szdata == NULL) {
						lua_pushnil(L);
					}
					else {
						rv = RegQueryValueExA(hsk, valuename, NULL, &type, (LPBYTE)szdata, &len);
						if (rv == ERROR_SUCCESS)
							lua_pushlstring(L, szdata, len);
						else
							lua_pushnil(L);
						mir_free(szdata);
					}
				}
				else
					lua_pushlstring(L, (const char *)&dwdata, len);
				break;
			default:
				lua_pushnil(L);
			}
		}
		else
			lua_pushnil(L);
		RegCloseKey(hsk);
	}
	else
		lua_pushnil(L);

	return 1;
}

static int global_RegSetValueEx(lua_State *L) {
	long rv;
	HKEY hsk;
	DWORD type;
	DWORD dwdata;
	DWORD len = 0;
	char *szdata = NULL;
	HKEY hkey = (HKEY)(intptr_t)luaL_checknumber(L, 1);
	const char *subkey = luaL_checkstring(L, 2);
	const char *valuename = luaL_checkstring(L, 3);

	if (lua_isnumber(L, 4)) {
		dwdata = (DWORD)luaL_checknumber(L, 4);
		type = (DWORD)luaL_optnumber(L, 5, REG_DWORD);
	}
	else {
		szdata = (char *)luaL_checklstring(L, 4, (size_t*)&len);
		type = (DWORD)luaL_optnumber(L, 5, REG_SZ);
	}

	rv = RegCreateKeyExA(hkey, subkey, 0, "", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hsk, NULL);
	if (rv == ERROR_SUCCESS) {
		if (szdata == NULL)
			rv = RegSetValueExA(hsk, valuename, 0, type, (CONST BYTE *) &dwdata, sizeof(dwdata));
		else
			rv = RegSetValueExA(hsk, valuename, 0, type, (CONST BYTE *) szdata, len + 1);
		lua_pushboolean(L, rv == ERROR_SUCCESS);
		RegCloseKey(hsk);
	}
	else
		lua_pushboolean(L, 0);

	return 1;
}

static int global_RegDeleteValue(lua_State *L) {
	long rv;
	HKEY hsk;
	HKEY hkey = (HKEY)(intptr_t)luaL_checknumber(L, 1);
	const char *subkey = luaL_checkstring(L, 2);
	const char *valuename = luaL_checkstring(L, 3);

	rv = RegOpenKeyExA(hkey, subkey, 0, KEY_SET_VALUE, &hsk);
	if (rv == ERROR_SUCCESS) 
	{
		rv = RegDeleteValueA(hsk, valuename);
		lua_pushboolean(L, rv == ERROR_SUCCESS);
		RegCloseKey(hsk);
	}
	else
		lua_pushboolean(L, 0);

	return 1;
}

static int global_RegDeleteKey(lua_State *L) {
	HKEY hkey = (HKEY)(intptr_t)luaL_checknumber(L, 1);
	const char *subkey = luaL_checkstring(L, 2);
	lua_pushboolean(L, RegDeleteKeyA(hkey, subkey) == ERROR_SUCCESS);
	return 1;
}

static int global_RegEnumKeyEx(lua_State *L) 
{
	long rv;
	HKEY hsk;
	DWORD len;
	DWORD index;
	char name[256];
	FILETIME ft;
	HKEY hkey = (HKEY)(intptr_t)luaL_checknumber(L, 1);
	const char *subkey = luaL_checkstring(L, 2);

	rv = RegOpenKeyExA(hkey, subkey, 0, KEY_ENUMERATE_SUB_KEYS, &hsk);
	if (rv == ERROR_SUCCESS) 
	{
		lua_newtable(L);
		for (index = 0 ; ; index++) 
		{
			len = sizeof(name);
			if (RegEnumKeyExA(hsk, index, name, &len,
				NULL, NULL, NULL, &ft) != ERROR_SUCCESS)
				break;
			lua_pushnumber(L, index + 1);
			lua_pushstring(L, name);
			lua_settable(L, -3);
		}
		RegCloseKey(hsk);
	}
	else
		lua_pushnil(L);

	return 1;
}

static int global_RegEnumValue(lua_State *L) {
	long rv;
	HKEY hsk;
	DWORD len;
	DWORD index;
	char name[256];
	HKEY hkey = (HKEY)(intptr_t)luaL_checknumber(L, 1);
	const char *subkey = luaL_checkstring(L, 2);

	rv = RegOpenKeyExA(hkey, subkey, 0, KEY_QUERY_VALUE, &hsk);
	if (rv == ERROR_SUCCESS) 
	{
		lua_newtable(L);
		for (index = 0;; index++) 
		{
			len = sizeof(name);
			if (RegEnumValueA(hsk, index, name, &len,
				NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
				break;
			lua_pushnumber(L, index + 1);
			lua_pushstring(L, name);
			lua_settable(L, -3);
		}
		RegCloseKey(hsk);
	}
	else
		lua_pushnil(L);

	return 1;
}

static int global_SetCurrentDirectory(lua_State *L) 
{
	DWORD le;
	BOOL ok;
	const char *pname = luaL_checkstring(L, 1);

	ok = SetCurrentDirectoryA(pname);
	if (!ok) {
		le = GetLastError();
		lua_pushboolean(L, ok);
		lua_pushnumber(L, le);
	}
	else {
		lua_pushboolean(L, ok);
		lua_pushnil(L);
	}

	return 2;
}

static int global_SHDeleteKey(lua_State *L) {
	HKEY hkey = (HKEY)(intptr_t)luaL_checknumber(L, 1);
	const char *subkey = luaL_checkstring(L, 2);
	lua_pushboolean(L, SHDeleteKeyA(hkey, subkey) == ERROR_SUCCESS);
	return 1;
}

static int global_Sleep(lua_State *L) 
{
	DWORD tosleep = (DWORD)luaL_checknumber(L, 1);
	Sleep(tosleep);
	return 0;
}

static int global_GetVersion(lua_State *L) 
{
	lua_pushnumber(L, GetVersion());
	return 1;
}

static void pushFFTime(lua_State *L, FILETIME *ft) 
{
	SYSTEMTIME st;
	FileTimeToSystemTime(ft, &st);
	lua_newtable(L);
	lua_pushstring(L, "Year");
	lua_pushnumber(L, st.wYear);
	lua_rawset(L, -3);
	lua_pushstring(L, "Month");
	lua_pushnumber(L, st.wMonth);
	lua_rawset(L, -3);
	lua_pushstring(L, "DayOfWeek");
	lua_pushnumber(L, st.wDayOfWeek);
	lua_rawset(L, -3);
	lua_pushstring(L, "Day");
	lua_pushnumber(L, st.wDay);
	lua_rawset(L, -3);
	lua_pushstring(L, "Hour");
	lua_pushnumber(L, st.wHour);
	lua_rawset(L, -3);
	lua_pushstring(L, "Minute");
	lua_pushnumber(L, st.wMinute);
	lua_rawset(L, -3);
	lua_pushstring(L, "Second");
	lua_pushnumber(L, st.wSecond);
	lua_rawset(L, -3);
	lua_pushstring(L, "Milliseconds");
	lua_pushnumber(L, st.wMilliseconds);
	lua_rawset(L, -3);
}
static void pushFFData(lua_State *L, WIN32_FIND_DATAA *wfd) 
{
	lua_newtable(L);
	lua_pushstring(L, "FileAttributes");
	lua_pushnumber(L, wfd->dwFileAttributes);
	lua_rawset(L, -3);
	lua_pushstring(L, "CreationTime");
	pushFFTime(L, &(wfd->ftCreationTime));
	lua_rawset(L, -3);
	lua_pushstring(L, "LastAccessTime");
	pushFFTime(L, &(wfd->ftLastAccessTime));
	lua_rawset(L, -3);
	lua_pushstring(L, "LastWriteTime");
	pushFFTime(L, &(wfd->ftLastWriteTime));
	lua_rawset(L, -3);
	lua_pushstring(L, "FileSizeHigh");
	lua_pushnumber(L, wfd->nFileSizeHigh);
	lua_rawset(L, -3);
	lua_pushstring(L, "FileSizeLow");
	lua_pushnumber(L, wfd->nFileSizeLow);
	lua_rawset(L, -3);
	lua_pushstring(L, "FileName");
	lua_pushstring(L, wfd->cFileName);
	lua_rawset(L, -3);
	lua_pushstring(L, "AlternateFileName");
	lua_pushstring(L, wfd->cAlternateFileName);
	lua_rawset(L, -3);
}

static int global_FindFirstFile(lua_State *L) 
{
	WIN32_FIND_DATAA wfd;
	HANDLE hfd;
	const char *fname = luaL_checkstring(L, 1);

	hfd = FindFirstFileA(fname, &wfd);
	if (hfd == NULL) {
		lua_pushnumber(L, 0);
		lua_pushnil(L);
	}
	else {
		lua_pushnumber(L, (long)hfd);
		pushFFData(L, &wfd);
	}

	return(2);
}

static int global_FindNextFile(lua_State *L) 
{
	WIN32_FIND_DATAA wfd;
	BOOL ok;
	HANDLE lfd = (HANDLE)(intptr_t)luaL_checknumber(L, 1);

	ok = FindNextFileA(lfd, &wfd);
	lua_pushboolean(L, ok);

	if (!ok) 
		lua_pushnil(L);
	else 
		pushFFData(L, &wfd);
	return(2);
}

static int global_FindClose(lua_State *L) 
{
	HANDLE lfd = (HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, FindClose(lfd));
	return(2);
}

static void FreePIDL(LPITEMIDLIST idl) 
{
	IMalloc *m = nullptr;
	SHGetMalloc(&m);
	if (m != nullptr)
	{
		m->Free(idl);
		m->Release();
	}
}

static int global_SHGetSpecialFolderLocation(lua_State *L) 
{
	LPITEMIDLIST idl;
	char out[MAX_PATH];
	int ifolder = (int)luaL_checknumber(L, 1);

	if (SHGetSpecialFolderLocation(GetDesktopWindow(),
		ifolder, &idl) != NOERROR) {
		lua_pushnil(L);
	}
	else {
		if (!SHGetPathFromIDListA(idl, out))
			lua_pushnil(L);
		else
			lua_pushstring(L, out);
		FreePIDL(idl);
	}

	return 1;
}

static int global_GetFullPathName(lua_State *L) {
	DWORD le;
	DWORD rc;
	char fpname[MAX_PATH];
	char *fpart;
	const char *pname = luaL_checkstring(L, 1);

	rc = GetFullPathNameA(pname, sizeof(fpname), fpname, &fpart);
	if (!rc) 
	{
		le = GetLastError();
		lua_pushnumber(L, 0);
		lua_pushnil(L);
		lua_pushnumber(L, le);
	}
	else 
	{
		lua_pushnumber(L, rc);
		lua_pushstring(L, fpname);
		lua_pushnil(L);
	}

	return 3;
}

static int global_IsUserAdmin(lua_State *L) 
{
	BOOL b;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup;

	b = AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&AdministratorsGroup);
	if (b) 
	{
		if (!CheckTokenMembership(NULL, AdministratorsGroup, &b))
			b = FALSE;
		FreeSid(AdministratorsGroup);
	}
	lua_pushboolean(L, b);
	return 1;
}


static int global_OpenProcess(lua_State *L) {
	HANDLE h;
	DWORD da = (DWORD)luaL_checknumber(L, 1);
	BOOL ih = (BOOL)luaL_checknumber(L, 2);
	DWORD pid = (DWORD)luaL_checknumber(L, 3);

	h = OpenProcess(da, ih, pid);
	if (h != NULL)
		lua_pushnumber(L, (long)h);
	else
		lua_pushnil(L);

	return 1;
}

static int global_IsRunning(lua_State *L) {
	HANDLE h;
	BOOL b = FALSE;
	DWORD pid = (DWORD)luaL_checknumber(L, 1);

	h = OpenProcess(SYNCHRONIZE, FALSE, pid);
	if (h != NULL) {
		b = TRUE;
		CloseHandle(h);
	}

	lua_pushboolean(L, b);

	return 1;
}

static int global_GetWindowThreadProcessId(lua_State *L) 
{
	HWND h = (HWND)(intptr_t)luaL_checknumber(L, 1);
	DWORD tid, pid;

	tid = GetWindowThreadProcessId(h, &pid);
	lua_pushnumber(L, tid);
	lua_pushnumber(L, pid);
	return 2;
}

static int global_OpenSCManager(lua_State *L) 
{
	lua_pushnumber(L, (intptr_t)OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
	return 1;
}

static int global_OpenService(lua_State *L) {
	SC_HANDLE h;
	SC_HANDLE scm = (SC_HANDLE)(intptr_t)luaL_checknumber(L, 1);
	const char *sname = luaL_checkstring(L, 2);

	h = OpenServiceA(scm, sname, SERVICE_ALL_ACCESS);
	lua_pushnumber(L, (intptr_t)h);

	return 1;
}

static int global_CloseServiceHandle(lua_State *L) {
	SC_HANDLE h = (SC_HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, CloseServiceHandle(h));
	return 1;
}

static int global_QueryServiceStatus(lua_State *L) {
	SERVICE_STATUS ss;
	BOOL brc;
	SC_HANDLE h = (SC_HANDLE)(intptr_t)luaL_checknumber(L, 1);

	brc = QueryServiceStatus(h, &ss);
	lua_pushboolean(L, brc);
	if (brc) 
	{
		lua_pushnumber(L, (ss.dwServiceType));
		lua_pushnumber(L, (ss.dwCurrentState));
		lua_pushnumber(L, (ss.dwControlsAccepted));
		lua_pushnumber(L, (ss.dwWin32ExitCode));
		lua_pushnumber(L, (ss.dwServiceSpecificExitCode));
		lua_pushnumber(L, (ss.dwCheckPoint));
		lua_pushnumber(L, (ss.dwWaitHint));
		return 8;
	}
	else
		return 1;
}

static int global_QueryServiceConfig(lua_State *L) {
	union {
		QUERY_SERVICE_CONFIGA sc;
		char buf[4096];
	} storage;
	BOOL brc;
	DWORD needed = 0, errcode = 0;
	SC_HANDLE h = (SC_HANDLE)(intptr_t)luaL_checknumber(L, 1);

	brc = QueryServiceConfig(h, (LPQUERY_SERVICE_CONFIG)&storage, sizeof(storage), &needed);
	if (!brc) {
		errcode = GetLastError();
	}

	lua_pushboolean(L, brc);
	if (brc) {
		// todo: add other values, if needed
		lua_pushnumber(L, (long)(storage.sc.dwServiceType));
		lua_pushnumber(L, (long)(storage.sc.dwStartType));
		lua_pushnumber(L, (long)(storage.sc.dwErrorControl));
		lua_pushstring(L, storage.sc.lpBinaryPathName);
		lua_pushstring(L, storage.sc.lpDisplayName);
		return 6;
	}
	else {
		lua_pushnumber(L, (long)(errcode));
		return 2;
	}
}

static int global_ControlService(lua_State *L) {
	SERVICE_STATUS ss;
	BOOL brc;
	SC_HANDLE h = (SC_HANDLE)(intptr_t)luaL_checknumber(L, 1);
	DWORD c = (DWORD)luaL_checknumber(L, 2);

	brc = ControlService(h, c, &ss);
	lua_pushboolean(L, brc);

	return 1;
}

static int global_DeleteService(lua_State *L) 
{
	SC_HANDLE h = (SC_HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, DeleteService(h));
	return 1;
}

static int global_StartService(lua_State *L) 
{
	SC_HANDLE h = (SC_HANDLE)(intptr_t)luaL_checknumber(L, 1);
	lua_pushboolean(L, StartService(h, 0, NULL));

	return 1;
}

static int global_mciSendString(lua_State *L) 
{
	const char *cmd = luaL_checkstring(L, 1);  // only one string parameter is used
	DWORD lrc = mciSendStringA(cmd, NULL, 0, NULL);
	lua_pushnumber(L, lrc);
	return(1);
}

static int global_MessageBeep(lua_State *L) {
	const UINT uType = luaL_checkinteger(L, 1);
	lua_pushboolean(L, MessageBeep(uType));
	return 1;
}

static int global_Beep(lua_State *L) 
{
	const DWORD dwFreq = luaL_checkinteger(L, 1);
	const DWORD dwDuration = luaL_checkinteger(L, 2);
	lua_pushboolean(L, Beep(dwFreq, dwDuration));
	return 1;
}

static int global_CoInitialize(lua_State *L) 
{
	HRESULT lrc = CoInitialize(NULL);
	lua_pushinteger(L, lrc);
	return(1);
}

static int global_CoUninitialize(lua_State *) 
{
	CoUninitialize();
	return(0);
}

static luaM_consts consts[] = 
{
	{ "TRUE", TRUE },
	{ "FALSE", FALSE },
	{ "INVALID_HANDLE_VALUE", (intptr_t)INVALID_HANDLE_VALUE },
	{ "INFINITE", INFINITE },
	{ "EVENT_ALL_ACCESS", EVENT_ALL_ACCESS },
	{ "EVENT_MODIFY_STATE", EVENT_MODIFY_STATE },
	{ "SYNCHRONIZE", SYNCHRONIZE },
	{ "MUTEX_ALL_ACCESS", MUTEX_ALL_ACCESS },
	{ "SEMAPHORE_ALL_ACCESS", SEMAPHORE_ALL_ACCESS },
	{ "SEMAPHORE_MODIFY_STATE", SEMAPHORE_MODIFY_STATE },
	{ "WAIT_OBJECT_0", WAIT_OBJECT_0 },
	{ "WAIT_ABANDONED_0", WAIT_ABANDONED_0 },
	{ "WAIT_ABANDONED", WAIT_ABANDONED },
	{ "WAIT_TIMEOUT", WAIT_TIMEOUT },
	{ "WAIT_FAILED", WAIT_FAILED },
	{ "CREATE_DEFAULT_ERROR_MODE", CREATE_DEFAULT_ERROR_MODE },
	{ "CREATE_NEW_CONSOLE", CREATE_NEW_CONSOLE },
	{ "CREATE_NEW_PROCESS_GROUP", CREATE_NEW_PROCESS_GROUP },
	{ "CREATE_SUSPENDED", CREATE_SUSPENDED },
	{ "CREATE_UNICODE_ENVIRONMENT", CREATE_UNICODE_ENVIRONMENT },
	{ "DETACHED_PROCESS", DETACHED_PROCESS },
	{ "HIGH_PRIORITY_CLASS", HIGH_PRIORITY_CLASS },
	{ "IDLE_PRIORITY_CLASS", IDLE_PRIORITY_CLASS },
	{ "NORMAL_PRIORITY_CLASS", NORMAL_PRIORITY_CLASS },
	{ "REALTIME_PRIORITY_CLASS", REALTIME_PRIORITY_CLASS },
	{ "FOREGROUND_BLUE", FOREGROUND_BLUE },
	{ "FOREGROUND_GREEN", FOREGROUND_GREEN },
	{ "FOREGROUND_RED", FOREGROUND_RED },
	{ "FOREGROUND_INTENSITY", FOREGROUND_INTENSITY },
	{ "BACKGROUND_BLUE", BACKGROUND_BLUE },
	{ "BACKGROUND_GREEN", BACKGROUND_GREEN },
	{ "BACKGROUND_RED", BACKGROUND_RED },
	{ "BACKGROUND_INTENSITY", BACKGROUND_INTENSITY },
	{ "STARTF_FORCEONFEEDBACK", STARTF_FORCEONFEEDBACK },
	{ "STARTF_FORCEOFFFEEDBACK", STARTF_FORCEOFFFEEDBACK },
	{ "STARTF_RUNFULLSCREEN", STARTF_RUNFULLSCREEN },
	{ "STARTF_USECOUNTCHARS", STARTF_USECOUNTCHARS },
	{ "STARTF_USEFILLATTRIBUTE", STARTF_USEFILLATTRIBUTE },
	{ "STARTF_USEPOSITION", STARTF_USEPOSITION },
	{ "STARTF_USESHOWWINDOW", STARTF_USESHOWWINDOW },
	{ "STARTF_USESIZE", STARTF_USESIZE },
	{ "STARTF_USESTDHANDLES", STARTF_USESTDHANDLES },
	{ "SW_HIDE", SW_HIDE },
	{ "SW_MAXIMIZE", SW_MAXIMIZE },
	{ "SW_MINIMIZE", SW_MINIMIZE },
	{ "SW_RESTORE", SW_RESTORE },
	{ "SW_SHOW", SW_SHOW },
	{ "SW_SHOWDEFAULT", SW_SHOWDEFAULT },
	{ "SW_SHOWMAXIMIZED", SW_SHOWMAXIMIZED },
	{ "SW_SHOWMINIMIZED", SW_SHOWMINIMIZED },
	{ "SW_SHOWMINNOACTIVE", SW_SHOWMINNOACTIVE },
	{ "SW_SHOWNA", SW_SHOWNA },
	{ "SW_SHOWNOACTIVATE", SW_SHOWNOACTIVATE },
	{ "SW_SHOWNORMAL", SW_SHOWNORMAL },
	{ "GENERIC_READ", GENERIC_READ },
	{ "GENERIC_WRITE", GENERIC_WRITE },
	{ "MAXIMUM_ALLOWED", MAXIMUM_ALLOWED },
	{ "GENERIC_EXECUTE", GENERIC_EXECUTE },
	{ "GENERIC_ALL", GENERIC_ALL },
	{ "FILE_SHARE_READ", FILE_SHARE_READ },
	{ "FILE_SHARE_WRITE", FILE_SHARE_WRITE },
	{ "CREATE_NEW", CREATE_NEW },
	{ "CREATE_ALWAYS", CREATE_ALWAYS },
	{ "OPEN_EXISTING", OPEN_EXISTING },
	{ "OPEN_ALWAYS", OPEN_ALWAYS },
	{ "TRUNCATE_EXISTING", TRUNCATE_EXISTING },
	{ "FILE_ATTRIBUTE_ARCHIVE", FILE_ATTRIBUTE_ARCHIVE },
	{ "FILE_ATTRIBUTE_ENCRYPTED", FILE_ATTRIBUTE_ENCRYPTED },
	{ "FILE_ATTRIBUTE_HIDDEN", FILE_ATTRIBUTE_HIDDEN },
	{ "FILE_ATTRIBUTE_NORMAL", FILE_ATTRIBUTE_NORMAL },
	{ "FILE_ATTRIBUTE_NOT_CONTENT_INDEXED", FILE_ATTRIBUTE_NOT_CONTENT_INDEXED },
	{ "FILE_ATTRIBUTE_OFFLINE", FILE_ATTRIBUTE_OFFLINE },
	{ "FILE_ATTRIBUTE_READONLY", FILE_ATTRIBUTE_READONLY },
	{ "FILE_ATTRIBUTE_SYSTEM", FILE_ATTRIBUTE_SYSTEM },
	{ "FILE_ATTRIBUTE_TEMPORARY", FILE_ATTRIBUTE_TEMPORARY },
	{ "FILE_FLAG_FIRST_PIPE_INSTANCE", FILE_FLAG_FIRST_PIPE_INSTANCE },
	{ "FILE_FLAG_WRITE_THROUGH", FILE_FLAG_WRITE_THROUGH },
	{ "FILE_FLAG_OVERLAPPED", FILE_FLAG_OVERLAPPED },
	{ "FILE_FLAG_NO_BUFFERING", FILE_FLAG_NO_BUFFERING },
	{ "FILE_FLAG_RANDOM_ACCESS", FILE_FLAG_RANDOM_ACCESS },
	{ "FILE_FLAG_SEQUENTIAL_SCAN", FILE_FLAG_SEQUENTIAL_SCAN },
	{ "FILE_FLAG_DELETE_ON_CLOSE", FILE_FLAG_DELETE_ON_CLOSE },
	{ "FILE_FLAG_POSIX_SEMANTICS", FILE_FLAG_POSIX_SEMANTICS },
	{ "FILE_FLAG_OPEN_REPARSE_POINT", FILE_FLAG_OPEN_REPARSE_POINT },
	{ "FILE_FLAG_OPEN_NO_RECALL", FILE_FLAG_OPEN_NO_RECALL },
	{ "SECURITY_ANONYMOUS", SECURITY_ANONYMOUS },
	{ "SECURITY_IDENTIFICATION", SECURITY_IDENTIFICATION },
	{ "SECURITY_IMPERSONATION", SECURITY_IMPERSONATION },
	{ "SECURITY_DELEGATION", SECURITY_DELEGATION },
	{ "SECURITY_CONTEXT_TRACKING", SECURITY_CONTEXT_TRACKING },
	{ "SECURITY_EFFECTIVE_ONLY", SECURITY_EFFECTIVE_ONLY },
	{ "PM_REMOVE", PM_REMOVE },
	{ "PM_NOREMOVE", PM_NOREMOVE },

	{ "PIPE_ACCESS_DUPLEX", PIPE_ACCESS_DUPLEX },
	{ "PIPE_ACCESS_INBOUND", PIPE_ACCESS_INBOUND },
	{ "PIPE_ACCESS_OUTBOUND", PIPE_ACCESS_OUTBOUND },
	{ "PIPE_TYPE_BYTE", PIPE_TYPE_BYTE },
	{ "PIPE_TYPE_MESSAGE", PIPE_TYPE_MESSAGE },
	{ "PIPE_READMODE_BYTE", PIPE_READMODE_BYTE },
	{ "PIPE_READMODE_MESSAGE", PIPE_READMODE_MESSAGE },
	{ "PIPE_WAIT", PIPE_WAIT },
	{ "PIPE_NOWAIT", PIPE_NOWAIT },
	{ "PIPE_ACCEPT_REMOTE_CLIENTS", PIPE_ACCEPT_REMOTE_CLIENTS },
	{ "PIPE_REJECT_REMOTE_CLIENTS", PIPE_REJECT_REMOTE_CLIENTS },
	{ "WRITE_DAC", WRITE_DAC },
	{ "WRITE_OWNER", WRITE_OWNER },
	{ "ACCESS_SYSTEM_SECURITY", ACCESS_SYSTEM_SECURITY },

	{ "HKEY_CLASSES_ROOT", (intptr_t)HKEY_CLASSES_ROOT },
	{ "HKEY_CURRENT_CONFIG", (intptr_t)HKEY_CURRENT_CONFIG },
	{ "HKEY_CURRENT_USER", (intptr_t)HKEY_CURRENT_USER },
	{ "HKEY_LOCAL_MACHINE", (intptr_t)HKEY_LOCAL_MACHINE },
	{ "HKEY_USERS", (intptr_t)HKEY_USERS },
	{ "REG_BINARY", REG_BINARY },
	{ "REG_DWORD", REG_DWORD },
	{ "REG_DWORD_BIG_ENDIAN", REG_DWORD_BIG_ENDIAN },
	{ "REG_EXPAND_SZ", REG_EXPAND_SZ },
	{ "REG_MULTI_SZ", REG_MULTI_SZ },
	{ "REG_SZ", REG_SZ },
	{ "CSIDL_STARTUP", CSIDL_STARTUP },
	{ "CSIDL_STARTMENU", CSIDL_STARTMENU },
	{ "CSIDL_COMMON_PROGRAMS", CSIDL_COMMON_PROGRAMS },
	{ "CSIDL_COMMON_STARTUP", CSIDL_COMMON_STARTUP },
	{ "CSIDL_COMMON_STARTMENU", CSIDL_COMMON_STARTMENU },
	{ "CSIDL_PROGRAM_FILES", 38 },
	{ "SERVICE_CONTROL_STOP", SERVICE_CONTROL_STOP },
	{ "SERVICE_CONTROL_PAUSE", SERVICE_CONTROL_PAUSE },
	{ "SERVICE_CONTROL_CONTINUE", SERVICE_CONTROL_CONTINUE },
	{ "SERVICE_CONTROL_INTERROGATE", SERVICE_CONTROL_INTERROGATE },
	{ "SERVICE_CONTROL_SHUTDOWN", SERVICE_CONTROL_SHUTDOWN },

	{ "SERVICE_STOPPED", SERVICE_STOPPED },
	{ "SERVICE_START_PENDING", SERVICE_START_PENDING },
	{ "SERVICE_STOP_PENDING", SERVICE_STOP_PENDING },
	{ "SERVICE_RUNNING", SERVICE_RUNNING },
	{ "SERVICE_CONTINUE_PENDING", SERVICE_CONTINUE_PENDING },
	{ "SERVICE_PAUSE_PENDING", SERVICE_PAUSE_PENDING },
	{ "SERVICE_PAUSED", SERVICE_PAUSED },

	{ "SERVICE_BOOT_START", SERVICE_BOOT_START },
	{ "SERVICE_SYSTEM_START", SERVICE_SYSTEM_START },
	{ "SERVICE_AUTO_START", SERVICE_AUTO_START },
	{ "SERVICE_DEMAND_START", SERVICE_DEMAND_START },
	{ "SERVICE_DISABLED", SERVICE_DISABLED },

	{ "MB_ICONASTERISK", MB_ICONASTERISK },
	{ "MB_ICONEXCLAMATION", MB_ICONEXCLAMATION },
	{ "MB_ICONERROR", MB_ICONERROR },
	{ "MB_ICONHAND", MB_ICONHAND },
	{ "MB_ICONINFORMATION", MB_ICONINFORMATION },
	{ "MB_ICONQUESTION", MB_ICONQUESTION },
	{ "MB_ICONSTOP", MB_ICONSTOP },
	{ "MB_ICONWARNING", MB_ICONWARNING },
	{ "MB_OK", MB_OK },

	{ "BM_CLICK", BM_CLICK },
	{ NULL, 0 }
};

/***********************************************/

static luaL_Reg winApi[] =
{
	{ "MessageBox", lua_MessageBox },

	{ "ShellExecute", lua_ShellExecute },

	{ "Find", lua_Find },

	{ "GetKeyState", lua_GetKeyState },

	{ "GetIniValue", lua_GetIniValue },
	{ "SetIniValue", lua_SetIniValue },
	{ "DeleteIniValue", lua_DeleteIniValue },

	//{ "GetRegValue", lua_GetRegValue },
	//{ "SetRegValue", lua_SetRegValue },
	//{ "DeleteRegValue", lua_DeleteRegValue },

	{ "FindWindow", global_FindWindow },
	{ "FindWindowEx", global_FindWindowEx },
	{ "SetFocus", global_SetFocus },
	{ "GetWindowText", global_GetWindowText },
	{ "SetWindowText", global_SetWindowText },
	{ "GetWindowRect", global_GetWindowRect },
	{ "SetForegroundWindow", global_SetForegroundWindow },
	{ "PostMessage", global_PostMessage },
	{ "PostThreadMessage", global_PostThreadMessage },
	{ "GetMessage", global_GetMessage },
	{ "PeekMessage", global_PeekMessage },
	{ "ReplyMessage", global_ReplyMessage },
	{ "DispatchMessage", global_DispatchMessage },
	{ "SetTopmost", global_SetTopmost },
	{ "GetLastError", global_GetLastError },
	{ "CloseHandle", global_CloseHandle },
	{ "CreateEvent", global_CreateEvent },
	{ "OpenEvent", global_OpenEvent },
	{ "PulseEvent", global_PulseEvent },
	{ "ResetEvent", global_ResetEvent },
	{ "SetEvent", global_SetEvent },
	{ "CreateMutex", global_CreateMutex },
	{ "OpenMutex", global_OpenMutex },
	{ "ReleaseMutex", global_ReleaseMutex },
	{ "CreateSemaphore", global_CreateSemaphore },
	{ "OpenSemaphore", global_OpenSemaphore },
	{ "ReleaseSemaphore", global_ReleaseSemaphore },
	{ "CreateProcess", global_CreateProcess },
	{ "GetTempFileName", global_GetTempFileName },
	{ "GetTempPath", global_GetTempPath },
	{ "CreateNamedPipe", global_CreateNamedPipe },
	{ "CreateFile", global_CreateFile },
	{ "ReadFile", global_ReadFile },
	{ "WriteFile", global_WriteFile },
	{ "DeleteFile", global_DeleteFile },
	{ "TerminateProcess", global_TerminateProcess },
	{ "GetExitCodeProcess", global_GetExitCodeProcess },
	{ "WaitForSingleObject", global_WaitForSingleObject },
	{ "WaitForMultipleObjects", global_WaitForMultipleObjects },
	{ "GetCurrentThreadId", global_GetCurrentThreadId },
	{ "RegisterWindowMessage", global_RegisterWindowMessage },
	{ "RegQueryValueEx", global_RegQueryValueEx },
	{ "RegSetValueEx", global_RegSetValueEx },
	{ "RegDeleteKey", global_RegDeleteKey },
	{ "RegDeleteValue", global_RegDeleteValue },
	{ "RegEnumKeyEx", global_RegEnumKeyEx },
	{ "RegEnumValue", global_RegEnumValue },
	{ "SetCurrentDirectory", global_SetCurrentDirectory },
	{ "SHDeleteKey", global_SHDeleteKey },
	{ "Sleep", global_Sleep },
	{ "GetVersion", global_GetVersion },
	{ "FindFirstFile", global_FindFirstFile },
	{ "FindNextFile", global_FindNextFile },
	{ "FindClose", global_FindClose },
	{ "SHGetSpecialFolderLocation", global_SHGetSpecialFolderLocation },
	{ "GetFullPathName", global_GetFullPathName },
	{ "IsUserAdmin", global_IsUserAdmin },
	{ "OpenProcess", global_OpenProcess },
	{ "IsRunning", global_IsRunning },
	{ "GetWindowThreadProcessId", global_GetWindowThreadProcessId },
	{ "OpenSCManager", global_OpenSCManager },
	{ "OpenService", global_OpenService },
	{ "CloseServiceHandle", global_CloseServiceHandle },
	{ "QueryServiceStatus", global_QueryServiceStatus },
	{ "QueryServiceConfig", global_QueryServiceConfig },
	{ "ControlService", global_ControlService },
	{ "DeleteService", global_DeleteService },
	{ "StartService", global_StartService },
	{ "mciSendString", global_mciSendString },
	{ "MessageBeep", global_MessageBeep },
	{ "Beep", global_Beep },
	{ "CoInitialize", global_CoInitialize },
	{ "CoUninitialize", global_CoUninitialize },
	
	{ NULL, NULL }
};

LUA_WINAPI_LIB luaopen_winapi(lua_State *L)
{
	luaL_newlib(L, winApi);
	
	for (size_t i = 0; consts[i].name != NULL; i++)
	{
		lua_pushstring(L, consts[i].name);
		lua_pushnumber(L, consts[i].value);
		lua_settable(L, -3);
	}

	return 1;
}
