// Lic:
// Scyndi
// Translator (header)
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
// Version: 22.12.27
// EndLic

#pragma once
#include <Slyvina.hpp>
#include <SlyvGINIE.hpp>
#include <SlyvTime.hpp>
#include <JCR6_Core.hpp>

namespace Scyndi {

	extern bool TransVerbose;

	enum class ScriptKind { Unknown, Script, Module };

	struct _Translation {
		std::string
			ScriptName{ "" }, // Used to store the name set up with the SCRIPT or MODULE top line
			LuaSource{ "" },
			Headers{ "" }; // Needed for imports with global definitions
		ScriptKind
			Kind{ ScriptKind::Unknown };
		Slyvina::StringMap
			Classes{ Slyvina::NewStringMap() },
			GlobalVar{ Slyvina::NewStringMap() };
		Slyvina::VecString
			RealIncludes{ Slyvina::NewVecString() },
			JCRIncludes{ Slyvina::NewVecString() };
		Slyvina::Units::GINIE
			Data{ ParseGINIE("[Create]\nData=" + Slyvina::Units::CurrentDate()) };
	};
	typedef std::shared_ptr<_Translation> Translation;

	/// <summary>
	/// Returns an empty string if the last translation was succesful, otherwise it will return the error message
	/// </summary>
	/// <returns></returns>
	std::string TranslationError();


	Translation Translate(Slyvina::VecString sourcelines, std::string srcfile = "", Slyvina::JCR6::JT_Dir JD = nullptr, bool debug = false);
	Translation Translate(std::string source, std::string srcfile = "", Slyvina::JCR6::JT_Dir JD = nullptr, bool debug = false);
	

}