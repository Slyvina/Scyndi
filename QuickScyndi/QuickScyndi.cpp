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
// Version: 23.01.03
// EndLic
#include "../ScyndiVersion.hpp"

#include <Lunatic.hpp>

#include <SlyvQCol.hpp>
#include <SlyvString.hpp>

using namespace Slyvina;
using namespace Slyvina::Units;
using namespace Slyvina::Lunatic;

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
}


int main(int c, char** args) {
	using namespace Scyndi;
	if (c < 2) Header(args[0]);
}