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
// Version: 22.12.24
// EndLic
#include <SlyvString.hpp>

#include "Translate.hpp"
#include "ScyndiGlobals.hpp"
#include "Keywords.hpp"

using namespace Slyvina;
using namespace Slyvina::Units;

namespace Scyndi {

	enum class InsKind { Unknown, HeaderDefintiion, General, IfStatement, WhileStatement, Increment, Decrement, DeclareVariable, DefineFunction, CompilerDirective, WhiteLine };
	enum class WordKind { Unknown, String, Number, KeyWord, Identifier, Operator, Macro };
	enum class ScopeKind { Unknown, General, Root, Repeat, Method, Class, Group };

	struct _Scope;


	struct Word;
	typedef std::shared_ptr<_Word> Word;
	struct _Word {
		WordKind Kind{ WordKind::Unknown };
		std::string TheWord;
		static Word NewWord(WordKind K, std::string W) { 
			auto ret{ std::make_shared<_Word>() };
			ret->Kind = K;
			ret->TheWord = W;
			return ret;
		}
	};


	struct _Instruction {
		InsKind Kind{ InsKind::Unknown };
		uint32 LineNumber{ 0 };
		std::string RawInstruction{ "" };
		std::vector<Word> Words{};
		std::string Comment{ "" };
		//_Scope* Parent;
		_TransProcess* TransParent{ nullptr };
		uint64 ScopeLevel{ 0 };
	};
	typedef std::shared_ptr<_Instruction> Instruction;

	/*
	struct _Scope {
		_Scope* Parent;
		std::vector<_Scope> KidScopes;
		ScopeKind Kind{ ScopeKind::Unknown };
	};
	*/

	struct _TransProcess {
		//_Scope RootScope{};
		std::vector < Instruction > Instructions;
		Translation Trans{};
	};

	static std::string _TLError{ "" };
	std::string TranslationError() {
		return _TLError;
	}
#define TransError(Err) { _TLError=Err; _TLError += " in line #"+std::to_string(LineNumber)+" ("+srcfile+")"; return nullptr; }
#define TransAssert(Condition,Err) { if (!(Condition)) TransError(Err); }

	Instruction Chop(std::string Line, uint64 LineNumber, size_t& pos, std::string srcfile) {
		bool
			InString{ false },
			InCharSeries{ false },
			InComment{ false },
			StringEscape{ false };
		Instruction
			Ret = std::make_shared<_Instruction>();
		std::string FormWord{ "" };
		while (pos < Line.size()) {
			auto ch{ Line[pos] };
			// In Comment
			if (InComment) {
				Ret->Comment += ch;
				pos++;

			// In a string
			} else if (InString) {
				if (StringEscape) {
					FormWord += ch;
					StringEscape = false;
					pos++;
				} else if (ch == '\\') {
					StringEscape = true;
					pos++;
				} else if (ch == '"') {
					InString = false;
					auto W{ _Word::NewWord(WordKind::String,FormWord) };
					FormWord = "";
					Ret->Words.push_back(W);
					pos++;
				} else {
					FormWord += ch;
					pos++;
				}
				if (InString) TransAssert(pos < Line.size(), "Unfinished string");
			}

		}
		return Ret; // debug
	}

	Translation Translate(Slyvina::VecString sourcelines, std::string srcfile, Slyvina::JCR6::JT_Dir JD, bool debug) {
		_TLError = "";
		_TransProcess Ret;
		Ret.Trans = std::make_shared<_Translation>();
		uint64 ScopeLevel{ 0 };
		for (size_t _ln = 0; _ln < sourcelines->size(); _ln++) {
			auto LineNumber{ _ln + 1 };
			size_t pos{0};
			while (pos >= 0 && pos < (*sourcelines)[_ln].size()) {
				auto Chopped = Chop((*sourcelines)[_ln], LineNumber, pos, srcfile); if (!Chopped) return nullptr;
			}
		}
		return Ret.Trans;
	}

	Translation Translate(std::string source, std::string srcfile, Slyvina::JCR6::JT_Dir JD, bool debug) {
		return Translate(Split(source, '\n'), srcfile, JD);
	}

}