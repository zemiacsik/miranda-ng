#include "stdafx.h"

static void MapToTable(lua_State *L, const PROTOCOLDESCRIPTOR* pd)
{
	lua_newtable(L);
	lua_pushliteral(L, "Name");
	lua_pushstring(L, ptrA(mir_utf8encode(pd->szName)));
	lua_settable(L, -3);
	lua_pushliteral(L, "Type");
	lua_pushinteger(L, pd->type);
	lua_settable(L, -3);
}

static int lua_GetProto(lua_State *L)
{
	ptrA name(mir_utf8decodeA(luaL_checkstring(L, 1)));

	PROTOCOLDESCRIPTOR* pd = ::Proto_IsProtocolLoaded(name);

	if (pd)
		MapToTable(L, pd);
	else
		lua_pushnil(L);

	return 1;
}

static int lua_EnumProtos(lua_State *L)
{
	if (!lua_isfunction(L, 1))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}

	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	int count;
	PROTOCOLDESCRIPTOR** protos;
	Proto_EnumProtocols(&count, &protos);

	for (int i = 0; i < count; i++)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		MapToTable(L, protos[i]);
		if (lua_pcall(L, 1, 0, 0))
			CallService(MS_NETLIB_LOG, (WPARAM)hNetlib, (LPARAM)lua_tostring(L, -1));
	}

	luaL_unref(L, LUA_REGISTRYINDEX, ref);
	lua_pushinteger(L, count);

	return 1;
}

static void MapToTable(lua_State *L, const PROTOACCOUNT* pa)
{
	lua_newtable(L);
	lua_pushliteral(L, "InternalName");
	lua_pushstring(L, ptrA(mir_utf8encode(pa->szModuleName)));
	lua_settable(L, -3);
	lua_pushliteral(L, "AccountName");
	lua_pushstring(L, ptrA(mir_utf8encodeT(pa->tszAccountName)));
	lua_settable(L, -3);
	lua_pushliteral(L, "ProtoName");
	lua_pushstring(L, ptrA(mir_utf8encode(pa->szProtoName)));
	lua_settable(L, -3);
	lua_pushliteral(L, "IsEnabled");
	lua_pushboolean(L, pa->bIsEnabled);
	lua_settable(L, -3);
	lua_pushliteral(L, "IsVisible");
	lua_pushboolean(L, pa->bIsVisible);
	lua_settable(L, -3);
	lua_pushliteral(L, "IsVirtual");
	lua_pushboolean(L, pa->bIsVirtual);
	lua_settable(L, -3);
	lua_pushliteral(L, "OldProto");
	lua_pushboolean(L, pa->bOldProto);
	lua_settable(L, -3);
}

static int lua_GetAccount(lua_State *L)
{
	ptrA moduleName(mir_utf8decodeA(luaL_checkstring(L, 1)));

	PROTOACCOUNT* pa = ::Proto_GetAccount(moduleName);

	if (pa)
		MapToTable(L, pa);
	else
		lua_pushnil(L);

	return 1;
}

static int lua_EnumAccounts(lua_State *L)
{
	if (!lua_isfunction(L, 1))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}

	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	int count;
	PROTOACCOUNT** accounts;
	Proto_EnumAccounts(&count, &accounts);

	for (int i = 0; i < count; i++)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		MapToTable(L, accounts[i]);
		if (lua_pcall(L, 1, 0, 0))
			CallService(MS_NETLIB_LOG, (WPARAM)hNetlib, (LPARAM)lua_tostring(L, -1));
	}

	luaL_unref(L, LUA_REGISTRYINDEX, ref);
	lua_pushinteger(L, count);

	return 1;
}

static luaL_Reg protocolsApi[] =
{
	{ "GetProto", lua_GetProto },
	{ "EnumProtos", lua_EnumProtos },

	{ "GetAccount", lua_GetAccount },
	{ "EnumAccounts", lua_EnumAccounts },

	{ NULL, NULL }
};

LUAMOD_API int luaopen_m_protocols(lua_State *L)
{
	luaL_newlib(L, protocolsApi);

	return 1;
}