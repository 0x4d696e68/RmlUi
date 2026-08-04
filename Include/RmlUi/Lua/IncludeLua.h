#pragma once

#ifndef RMLUI_LUA_AS_CXX
extern "C" {
#endif

// The standard Lua headers
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#ifndef RMLUI_LUA_AS_CXX
}
#endif

#if LUA_VERSION_NUM < 502
/*
    The plugin is written against the Lua 5.2 auxiliary API. Upstream only ships the
    5.1 fallbacks that PUC Lua and LuaJIT disagree on (lua_absindex, luaL_len and
    friends, in Lua/Utilities.h) - the rest it takes for granted because a 5.1 build is
    assumed to be LuaJIT 2.1, which supplies them. This tree links plain PUC Lua 5.1
    (libsdl/lua), so they are supplied here instead.

    C++ inline, outside the extern "C" block above, and in IncludeLua.h rather than
    Utilities.h because Pairs.h and the proxies reach for luaL_setfuncs without ever
    including Utilities.h. Drop the whole block if lua is ever moved to 5.2+.
*/

	#define LUA_OK 0

inline void luaL_setfuncs(lua_State* L, const luaL_Reg* l, int nup)
{
	luaL_checkstack(L, nup + 1, "too many upvalues");
	for (; l->name != NULL; l++)
	{
		for (int i = 0; i < nup; i++)
		{
			lua_pushvalue(L, -nup);
		}
		lua_pushcclosure(L, l->func, nup);
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);
}

inline lua_Integer lua_tointegerx(lua_State* L, int idx, int* isnum)
{
	const lua_Integer n = lua_tointeger(L, idx);
	if (isnum != NULL)
	{
		// 5.1 has no failure channel, so a real 0 and a failed conversion both return 0
		*isnum = (n != 0 || lua_isnumber(L, idx));
	}
	return n;
}

inline void* luaL_testudata(lua_State* L, int idx, const char* tname)
{
	void* p = lua_touserdata(L, idx);
	if (p == NULL || lua_getmetatable(L, idx) == 0)
	{
		return NULL;
	}
	luaL_getmetatable(L, tname);
	const int bSame = lua_rawequal(L, -1, -2);
	lua_pop(L, 2);
	return bSame ? p : NULL;
}

inline void luaL_traceback(lua_State* L, lua_State* /*L1*/, const char* msg, int level)
{
	// No C-level traceback before 5.2; go through the debug library if it is open,
	// and fall back to the bare message if it is not (the host decides which
	// libraries the state gets).
	lua_getglobal(L, "debug");
	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, "traceback");
		lua_remove(L, -2);
		if (lua_isfunction(L, -1))
		{
			if (msg != NULL)
			{
				lua_pushstring(L, msg);
			}
			else
			{
				lua_pushnil(L);
			}
			lua_pushinteger(L, level);
			lua_call(L, 2, 1);
			return;
		}
	}
	lua_pop(L, 1);
	lua_pushstring(L, msg != NULL ? msg : "");
}
#endif
