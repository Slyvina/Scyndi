// Lic:
// Scyndi
// Project Management
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

#include <SlyvString.hpp>
#include <SlyvStream.hpp>
#include <SlyvQCol.hpp>
#include <SlyvTime.hpp>
#include <SlyvConInput.hpp>
#include "ScyndiProject.hpp"

using namespace Slyvina::Units;

namespace Scyndi {

	bool QuickYes(std::string Question) {
		do {
			QCol->Yellow(Question);
			QCol->LMagenta(" ? ");
			QCol->LCyan("<Y/N> ");
			auto answer{ Trim(ReadLine()) };
			if (answer.size()) {
				switch (answer[0]) {
				case 'J': // Ja (Dutch for "yes")
				case 'j':
				case 'Y': // Yes
				case 'y':
					return true;
				case 'N': // Nee (Dutch for "no") and "No"
				case 'n':
					return false;
				}
			}
		} while (true);
	}

	void ProcessProject(std::string prj, bool force, bool debug) {
		if (!FileExists(prj)) {
			if (!QuickYes("Project '" + prj + "' does not yet exist. Create it"))
				return;
			if (!DirectoryExists(ExtractDir(prj))) {
				if (!QuickYes("Create directory '" + ExtractDir(prj) + "'")) return;
				MakeDir(ExtractDir(prj));
			}
			SaveString(prj,"[Project]\nCreated=" + CurrentDate() + "; " + CurrentTime()+"\n");
		}
	}
}