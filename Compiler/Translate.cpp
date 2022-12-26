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
// Version: 22.12.26
// EndLic

#include <Slyvina.hpp>
#include <SlyvString.hpp>
#include <SlyvVecSearch.hpp>
#include <SlyvQCol.hpp>
#include <SlyvStream.hpp>

#include "Translate.hpp"
#include "ScyndiGlobals.hpp"
#include "Keywords.hpp"

#undef TransDebug


#ifdef TransDebug
#define Chat(abc) QCol->LGreen("SCYNDI TRANSLATOR DEBUG> "); QCol->White(""); std::cout << abc << std::endl;
#else
#define Chat(abc)
#endif

using namespace Slyvina;
using namespace Slyvina::Units;

namespace Scyndi {

	bool TransVerbose{ false };

	enum class InsKind { Unknown, HeaderDefintiion, General, QuickMeta, IfStatement, WhileStatement, Increment, Decrement, DeclareVariable, DefineFunction, CompilerDirective, WhiteLine, MutedByIfDef, StartInit, EndScope };
	enum class WordKind { Unknown, String, Number, KeyWord, Identifier, Operator, Macro, Comma, Field, CompilerDirective, HaakjeOpenen, HaakjeSluiten };
	enum class ScopeKind { Unknown, General, Root, Repeat, Method, Class, Group, Init, QuickMeta };

	//struct _Scope;

	inline void Verb(std::string DHead, std::string DObj, std::string ending = "\n") {
		if (TransVerbose) QCol->Doing(DHead, DObj, ending);
	}


	class _Word;
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
			else if (ret->UpWord == "(")
				ret->Kind = WordKind::HaakjeOpenen;
			else if (ret->UpWord == ")")
				ret->Kind = WordKind::HaakjeSluiten;
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

	struct _TransProcess;
	struct _Instruction {
		_TransProcess* TransParent{ nullptr };
		InsKind Kind{ InsKind::Unknown };
		std::string SourceFile{""};
		uint32 LineNumber{ 0 };
		std::string RawInstruction{ "" };
		std::vector<Word> Words{};
		std::string Comment{ "" };
		//_Scope* Parent;
		uint64 ScopeLevel{ 0 };
		ScopeKind Scope{ ScopeKind::Unknown };
	};
	typedef std::shared_ptr<_Instruction> Instruction;

	/*
	struct _Scope {
		_Scope* Parent;
		std::vector<_Scope> KidScopes;
		ScopeKind Kind{ ScopeKind::Unknown };
	};
	*/

