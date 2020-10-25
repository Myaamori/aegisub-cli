// Copyright (c) 2006, 2007, Niels Martin Hansen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file auto4_lua_progresssink.cpp
/// @brief Lua 5.1-based scripting engine
/// @ingroup scripting
///

#include "auto4_lua.h"
#include "stdio.h"
#include "options.h"

#include <libaegisub/log.h>
#include <libaegisub/lua/utils.h>

using namespace agi::lua;

namespace {
	template<lua_CFunction fn>
	void set_field_to_closure(lua_State *L, const char *name, int ps_idx = -3)
	{
		lua_pushvalue(L, ps_idx);
		lua_pushcclosure(L, exception_wrapper<fn>, 1);
		lua_setfield(L, -2, name);
	}

	void set_field_to_nil(lua_State *L, int idx, const char *name)
	{
		lua_pushnil(L);
		lua_setfield(L, idx, name);
	}
}

namespace Automation4 {
	LuaProgressSink::LuaProgressSink(lua_State *L, agi::ProgressSink *ps, bool allow_config_dialog)
	: L(L)
	{
		auto ud = (agi::ProgressSink**)lua_newuserdata(L, sizeof(agi::ProgressSink*));
		*ud = ps;

		// register progress reporting stuff
		lua_getglobal(L, "aegisub");

		// Create aegisub.progress table
		lua_createtable(L, 0, 5);
		set_field_to_closure<LuaSetProgress>(L, "set");
		set_field_to_closure<LuaSetTask>(L, "task");
		set_field_to_closure<LuaSetTitle>(L, "title");
		set_field_to_closure<LuaGetCancelled>(L, "is_cancelled");
		lua_setfield(L, -2, "progress");

		// Create aegisub.debug table
		lua_createtable(L, 0, 4);
		set_field_to_closure<LuaDebugOut>(L, "out");
		lua_setfield(L, -2, "debug");

		// Set aegisub.log
		set_field_to_closure<LuaDebugOut>(L, "log", -2);

		if (allow_config_dialog) {
			lua_createtable(L, 0, 3);
			set_field_to_closure<LuaDisplayDialog>(L, "display");
			set_field_to_closure<LuaDisplayOpenDialog>(L, "open");
			set_field_to_closure<LuaDisplaySaveDialog>(L, "save");
			lua_setfield(L, -2, "dialog");
		}

		// reference so other objects can also find the progress sink
		lua_pushvalue(L, -2);
		lua_setfield(L, LUA_REGISTRYINDEX, "progress_sink");

		lua_pop(L, 2);
	}

	LuaProgressSink::~LuaProgressSink()
	{
		// remove progress reporting stuff
		lua_getglobal(L, "aegisub");
		set_field_to_nil(L, -2, "progress");
		set_field_to_nil(L, -2, "debug");
		lua_pop(L, 1);

		set_field_to_nil(L, LUA_REGISTRYINDEX, "progress_sink");
	}

	agi::ProgressSink* LuaProgressSink::GetObjPointer(lua_State *L, int idx)
	{
		assert(lua_type(L, idx) == LUA_TUSERDATA);
		return *((agi::ProgressSink**)lua_touserdata(L, idx));
	}

	int LuaProgressSink::LuaSetProgress(lua_State *L)
	{
		GetObjPointer(L, lua_upvalueindex(1))->SetProgress(lua_tonumber(L, 1), 100);
		return 0;
	}

	int LuaProgressSink::LuaSetTask(lua_State *L)
	{
		GetObjPointer(L, lua_upvalueindex(1))->SetMessage(check_string(L, 1));
		return 0;
	}

	int LuaProgressSink::LuaSetTitle(lua_State *L)
	{
		GetObjPointer(L, lua_upvalueindex(1))->SetTitle(check_string(L, 1));
		return 0;
	}

	int LuaProgressSink::LuaGetCancelled(lua_State *L)
	{
		lua_pushboolean(L, GetObjPointer(L, lua_upvalueindex(1))->IsCancelled());
		return 1;
	}

