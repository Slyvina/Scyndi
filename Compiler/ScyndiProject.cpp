// Lic:
// Scyndi
// Project Management
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
// Version: 23.01.01
// EndLic

#include <SlyvString.hpp>
#include <SlyvStream.hpp>
#include <SlyvQCol.hpp>
#include <SlyvTime.hpp>
#include <SlyvConInput.hpp>
#include <SlyvGINIE.hpp>
#include <SlyvAsk.hpp>

#include <JCR6_RealDir.hpp>
#include <JCR6_Write.hpp>

#include "ScyndiProject.hpp"
#include "Translate.hpp"
#include "SaveTranslation.hpp"

using namespace Slyvina::Units;
using namespace Slyvina::JCR6;

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

	bool Compile(GINIE PrjData,Slyvina::JCR6::JT_Dir Res,std::string ScyndiSource,bool debug,bool force) {
		// TODO: Skip if compilation if up-to-date, unless forced!
		QCol->Doing("Reading", ScyndiSource);
		auto src{ Res->GetString(ScyndiSource) };

		auto T{ Translate(src,ScyndiSource,Res,debug) };
		if (!T) {
			QCol->Error(TranslationError());
			return false;
		} else {
			// QCol->LGreen(T->LuaSource + "\n"); // debug only!
			auto OutputFile{ StripExt(Res->Entry(ScyndiSource)->MainFile) + ".STB" }; // STB = Scyndi Translated Bundle
			if (debug) { OutputFile = StripExt(Res->Entry(ScyndiSource)->MainFile) + ".Debug.STB"; }
			QCol->Doing("Bundling", OutputFile);
			auto Storage{ Ask(PrjData,"Package","Storage","Preferred package storage method:","zlib") };
			auto JO{ CreateJCR6(OutputFile) };
			auto ret{ SaveTranslation(T, JO, Storage) };
			JO->Close();
			QCol->Doing("Completed", ScyndiSource);
			std::cout << "\n\n";
			return ret;
		}

	}

	void ProcessProject(std::string prj, bool force, bool debug) {
		Slyvina::uint32
			Success{ 0 },
			Skipped{ 0 },
			Failed{ 0 };

		TransVerbose = true;
		if (!FileExists(prj)) {
			if (!QuickYes("Project '" + prj + "' does not yet exist. Create it"))
				return;
			if (!DirectoryExists(ExtractDir(prj))) {
				if (!QuickYes("Create directory '" + ExtractDir(prj) + "'")) return;
				MakeDir(ExtractDir(prj));
			}
			SaveString(prj,"[Project]\nCreated=" + CurrentDate() + "; " + CurrentTime()+"\n");
		}
		auto PrjData{ LoadGINIE(prj,prj,"Project file for a Scyndi Project\nLast modified: "+CurrentDate()) };
		Ask(PrjData, "AA_META", "01_Title", "Project title: ", StripAll(prj));
		auto author=Ask(PrjData, "AA_META", "02_CreatedBy", "Created by: ");
		Ask(PrjData, "AA_META", "03_Copyright", "Copyright: ", "(c) " + author);
		Ask(PrjData, "AA_META", "04_License", "License:");
		auto Dirs{ AskList(PrjData,"DIRECTORY","SOURCEFILES","Name the directories where I can find the source files:") };
		auto Libs{ AskList(PrjData,"DIRECTORY","Libraries","Name the directories where I can find the libraries:",0) };
		auto Res{ std::make_shared<Slyvina::JCR6::_JT_Dir>() };
		for (auto& D : *Dirs) {
			if (!DirectoryExists(D)) { QCol->Error("Source directory '" + D + "' does not exist."); return; }
			auto DRes{ Slyvina::JCR6::GetDirAsJCR(D) };
			if (!DRes) { QCol->Error("Source directory '" + D + "' could not be analyzed\n" + Last()->ErrorMessage); return; }
			Res->Patch(DRes, "Script/");
		}
		for (auto& D : *Libs) {
			if (!DirectoryExists(D)) { QCol->Error("Library directory '" + D + "' does not exist."); return; }
			auto DRes{ Slyvina::JCR6::GetDirAsJCR(D) };
			if (!DRes) { QCol->Error("Library directory '" + D + "' could not be analyzed\n" + Last()->ErrorMessage); return; }
			Res->Patch(DRes, "Libs/");
		}
		//auto Storage{ Ask(PrjData,"Package","Storage","Preferred package storage method:","zlib") };
		auto Entries{ Res->Entries() };
		for (auto SD : *Entries) {
			auto E{ Upper(ExtractExt(SD->Name())) };
			if (E == "LUA") {
				QCol->Error("Pure Lua code not (yet) supported");
			} else if (E == "SCYNDI") {
				if (Compile(PrjData, Res, SD->Name(), debug, force))
					Success++;
				else
					Failed++;
			}
		}
		QCol->Green("Project complete\n");
		if (Success) QCol->Doing("Success", Success);
		if (Failed) QCol->Doing("Failed", Failed);
		if (Skipped) QCol->Doing("Skipped", Skipped);
	}
}