	class _TransProcess {
	public:
		//_Scope RootScope{};
		std::vector < Instruction > Instructions;
		Translation Trans{};
		std::vector<ScopeKind> Scopes;
		uint64 ScopeLevel() { return Scopes.size(); };
		ScopeKind Scope() {
			auto lvl{ ScopeLevel() };
			if (lvl == 0) return ScopeKind::Root;						
			return Scopes[lvl - 1];
		}
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
		Ret->SourceFile = srcfile;
		Ret->LineNumber = LineNumber;
		while (pos < Line.size()) {
			auto ch{ Line[pos] };
			Chat("==> " << pos << "/" << Line.size() << "; Char: "<<ch);
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
					Chat("==> Ending string: " << FormWord);
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
				Chat("==> Char '" << ch << "' on pos " << pos << " leaves GoOn >" << GoOn);
				if (GoOn) {
					FormWord += ch;
					pos++;
				} else {
					Ret->Words.push_back(_Word::NewWord(FormWord)); // Autodetection is now in order. Is this a keyword or is this an identifyer?
					Chat("==> Ended word: " << FormWord);
					FormWord = "";
					FormingWord = false;
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
				case '\t':
					if (FormWord.size())
						Ret->Words.push_back(_Word::NewWord(FormWord));
					pos++;
					break;
				case '(':
				case ')': {
					std::string w{ "" }; w += ch;
					Ret->Words.push_back(_Word::NewWord(w));
					pos++;
				} break;
				case '#':
					TransAssert(pos == 0, "Syntax error! # is reserved for compiler directives and may only be at the start of the line");
					Ret->Words.push_back(_Word::NewWord("#"));
					FormingWord = false;
					FormWord = "";
					pos++;
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
				case '"':
					InString = true;
					pos++;
					break;
				default:
					if (ch >= '0' && ch <= '9') {
						FormNumber = true;
						FormNumberHex = false;
						FormWord = ch;
						pos++;
					} else if (ch == '_' || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
						FormingWord = true;
						FormWord = ch;
						pos++;
					} else if (VecSearch(Operators, std::string("" + ch))) {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "" + ch));
						pos++;
					} else if (pos < Line.size() - 1 && VecSearch(Operators, std::string("" + ch + Line[pos + 1]))) {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "" + ch+Line[pos+1]));
						pos += 2;
					} else {
						std::string chs{ "character '" }; chs += ch; chs += "'";				
						TransError(TrSPrintF("Completely unexpected %s (Position %d) ", chs.c_str(), pos));
					}
				}
			}
		}
		if (FormWord.size()) {
			Ret->Words.push_back(_Word::NewWord(FormWord));
		}
		return Ret; 
	}

	static std::vector<Instruction> ChopCode(Slyvina::VecString sourcelines, std::string srcfile, Slyvina::JCR6::JT_Dir JD, bool debug) {
		Chat("Chopping " << srcfile);
		std::vector<Instruction> Ret;
		for (size_t _ln = 0; _ln < sourcelines->size(); _ln++) {
			auto LineNumber{ _ln + 1 };
			Chat("=> Line " << LineNumber << "/" << sourcelines->size());
			size_t pos{0};
			while (pos >= 0 && pos < (*sourcelines)[_ln].size()) {
				Chat("=> Postion: " << pos << "/" << (*sourcelines)[_ln].size());
				auto Chopped = Chop((*sourcelines)[_ln], LineNumber, pos, srcfile);
				if (!Chopped) return std::vector<Instruction>();				
				Ret.push_back(Chopped);
			}
		}
		return Ret;
	}

	Translation Translate(Slyvina::VecString sourcelines, std::string srcfile, Slyvina::JCR6::JT_Dir JD, bool debug) {
		Verb("Compiling", srcfile);
		_TLError = "";
		_TransProcess Ret;
		Ret.Trans = std::make_shared<_Translation>();
		//uint64 ScopeLevel{ 0 };
		
		// Chopping
		Ret.Instructions = ChopCode(sourcelines, srcfile, JD, debug);
		if (!Ret.Instructions.size()) return nullptr; // Something must have gone wrong
		// Include
	StartInclude:
		for (size_t ln = 0; ln < Ret.Instructions.size(); ln++) {
			auto Ins = Ret.Instructions[ln];
			auto LineNumber = Ins->LineNumber;
			if (Ins->Words.size()>=2) {
				auto _hashtag = Ins->Words[0];
				auto _include = Ins->Words[1];
				if (_hashtag->TheWord == "#" && _include->UpWord == "INCLUDE") {
					TransAssert(Ins->Words.size() >= 3, "Incomplete #INCLUDE request");
					auto _file = Ins->Words[2];
					TransAssert(_file->Kind == WordKind::String, "#INCLUDE expects a string for a filename");
					Verb("=> Including", _file->TheWord);
					std::vector<Instruction> IncChopped{};
					if (FileExists(_file->TheWord)) {
						auto isrc = LoadLines(_file->TheWord);
						IncChopped = ChopCode(isrc,srcfile,JD,debug);
						Ret.Trans->RealIncludes->push_back(_file->TheWord);
					} else if (JD->EntryExists(_file->TheWord)) {
						auto isrc = JD->GetLines(_file->TheWord);
						IncChopped = ChopCode(isrc, srcfile, JD, debug);
						Ret.Trans->JCRIncludes->push_back(_file->TheWord);
					} else {
						TransError("Inclusion of " + _file->TheWord + " failed!\nFile not found");
					}
					TransAssert(IncChopped.size(), "Inclusion of " + _file->TheWord + " failed!\n" + _TLError + "\n");
					std::vector<Instruction> NewVec{};
					for (size_t oi = 0; oi < ln; ++oi) NewVec.push_back(Ret.Instructions[oi]);
					for (auto ich : IncChopped) NewVec.push_back(ich);
					for (size_t oi = ln + 1; oi < Ret.Instructions.size(); ++oi) NewVec.push_back(Ret.Instructions[oi]);
					Ret.Instructions = NewVec;
					goto StartInclude; // Basically this should not be needed, but ya never know, eh?
				}
			}
		}

		// Pre-Processing
		Verb("=> Pre-processing", srcfile);
		std::map<std::string, Word> TransConfig;
		std::map<std::string, bool> Defs;
		bool
			MuteByIfDef{ false },
			HaveIfDef{ false },
			HaveElse{ false },
			First{ false },
			HasInit{ false }; // If there's at least one init this must be true.
		for (auto ins : Ret.Instructions) {
			ins->ScopeLevel = Ret.ScopeLevel();
			ins->Scope = Ret.Scope();
			Chat("Preprocessing in instruction on line #" << ins->LineNumber);
#ifdef TransDebug
			for (size_t i = 0; i < ins->Words.size(); i++) { Chat("Word " << i + 1 << "/" << ins->Words.size()<<"> "<<ins->Words[i]->TheWord); }
#endif
			auto LineNumber = ins->LineNumber;
			if (!ins->Words.size()) {
				Chat("--> Whiteline!");
				ins->Kind = InsKind::WhiteLine;
			} else if (ins->Words[0]->TheWord == "#") {
				Chat("Parsing compilier directive");
				// Please note this comes before 'first'. This because #define/#ifdef etc. can have some influence here!
				ins->Kind = InsKind::CompilerDirective;
				TransAssert(ins->Words.size() >= 2,"Incomplete compiler directive");
				auto Opdracht{ ins->Words[1]->UpWord };
				std::string Para{ "" }; if (ins->Words.size() >= 3) Para = ins->Words[2]->TheWord;
				Chat("Compiler directive: " << Opdracht << "  (Param: " << Para << ")");
				if (Opdracht == "ERROR") {
					if (!MuteByIfDef) { // Ignore error if definitions call for that
						_TLError = Para + " in line #" + std::to_string(ins->LineNumber) + " of file " + srcfile;
						return nullptr;
					}
				} else if (Opdracht == "PRAGMA") {
					// Do nothing at the present time, but 'Pragma' can be used for engine specific settings (if the compiler supports those).					
				} else if (Opdracht == "REGION" || Opdracht == "ENDREGION") {
					// Do nothing as these are merely markers that some (advanced) IDEs may be able to use. Similar to #region and #endregion in C#.
				} else if (Opdracht == "DEF" || Opdracht == "DEFINE") {
					if (!MuteByIfDef) Defs[Upper(Para)] = true;
				} else if (Opdracht == "UNDEF" || Opdracht == "UNDEFINE") {
					if (!MuteByIfDef) Defs[Upper(Para)] = false;
				} else if (Opdracht == "IF" || Opdracht == "IFDEF") {
					TransAssert(ins->Words.size() < 3, "Incomplete #IF statement");
					TransAssert(!(HaveIfDef || HaveElse), "Old #IF must be closed before starting a new one!");
					auto outcome{ true };
					for (size_t p = 2; p < ins->Words.size(); p++) {
						outcome = outcome && Defs[ins->Words[p]->UpWord];
					}
					MuteByIfDef = !outcome;
					HaveIfDef = true;
				} else if (Opdracht == "ELSEIF" || Opdracht == "ELIF") {
					TransAssert(ins->Words.size() < 3, "Incomplete #ELSEIF statement");
					TransAssert(HaveIfDef && (! HaveElse), "Unexpected #ELSEIF must be closed before starting a new one!");
					if (MuteByIfDef) {
						auto outcome{ true };
						for (size_t p = 2; p < ins->Words.size(); p++) {
							outcome = outcome && Defs[ins->Words[p]->UpWord];
						}
						MuteByIfDef = !outcome;
					} else MuteByIfDef = true;
				} else if (Opdracht == "ELSE") {
					TransAssert(HaveIfDef && (!HaveElse), "Unexpected #ELSE must be closed before starting a new one!");
					MuteByIfDef = !MuteByIfDef;
					HaveElse = true;
				} else if (Opdracht == "ENDIF" || Opdracht == "FI") {
					TransAssert(HaveIfDef || HaveElse, "Unexpected #ENDIF");
					MuteByIfDef = false;
					HaveElse = false;
					HaveIfDef = false;
				} else if (Opdracht == "WARN") {
					QCol->Warn("\x07 " + Para);
				} else if (Opdracht == "CONFIG") {
					TransAssert(ins->Words.size() >= 4, "Incomplete #CONFIG");
					TransConfig[Upper(Para)] = ins->Words[3];
				} else {
					TransError("Unknown compiler directive #" + Opdracht);
				}
			} else if (!First) {
				if (!MuteByIfDef) {
					First = true;
					ins->Kind = InsKind::HeaderDefintiion; // Script or Module
				}
			} else if (MuteByIfDef) {
				ins->Kind = InsKind::MutedByIfDef;
			} else if (ins->Words[0]->UpWord == "INIT") {
				TransAssert(Ret.ScopeLevel() == 0, "INIT scopes can only be started from the root scope");
				TransAssert(ins->Words.size() == 1, "INIT does not take any parameters or anything");
				ins->Kind = InsKind::StartInit;
				Ret.Scopes.push_back(ScopeKind::Init);
				HasInit = true;
			} else if (ins->Words[0]->UpWord == "END") {
				TransAssert(ins->Words.size() == 1, "END does not take any parameters or anything");
				switch (Ret.Scope()) {
				case ScopeKind::Root:
					TransError("END without any start of a scope");
				case ScopeKind::Repeat:
					TransError("REPEAT scope can only be ended with either UNTIL, FOREVER or LOOPWHILE");
				}
				ins->Kind = InsKind::EndScope;
				Ret.Scopes.pop_back();
			} else if (ins->Words[0]->Kind==WordKind::Identifier) {
				switch (Ret.Scope()) {
				case ScopeKind::Root:
					TransError("General instruction not possible in root scope");
				case ScopeKind::Class:
					TransError("General instruction not possible in class scope");
				case ScopeKind::Group:
					TransError("General instruction not possible in group scope");
				case ScopeKind::QuickMeta:
					TransError("General instruction not possible in QuickMetaScope");
				}
				ins->Kind = InsKind::General;
			} else {
				QCol->Error("The next kind of instruction is not yet understood, due to the translator not yet being finished (Line #" + std::to_string(LineNumber) + ")");
			}

		}
		{
			auto LineNumber = Ret.Instructions[Ret.Instructions.size() - 1]->LineNumber;
			TransAssert(Ret.ScopeLevel() == 0, TrSPrintF("Unclosed scope (%d)", (int)Ret.Scope()));
		}

		return Ret.Trans;
	}

	Translation Translate(std::string source, std::string srcfile, Slyvina::JCR6::JT_Dir JD, bool debug) {
		return Translate(Split(source, '\n'), srcfile, JD);
	}

}