	int LuaProgressSink::LuaDebugOut(lua_State *L)
	{
		agi::ProgressSink *ps = GetObjPointer(L, lua_upvalueindex(1));

		// Check trace level
		if (lua_type(L, 1) == LUA_TNUMBER) {
			// TODO: trace level
			if (lua_tointeger(L, 1) > 3)
				return 0;
			// remove trace level
			lua_remove(L, 1);
		}

		// Only do format-string handling if there's more than one argument left
		// (If there's more than one argument left, assume first is a format string and rest are format arguments)
		if (lua_gettop(L) > 1) {
			// Format the string
			lua_getglobal(L, "string");
			lua_getfield(L, -1, "format");
			// Here stack contains format string, format arguments, 'string' table, format function
			// remove 'string' table
			lua_remove(L, -2);
			// put the format function into place
			lua_insert(L, 1);
			// call format function
			if (lua_pcall(L, lua_gettop(L) - 1, 1, 0)) {
				// format failed so top of the stack now has an error message
				// which we want to add position information to
				luaL_where(L, 1);
				lua_insert(L, 1);
				lua_concat(L, 2);
				throw error_tag{};
			}
		}

		// Top of stack is now a string to output
		ps->Log(check_string(L, 1));
		return 0;
	}

	int LuaProgressSink::LuaDisplayDialog(lua_State *L)
	{
		agi::ProgressSink *ps = GetObjPointer(L, lua_upvalueindex(1));

		LuaDialog dlg(L, true); // magically creates the config dialog structure etc

		if (config::dialog_responses->size() > 0) {
			auto& pair = config::dialog_responses->front();
			dlg.PushButton(pair.first);
			dlg.Unserialise(pair.second);
			config::dialog_responses->pop_front();
		}

		// more magic: puts two values on stack: button pushed and table with control results
		return dlg.LuaReadBack(L);
	}

	int LuaProgressSink::LuaDisplayOpenDialog(lua_State *L)
	{
		agi::ProgressSink *ps = GetObjPointer(L, lua_upvalueindex(1));
		std::string message(check_string(L, 1));
		std::string dir(check_string(L, 2));
		std::string file(check_string(L, 3));
		std::string wildcard(check_string(L, 4));
		bool multiple = !!lua_toboolean(L, 5);
		bool must_exist = lua_toboolean(L, 6) || lua_isnil(L, 6);

		LOG_I("agi/auto4_lua_progresssink") << "Open file: " << message << "|" << dir << "|" << file << "|" << wildcard;

		if (config::file_responses->size() > 0) {
			auto& paths = config::file_responses->front();

			if (multiple) {
				lua_createtable(L, paths.size(), 0);
				for (size_t i = 0; i < paths.size(); ++i) {
					LOG_I("agi/auto4_lua_progresssink") << "Opening " << paths[i];
					lua_pushstring(L, paths[i].string().c_str());
					lua_rawseti(L, -2, i + 1);
				}
			} else {
				LOG_I("agi/auto4_lua_progresssink") << "Opening " << paths[0];
				lua_pushstring(L, paths[0].string().c_str());
			}

			config::file_responses->pop_front();
			return 1;
		} else {
			LOG_I("agi/auto4_lua_progresssink") << "Canceling open dialog";
			lua_pushnil(L);
			return 1;
		}
	}

	int LuaProgressSink::LuaDisplaySaveDialog(lua_State *L)
	{
		agi::ProgressSink *ps = GetObjPointer(L, lua_upvalueindex(1));
		std::string message(check_string(L, 1));
		std::string dir(check_string(L, 2));
		std::string file(check_string(L, 3));
		std::string wildcard(check_string(L, 4));
		bool prompt_overwrite = !lua_toboolean(L, 5);
		
		LOG_I("agi/auto4_lua_progresssink") << "Save file: " << message << "|" << dir << "|" << file << "|" << wildcard;

		if (config::file_responses->size() > 0) {
			auto& paths = config::file_responses->front();
			LOG_I("agi/auto4_lua_progresssink") << "Saving to " << paths[0];
			lua_pushstring(L, paths[0].string().c_str());
			config::file_responses->pop_front();
			return 1;
		} else {
			LOG_I("agi/auto4_lua_progresssink") << "Canceling save dialog";
			lua_pushnil(L);
			return 1;
		}
	}
}
