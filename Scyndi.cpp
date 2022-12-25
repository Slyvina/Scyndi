// Lic:
// Scyndi
// Main File
// 
// 
// 
// (c) Jeroen P. Broks, 2022
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
// Version: 22.12.25
// EndLic

#include <Lunatic.hpp>
#include <SlyvQCol.hpp>
#include <SlyvArgParse.hpp>
#include <SlyvString.hpp>

#include <JCR6_RealDir.hpp>
#include <JCR6_zlib.hpp>

#include "ScyndiVersion.hpp" 
#include "Compiler/Config.hpp"
#include "Compiler/ScyndiProject.hpp"

using namespace Scyndi;
using namespace Slyvina;
using namespace Slyvina::Units;

int main(int nargs, char** args) {
	using namespace Scyndi;
	QCol->LGreen("Scyndi Compiler\n");
	QCol->Doing("Version", QVersion.Version(true));
	QCol->Doing("Coded by", "Jeroen P. Broks");
	std::cout << "\n";
	QCol->Doing("Lua version", Slyvina::Lunatic::_Lunatic::Lua_Version());
	QCol->Doing("Lua developed by", "PUC Rio");
	std::cout << "\n\n\n";
	FlagConfig cargs{};
	AddFlag(cargs, "sf", false); // if true project, if false single file
	AddFlag(cargs, "dbg", false);
	AddFlag(cargs, "force", false);
	RegArgs(cargs, nargs, args);
	if (!NumFiles()) {
		QCol->White("Usage: ");
		QCol->Yellow(StripAll(args[0]));
		QCol->LMagenta(" [<switchs>]");
		QCol->LCyan(" <source files/project>\n\n");
		QCol->Yellow("Switches:\n");
		QCol->LCyan("\t-sf     "); QCol->LGreen("Single files (in stead of project\n");
		QCol->LCyan("\t-dbg    "); QCol->LGreen("Make debug builds\n");
		QCol->LCyan("\t-force    "); QCol->LGreen("Force a compilation\n");
		QCol->Reset();
		std::cout << "\n\n\n";
		return 1;
	} else if (WantProject()) {
		auto F{ Files() };
		for (size_t i = 0; i < NumFiles(); i++) {
			auto Prj = (*F)[i];
			if (!Suffixed(Lower(Prj), ".scyndiproject")) Prj += ".ScyndiProject";
			QCol->Doing(TrSPrintF("Project %d/%d", i+1, NumFiles()), Prj);
			ProcessProject(Prj, WantForce(), WantDebug());
		}
	}
	return 0;
}