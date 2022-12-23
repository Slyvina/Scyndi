// Lic:
// Scyndi
// Translator
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
// Version: 22.12.23
// EndLic
#include <SlyvString.hpp>

#include "Translate.hpp"
#include "ScyndiGlobals.hpp"

using namespace Slyvina;
using namespace Slyvina::Units;

namespace Scyndi {

	enum class InsKind {Unknown,General,IfStatement,WhileStatement,Increment,Decrement,DeclareVariable,DefineFunction,CompilerDirective,WhiteLine};
	enum class WordKind { Unknown, String, Number, KeyWord, Identifier, Operator, Macro };

	struct _Word {
		WordKind Kind{ WordKind::Unknown };
		std::string Word;
	};

	struct _Instruction {
		InsKind Kind{ InsKind::Unknown };
		uint32 LineNumber{ 0 };
		std::string RawInstruction{ "" };
		std::vector<_Word> Words{};
	};

	std::string TranslationError() {
		return std::string();
	}

	Translation Translate(Slyvina::VecString sourcelines, std::string srcfile, Slyvina::JCR6::JT_Dir JD) {
		return Translation();
	}

	Translation Translate(std::string source, std::string srcfile, Slyvina::JCR6::JT_Dir JD) {
		return Translate(Split(source, '\n'), srcfile, JD);
	}

}