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
// Version: 23.01.07
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

	static std::map<std::string, bool> Done;
	static bool IsDone(std::string E) { return Done.count(E) && Done[E]; }


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

	inline Compilation CReturn(CompileResult R, GINIE Data = nullptr) {
		auto ret{ new _Compilation{R,Data} };
		return Compilation{ ret };
	}

	bool Modified(std::string File, bool debug, bool force) {
		if (Done.count(File) && Done[File]) return false; // Even with force, you do not want dupe compilations!
		if (force) {
			return true;
		}
		auto
			bcfile{ StripExt(File) + ".stb" }; if (debug) bcfile = StripExt(File) + ".debug.stb";
		if (!FileExists(bcfile)) return true;
		auto
			sourcestamp{ FileTimeStamp(File) },
			compstamp{ FileTimeStamp(bcfile) };
		auto
			ret{ (sourcestamp >= compstamp) };
		//std::cout << "Src: " << sourcestamp << "; Compiled: " << compstamp << ";  modified: " << ret << "\t" << File << "\n";
		//std::vector < std::string > Dependencies;
		return ret;
	}

	bool Modified(JT_Dir JD, std::string file, bool debug, bool Force) {
		auto ret{ Modified(JD->Entry(file)->MainFile, debug, Force) };
		//std::cout << file << " (" << JD->Entry(file)->MainFile << ")\t " << ret << "\n";
		return ret;
	}

	Compilation Compile(GINIE PrjData, Slyvina::JCR6::JT_Dir Res, std::string ScyndiSource, bool debug, bool force) {

		auto OutputFile{ StripExt(Res->Entry(ScyndiSource)->MainFile) + ".STB" };
		if (debug) OutputFile = StripExt(Res->Entry(ScyndiSource)->MainFile) + ".Debug.STB";
		if (IsDone(Res->Entry(ScyndiSource)->MainFile)) {
			auto HRes{ JCR6_Dir(OutputFile) };
			auto GDat{ ParseGINIE(HRes->GetString("Configuration.ini")) };
			if (!GDat) {
				QCol->Error("Error in configuration! Delete " + OutputFile + " and try running Scyndi again");
				return CReturn(CompileResult::Fail);
			}
			return CReturn(CompileResult::Skip, GDat);
		}
		if (!force) {
			auto CanSkip{ true };
			auto GDat{ ParseGINIE("[NOTHING]\nNothing=Nothing")};
			//std::cout << OutputFile << " E"<< FileExists(OutputFile)<< " M" << Modified(Res, ScyndiSource, debug, force) << "\n";
			if (FileExists(OutputFile) && (!Modified(Res,ScyndiSource,debug,force))) {				
				auto HRes{ JCR6_Dir(OutputFile) };
				GDat = ParseGINIE(HRes->GetString("Configuration.ini"));
				//QCol->Doing("Unmodified", ScyndiSource); // debug
				auto Depn{ GDat->List("DEPENDENCIES","List") };
				//QCol->Doing("Dependencies", Depn->size());
				if (!Modified(Res, ScyndiSource, debug, force)) {
					for (auto& D : *Depn) {
						// QCol->Doing("Checking", D); // debug
						if (Res->EntryExists(D + ".Scyndi")) {
							auto C{ Compile(PrjData, Res, D + ".Scyndi", debug, force) };
							CanSkip = CanSkip && C->Result == CompileResult::Skip;
							CanSkip = CanSkip && (!IsDone(Res->Entry(D + ".Scyndi")->MainFile));
							if (C->Result == CompileResult::Fail) return CReturn(CompileResult::Fail);
						}
					}
				}				
			} else { CanSkip = false; }
			if (CanSkip) return CReturn(CompileResult::Skip, GDat);
		} //else CanSkip = false;
	
		
		QCol->Doing("Reading", ScyndiSource);
		auto src{ Res->GetString(ScyndiSource) };
		auto T{ Translate(src,ScyndiSource,Res,PrjData,debug,force) };
		if (!T) {
			QCol->Error(TranslationError());
			return CReturn(CompileResult::Fail);
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
			Done[Res->Entry(ScyndiSource)->MainFile] = true;
			return CReturn(CompileResult::Success,T->Data);
		}

	}



	Slyvina::uint64 ProcessProject(std::string prj, bool force, bool debug) {
		Slyvina::uint64
			Success{ 0 },
			Skipped{ 0 },
			Failed{ 0 };

		TransVerbose = true;
		if (!FileExists(prj)) {
			if (!QuickYes("Project '" + prj + "' does not yet exist. Create it"))
				return 1;
			if (!DirectoryExists(ExtractDir(prj))) {
				if (!QuickYes("Create directory '" + ExtractDir(prj) + "'")) return 1;
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
			if (!DirectoryExists(D)) { QCol->Error("Source directory '" + D + "' does not exist."); return 1; }
			auto DRes{ Slyvina::JCR6::GetDirAsJCR(D) };
			if (!DRes) { QCol->Error("Source directory '" + D + "' could not be analyzed\n" + Last()->ErrorMessage); return 1; }
			Res->Patch(DRes, "Script/");
		}
		for (auto& D : *Libs) {
			if (!DirectoryExists(D)) { QCol->Error("Library directory '" + D + "' does not exist."); return 1; }
			auto DRes{ Slyvina::JCR6::GetDirAsJCR(D) };
			if (!DRes) { QCol->Error("Library directory '" + D + "' could not be analyzed\n" + Last()->ErrorMessage); return 1; }
			Res->Patch(DRes, "Libs/");
		}
		//auto Storage{ Ask(PrjData,"Package","Storage","Preferred package storage method:","zlib") };
		auto Entries{ Res->Entries() };
		//for (auto& E : *Entries) std::cout << "Entry: " << E->Name() << std::endl; // debug only!
		for (auto SD : *Entries) {
			auto E{ Upper(ExtractExt(SD->Name())) };
			if (E == "LUA") {
				QCol->Error("Pure Lua code not (yet) supported");
			} else if (E == "SCYNDI") {
				auto Result{ Compile(PrjData, Res, SD->Name(), debug, force) };
				switch (Result->Result) {
				case CompileResult::Success:
					Success++; break;
				case CompileResult::Fail:
					Failed++; break;
				case CompileResult::Skip:
					Skipped++; break;
				default:
					QCol->Error(TrSPrintF("Unknown compiler result (Internal error! Please report) (%03d)", (int)Result->Result));
					break;
				}
			}
		}
		QCol->Green("Project complete\n");
		if (Success) QCol->Doing("Success", Success);
		if (Failed) QCol->Doing("Failed", Failed);
		if (Skipped) QCol->Doing("Skipped", Skipped);
		return Failed;
	}
}