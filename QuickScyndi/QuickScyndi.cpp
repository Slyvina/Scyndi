// Lic:
// Quick Scyndi
// A quick runtime tool, for debugging purposes
// 
// 
// 
// (c) Jeroen P. Broks, 2023
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

#include "../ScyndiVersion.hpp"

#include <zlib.h>

#include <Lunatic.hpp>

#include <SlyvQCol.hpp>
#include <SlyvString.hpp>
#include <SlyvStream.hpp>

#include <JCR6_Core.hpp>
#include <JCR6_zlib.hpp>


using namespace Slyvina;
using namespace Slyvina::Units;
using namespace Slyvina::Lunatic;
using namespace Slyvina::JCR6;

namespace Scyndi {
	void Header(std::string MyExe) {
		QCol->LGreen("Quick Scyndi Runner\n");
		QCol->Doing("Version", QVersion.Version(true));
		QCol->Doing("Coded by", "Jeroen P. Broks");
		std::cout << "\n";
		QCol->Doing("Lua version", _Lunatic::Lua_Version());
		QCol->Doing("Lua developed by", "PUC Rio");
		std::cout << "\n\n\n";
		QCol->White("Usage: ");
		QCol->Yellow(StripAll(MyExe));		
		QCol->LCyan(" <STB Bundle>\n\n");
	}

	int Paniek(lua_State* L) {
		QCol->Error("Lua Error");
		//std::string Trace{};
		//Error("Lua Error!");
		QCol->Doing("Lua#", lua_gettop(L));
		for (int i = 1; i <= lua_gettop(L); i++) {
			QCol->Magenta(TrSPrintF("Arg #%03d\t", i));
			switch (lua_type(L, i)) {
			case LUA_TSTRING:
				QCol->Doing("String", "\"" + std::string(luaL_checkstring(L, i)) + "\"");
				//Trace += luaL_checkstring(L, i); Trace += "\n";
				break;
			case LUA_TNUMBER:
				QCol->Doing("Number ", std::to_string(luaL_checknumber(L, i)));
			case LUA_TFUNCTION:
				QCol->Doing("Function", "<>");
			default:
				QCol->Doing("Unknown: ", lua_type(L, i));
				break;
			}
			///cout << "\n";
		}
		//Error("", false, true);
		QCol->Reset();
		exit(11);		
		return 0;
	}
}


int main(int c, char** args) {
	using namespace Scyndi;
	if (c < 2) {
		Header(args[0]);
		return 0;
	}
	init_zlib();
	auto d{ ExtractDir(args[0]) };
	auto ScyndiCoreFile{ d + "/ScyndiCore.lua" }; if (!FileExists(ScyndiCoreFile)) { QCol->Error(ScyndiCoreFile + " not found"); return 404; }
	auto ScyndiCore{ FLoadString(ScyndiCoreFile) };
	_Lunatic::Panick = Paniek;
	for (int i = 1; i < c; i++) {
		auto L{ LunaticBySource(ScyndiCore) };
		auto J{ JCR6_Dir(args[i]) };
		if (!J) { QCol->Error("Could not read " + std::string(args[i]) + ". " + Last()->ErrorMessage); return 500; }
		auto src = J->GetString("Translation.lua"); // Easiest way to go. It's only a test tool after all!
		if (Last()->Error) { QCol->Error(Last()->ErrorMessage); return 100; }
		L->QDoString(src);
		std::cout << "\n\n";
	}
	QCol->Reset();
	return 0;
}