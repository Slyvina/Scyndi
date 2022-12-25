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

#include <Slyvina.hpp>
#include <SlyvString.hpp>
#include <SlyvVecSearch.hpp>

#include "Translate.hpp"
#include "ScyndiGlobals.hpp"
#include "Keywords.hpp"

using namespace Slyvina;
using namespace Slyvina::Units;

namespace Scyndi {

	bool TransVerbose{ false };

	enum class InsKind { Unknown, HeaderDefintiion, General, IfStatement, WhileStatement, Increment, Decrement, DeclareVariable, DefineFunction, CompilerDirective, WhiteLine };
	enum class WordKind { Unknown, String, Number, KeyWord, Identifier, Operator, Macro, Comma, Field, CompilerDirective };
	enum class ScopeKind { Unknown, General, Root, Repeat, Method, Class, Group };

	//struct _Scope;


	class Word;
	typedef std::shared_ptr<_Word> Word;
	class _Word {
	public:
		WordKind Kind{ WordKind::Unknown };
		std::string TheWord;
		std::string UpWord;
		static Word NewWord(WordKind K, std::string W) { 
			auto ret{ std::make_shared<_Word>() };
			ret->Kind = K;
			ret->TheWord = W;
			ret->UpWord = Upper(W);
			return ret;
		}

