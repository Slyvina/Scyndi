// Lic:
// Scyndi
// Save and bundle translation
// 
// 
// 
// (c) Jeroen P. Broks, 2022, 2023
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// Please note that some references to data like pictures or audio, do not automatically
// fall under this licenses. Mostly this is noted in the respective files.
// 
// Version: 23.01.04
// EndLic
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <SlyvQCol.hpp>
#include <SlyvString.hpp>
#include <JCR6_Write.hpp>

#include "Translate.hpp"


using namespace Slyvina::Units;
using namespace Slyvina::JCR6;

namespace Scyndi {

	static bool err{ false };

	std::shared_ptr<std::vector<char>> OutBuf{ nullptr };

	static int LuaPaniek(lua_State* L) {
		QCol->Error("Compiling Lua translation failed");
		//std::string Trace{};
		//Error("Lua Error!");
		QCol->Doing("Lua#", lua_gettop(L));
		for (int i = 1; i <= lua_gettop(L); i++) {
			QCol->Magenta(TrSPrintF( "Arg #%03d\t", i));
			switch (lua_type(L, i)) {
			case LUA_TSTRING:
				QCol->Doing("String", "\"" + std::string(luaL_checkstring(L, i)) + "\"");
				//Trace += luaL_checkstring(L, i); Trace += "\n";
				break;
			case LUA_TNUMBER:
				QCol->Doing("Number ", std::to_string(luaL_checknumber(L, i)));
			case LUA_TFUNCTION:
				QCol->Doing("Function","<>");
			default:
				QCol->Doing("Unknown: " , lua_type(L, i));
				break;
			}
			///cout << "\n";
		}
		//Error("", false, true);
		//exit(11);
		err = true;
		return 0;
	}

	static int DumpLua(lua_State* L, const void* p, size_t sz, void* ud) {
	   
		auto pcp{ (char*)p };
		for (size_t i = 0; i < sz; ++i) OutBuf->push_back(pcp[i]);
		return 0;
	}

	bool SaveTranslation(Translation Trans, JT_Create Out,std::string Storage) {
		err = false;
		auto L{ luaL_newstate() };
		auto source{ Trans->LuaSource };
		QCol->Doing("Compiling", "Lua translation");
		lua_atpanic(L, LuaPaniek);
		luaL_openlibs(L);
		luaL_loadstring(L, source.c_str());
		//lua_call(L, 0, 0);
		if (!err) {
			OutBuf = std::make_shared < std::vector < char > >();
			lua_dump(L, DumpLua, NULL, 0);
		}
		if (!OutBuf->size()) {
			QCol->Error("Lua translation failed!");
			LuaPaniek(L); // Test
			QCol->LGreen("<source>\n"); 
			auto Lines = Split(source,'\n');
			for (size_t ln = 0; ln < Lines->size(); ln++) {
				QCol->LMagenta(TrSPrintF("%9d\t", ln + 1));
				QCol->LCyan((*Lines)[ln]);
				QCol->White("\n");
			}
			QCol->LGreen("</source>\n");
			lua_close(L);
			return false;
		}
		if (Out) {
			QCol->Doing("Writing", "Bytecode");
			if (OutBuf->size() > 2048)
				Out->AddChars(*OutBuf, "ByteCode.lbc", Storage);
			else
				Out->AddChars(*OutBuf, "ByteCode.lbc");
			QCol->Doing("Writing", "Translation");
			if (source.size() > 2048)
				Out->AddString(source, "Translation.lua", Storage);
			else
				Out->AddString(source, "Translation.lua");
			QCol->Doing("Writing", "Configuration");
			Trans->Data->Value("Lua", "Version", TrSPrintF("%s.%s.%s", LUA_VERSION_MAJOR, LUA_VERSION_MINOR, LUA_VERSION_RELEASE));
			auto UPD{ Trans->Data->UnParse() };
			if (UPD.size() > 2048)
				Out->AddString(UPD, "Configuration.ini", Storage);
			else
				Out->AddString(UPD, "Configuration.ini");

		} else {
			//cout << "Writing:   " << dest << endl;
			//auto BT{ WriteFile(dest) };
			//for (auto ch : OutBuf) BT->Write(ch);
			//BT->Close();
		}
		lua_close(L);
		return !err;
	}

}