		static Word NewWord(std::string W) {
			auto ret{ std::make_shared<_Word>() };
			ret->TheWord = W;
			ret->UpWord = Upper(W);
			if (ret->TheWord[0] == '$') {
				ret->Kind = WordKind::Identifier;
				ret->UpWord = Upper(ret->TheWord).substr(1);
				if (!ret->UpWord.size()) ret->UpWord == "___DOLLARSIGN";
			} else if (VecSearch(KeyWords, ret->UpWord))
				ret->Kind = WordKind::KeyWord;
			else if (ret->UpWord == ",")
				ret->Kind = WordKind::Comma;
			else if (VecSearch(Operators, ret->UpWord))
				ret->Kind = WordKind::Operator;
			else if (ret->UpWord[0] >= '0' && ret->UpWord[0] <= '9')
				ret->Kind = WordKind::Number;
			else if (ret->UpWord[0] == '.') {
				if (ret->UpWord.size() < 2) return nullptr; // Cannot be completed. Syntax error!
				if (ret->UpWord[1] >= '0' && ret->UpWord[1] <= 9) ret->Kind = WordKind::Number;
				else ret->Kind = WordKind::Field;
			} else if(ret->UpWord[0]=='#') {
				ret->Kind = WordKind::CompilerDirective;
			} else
				ret->Kind = WordKind::Identifier; // Although this is not entirely true

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
			FirstChar{ false },
			StringEscape{ false },
			FormNumber{ false },
			FormNumberHex{ false },
			FormingWord{ false }; // For both identifiers AND keywords!
		Instruction
			Ret = std::make_shared<_Instruction>();
		std::vector<ScopeKind>
			Scopes{ ScopeKind::Root };
		std::string FormWord{ "" };
		Ret->LineNumber = LineNumber;
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
					FormWord += "\\";
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

			// In Char Series
			} else if (InCharSeries) {
				if (StringEscape) {
					if (!FirstChar) {
						Ret->Words.push_back(_Word::NewWord(WordKind::Comma, ","));
						FirstChar = false;
					}
					Ret->Words.push_back(_Word::NewWord(WordKind::Number, std::to_string((int)ch)));
					StringEscape = false;
					pos++;
				} else if (ch == '\\') {
					StringEscape = true;
					pos++;
				} else if (ch == '\'') {
					InCharSeries = false;
					pos++;
				} else {
					if (!FirstChar) {
						Ret->Words.push_back(_Word::NewWord(WordKind::Comma, ","));
						FirstChar = false;
					}
					Ret->Words.push_back(_Word::NewWord(WordKind::Number, std::to_string((int)ch)));
					pos++;
				}
				if (InCharSeries) TransAssert(pos < Line.size(), "Unfinished character series");
			// Start Comment
			} else if (ch == '/' && pos < Line.size() - 1 && Line[pos + 1] == '/') {
				InComment = true;
				pos += 2;

			// Doing Number
			} else if (FormNumber) {
				bool EndNum{ false };
				// Please note! No position increase on end num (except for whitespaces). It's vital as otherwise the next character may not be taken up for what it should be taken up.
				switch (ch) {
				case 'x':
				case 'X':
					if (FormWord.size() == 1 && FormWord[0] == '0') {
						FormNumberHex = true; pos++;
						FormWord += "x";
					} else EndNum = true;
					break;
				case '.':
					TransAssert(!FormNumberHex, "Decimal point in hexadecimal number");
					TransAssert(pos < Line.size() - 1, "Decimal point at the end of the line");
					FormWord += ".";
					pos++;
					break;
				case 'a':
				case 'A':
				case 'b':
				case 'B':
				case 'c':
				case 'C':
				case 'd':
				case 'D':
				case 'e':
				case 'E':
				case 'f':
				case 'F':
					if (FormNumberHex) {
						FormWord += Lower("" + ch);
						pos++;
					} else {
						EndNum = true;
					}
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					FormWord += ch;
					pos++;
					break;
				default:
					EndNum = true;
					break;
				}
				if (EndNum) {
					Ret->Words.push_back(_Word::NewWord(WordKind::Number, FormWord));
					FormWord = "";
					FormNumber = false;
					FormNumberHex = false;
				}

			} else if (FormingWord) {
				bool GoOn{
					(ch >= 'A' && ch <= 'Z') ||
					(ch >= 'a' && ch <= 'z') ||
					(ch >= '0' && ch <= '9') ||
					(ch >= '_')
				};
				if (GoOn) {
					FormingWord += ch;
					pos++;
				} else {
					Ret->Words.push_back(_Word::NewWord(FormWord)); // Autodetection is now in order. Is this a keyword or is this an identifyer?
					FormingWord = "";
				}
			// TODO: Next
			} else {
				switch (ch) {
				case ';':
					if (FormWord.size())
						Ret->Words.push_back(_Word::NewWord(FormWord));
					pos++; // Remember this is reference based!
					return Ret;
				case ' ':
					if (FormWord.size())
						Ret->Words.push_back(_Word::NewWord(FormWord));
					pos++;
					break;
				case '#':
					TransAssert(pos == 0, "Syntax error! # is reserved for compiler directives and may only be at the start of the line");
					FormingWord = true;
					FormWord = "#";
					break;
				case '.': {
					TransAssert(pos < Line.size() - 1 && Line[pos + 1] != ';', "No instruction can end with a period/dot");
					auto next{ Line[pos + 1] };
					if (next >= '0' && next <= '1') {
						FormWord = "0." + next;
						FormNumber = true;
						FormNumberHex = false;
						pos += 2;
					} else if (next == '_' || (next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z')) {
						FormingWord = "." + next;
						pos += 2;
					}
				} break;
				case ',':
					Ret->Words.push_back(_Word::NewWord(","));
				default:
					if (ch >= '0' && ch <= '9') {
						FormNumber = true;
						FormNumberHex = false;
						FormWord = ch;
					} else if (ch == '_' || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
						FormingWord = true;
						FormWord = ch;
					} else if (VecSearch(Operators, std::string("" + ch))) {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "" + ch));
						pos++;
					} else if (pos < Line.size() - 1 && VecSearch(Operators, std::string("" + ch + Line[pos + 1]))) {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "" + ch+Line[pos+1]));
						pos += 2;
					} else {
						TransError(TrSPrintF("Completely unexpected %s (Position %d) ", ch, pos));
					}
				}
			}
		}
		return Ret; 
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
				// TODO: Process the chopped data
			}
		}
		return Ret.Trans;
	}

	Translation Translate(std::string source, std::string srcfile, Slyvina::JCR6::JT_Dir JD, bool debug) {
		return Translate(Split(source, '\n'), srcfile, JD);
	}

}