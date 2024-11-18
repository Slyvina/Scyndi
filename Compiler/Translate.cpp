// License:
// 
// Scyndi
// Translator
// 
// 
// 
// 	(c) Jeroen P. Broks, 2022, 2023, 2024
// 
// 		This program is free software: you can redistribute it and/or modify
// 		it under the terms of the GNU General Public License as published by
// 		the Free Software Foundation, either version 3 of the License, or
// 		(at your option) any later version.
// 
// 		This program is distributed in the hope that it will be useful,
// 		but WITHOUT ANY WARRANTY; without even the implied warranty of
// 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// 		GNU General Public License for more details.
// 		You should have received a copy of the GNU General Public License
// 		along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// 	Please note that some references to data like pictures or audio, do not automatically
// 	fall under this licenses. Mostly this is noted in the respective files.
// 
// Version: 24.11.18
// End License

#include <Slyvina.hpp>
#include <SlyvString.hpp>
#include <SlyvVecSearch.hpp>
#include <SlyvQCol.hpp>
#include <SlyvStream.hpp>
#include <SlyvMD5.hpp>

#include <Lunatic.hpp>

#include "Translate.hpp"
#include "ScyndiGlobals.hpp"
#include "Keywords.hpp"
#include "ScyndiProject.hpp"

#undef TransDebug


#ifdef TransDebug
#define Chat(abc) QCol->LGreen("SCYNDI TRANSLATOR DEBUG> "); QCol->White(""); std::cout << abc << std::endl;
#else
#define Chat(abc)
#endif

#define DbgLineCheck if (debug && (!Ins->ScopeData->DidReturn)) *Trans += TrSPrintF("Scyndi.Debug.Line(Scyndi.Debug.StateName,\"%s\",%d)\t",srcfile.c_str(),Ins->LineNumber)

using namespace Slyvina;
using namespace Slyvina::Units;

namespace Scyndi {

	bool TransVerbose{ false };

	enum class InsKind {
		Unknown, HeaderDefintion, General,
		QuickMeta, IfStatement, ElseIfStatement, ElseStatement,
		WhileStatement, Increment, Decrement, DeclareVariable,
		DefineFunction, CompilerDirective, WhiteLine,
		Return, MutedByIfDef, StartInit, EndScope,
		StartFor, StartForEach, Declaration,
		StartDeclarationScope, StartFunction, StartMethod,
		StartDo,
		Switch, Case, Default,
		FallThrough, Defer, StartClass,
		StartGroup, Repeat, Until,
		Forever, LoopWhile, StartMetaMethod,
		StartQuickMeta,PropertyGet,PropertySet, 
		ExternImport,
		Break
	};
	enum class WordKind { 
		Unknown, String, Number, 
		KeyWord, Identifier, IdentifierClass,
		Operator, Macro, Comma, 
		Field, CompilerDirective, HaakjeOpenen,
		HaakjeSluiten 
	};
	enum class ScopeKind {
		Unknown, General, Root,
		Repeat, Method, Class,
		Group, Init, QuickMeta,
		ForLoop, IfScope, ElIf,
		ElseScope, Declaration, WhileScope,
		Switch, Case, Default,
		FunctionBody, Defer , Do
	};

	enum class VarType { Unknown, Integer, String, Table, Number, Boolean, CustomClass, pLua, Byte, UserData, Delegate, Void, Var };

	struct Arg { std::string Name{ "" }, ref{ "" }, BaseValue{ "" }; VarType dType{ VarType::Unknown }; bool HasBaseValue{ false }; };

	class _Declaration {
	public:
		static std::map<std::string, VarType> S2E;
		VarType Type{ VarType::Unknown };
		bool
			IsGet{ false }, IsSet{ false },
			IsRoot{ false }, // Only checked if not global
			IsStatic{ false },
			IsGlobal{ false }, // May not be used in classes and groups			
			IsReadOnly{ false },
			IsFinal{ false },
			IsConstant{ false };
		std::string
			CustomClass{ "" },
			BoundToClass{ "" };
		
		static std::string E2S(VarType T) {
			for (auto& K : S2E) if (K.second == T) return K.first;
			return "";
		}
	};
	std::map<std::string, VarType> _Declaration::S2E{
		{"INT", VarType::Integer},
		{"NUMBER",VarType::Number},
		{"STRING",VarType::String},
		{"TABLE",VarType::Table},
		{"BOOL",VarType::Boolean},
		{"PLUA",VarType::pLua},
		{"DELEGATE",VarType::Delegate},
		{"BYTE",VarType::Byte},
		{"VAR",VarType::Var},
		{"VOID",VarType::Void}
	};

	typedef std::shared_ptr<_Declaration> Declaration;

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
			Chat("New Word <" << (int)K << "> string: " << W);
			return ret;			
		}

		static Word NewWord(WordKind K, char C) {
			std::string W{ "" }; W += C;
			auto ret{ std::make_shared<_Word>() };
			ret->Kind = K;
			ret->TheWord = W;
			ret->UpWord = Upper(W);
			Chat("New Word <" << (int)K << "> Char: " << (int)C << "/" << W);
			return ret;
		}

		static Word NewWord(std::string W) {
			auto ret{ std::make_shared<_Word>() };
			Chat("New Word <AUTO> string: " << W);
			ret->TheWord = W;
			ret->UpWord = Upper(W);
			if (ret->TheWord[0] == '$') {
				ret->Kind = WordKind::Identifier;
				ret->UpWord = Upper(ret->TheWord).substr(1);
				if (!ret->UpWord.size()) ret->UpWord = "___DOLLARSIGN";
			} else if (ret->TheWord[0]=='@') {
				ret->Kind = WordKind::IdentifierClass;
				ret->UpWord = Upper(ret->TheWord).substr(1);
				if (!ret->UpWord.size()) ret->UpWord = "___DOLLARSIGN";
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

	class _Scope;
	typedef std::shared_ptr<_Scope> Scope;
	class _TransProcess;
	struct _Instruction {
		Declaration DecData{ nullptr };
		_TransProcess* TransParent{ nullptr };
		InsKind Kind{ InsKind::Unknown };
		std::string SourceFile{""};
		uint32 LineNumber{ 0 };
		std::string RawInstruction{ "" };
		std::vector<Word> Words{};
		std::string Comment{ "" };
		std::shared_ptr<std::vector<VecString>> switchcase{ nullptr };
		bool* switchHasDefault{ nullptr };
		std::string* SwitchName{ nullptr };
		//_Scope* Parent;
		uint64 ScopeLevel{ 0 };
		Scyndi::Scope ScopeData{ nullptr };
		ScopeKind Scope{ ScopeKind::Unknown };
		size_t ForEachExpression{ 0 };
		std::vector < std::string > ForVars{};
		StringMap ForTrans{ NewStringMap() };		
		//std::string ForStart{ "0" }, ForTo{ "0" }, ForStep{ "1" };  // Although only numbers processed, in translation this is the better ride
		Scyndi::Scope NextScope{ nullptr };
		
	};
	typedef std::shared_ptr<_Instruction> Instruction;

	
class _Scope {
public:
	bool DidReturn{ false }; // Needed to make sure that nothing could be placed after the return command (Lua doesn't accept that).
	ScopeKind Kind;
	StringMap LocalVars{ NewStringMap() };
	std::map<std::string, size_t> LocalDeclaLine{};
	std::string ScopeLoc{ "" }; // The metatable containing the locals. Empty if not needed.
	std::string switchID{ "" };
	bool switchHasDefault{ false };
	std::shared_ptr<std::vector<VecString>> switchCases{ nullptr };
	bool caseFallThrough{ false }; // Only works in case scopes.
	size_t caseCount{ 0 }; // Only for the translation itself so the labels can be created properly.
	_Scope* Parent{ nullptr };
	Declaration DecData{ nullptr };
	std::string DeferID{ "" };
	std::string ClassID{ "" };

	_Scope* DecScope() {
		if (Kind == ScopeKind::Declaration) return Parent; else return this;
	}

	Scope Breed() {
		auto ret = std::make_shared<_Scope>();
		ret->Parent = this;
		return ret;
	}

	std::string Identifier(Translation Trans, size_t lnr, std::string _id, bool ignoreglobals = false) {
		/*
		if (Macros->count(Upper(_id))) {
			auto subcode{ (*Macros)[Upper(_id)] };
			if (Prefixed(Upper(subcode), "::PLUA::")) {
				return subcode.substr(8);
			}
		}
		//*/
		if (_id[0] == '@') {
			_id = _id.substr(1);
			if (Trans->Classes->count(_id)) {
				auto CL{ Trans->Classes };
				return (*CL)[_id];
			}
			return "";
		}
		if (_id[0] == '$') _id = _id.substr(1);		
		Trans2Upper(_id);
		// Is this a local?
		for (auto fscope = this; fscope; fscope = fscope->Parent) {
			if (!LocalDeclaLine.count(_id)) LocalDeclaLine[_id] = 0;
			//std::cout << "Local check " << _id << " in scope " << (uint64)fscope << "(" << (uint32)fscope->Kind<< "); Found: " << fscope->LocalVars->count(_id) << "; Declared in line: " << LocalDeclaLine[_id] << "; Instruction line: " << lnr << std::endl; // DEBUG ONLY!!!
			if (fscope->LocalVars->count(_id) && lnr >= LocalDeclaLine[_id]) {
				auto LV{ fscope->LocalVars };
				return (*LV)[_id];
			}
		}
		/* Copied from elsewhere, for I have been a stupid idiot!
	for (size_t i = Scopes.size(); i > 0; --i) {
		size_t idx{ i - 1 };
		auto Sc{ GetScope(idx) };
		if (Sc->LocalVars->count(_id)) {
			auto LV{ Sc->LocalVars };
			return (*LV)[_id];
		}
	}
	*/
		if (ignoreglobals) {
			//QCol->Warn("INGORE GLOBALS FOR THIS IDENTIFIER: " + _id); // debug only
			return "";
		}
		if (CoreGlobals.count(_id)) return CoreGlobals[_id];
		if (Trans->GlobalVar->count(_id)) { auto GV{ Trans->GlobalVar }; return (*GV)[_id]; }
		if (Trans->Classes->count(_id)) { auto CL{ Trans->Classes }; return (*CL)[_id]; }
		return ""; // Empty string just means unrecognized
	}

	VarType FunctionScopeType() {
		auto Check{ this };
		do {
			switch (Check->Kind) {
			case ScopeKind::Init:
			case ScopeKind::Root:
			case ScopeKind::Defer:
				return VarType::Void;
			case ScopeKind::FunctionBody:
			case ScopeKind::Method:
				return Check->DecData->Type;
			default:
				Check = Check->Parent;
				if (!Check) {
					QCol->Error("Scope function type error (internal error! Please report to Jeroen P. Broks)");
					exit(12345);
				}
			}
		} while (true);
	}
	std::string DeferLine() {
		auto Check{ this };
		do {
			switch (Check->Kind) {
			case ScopeKind::Init:
			case ScopeKind::FunctionBody:
			case ScopeKind::Method:
				if (DeferID.size())
					return "for _,ifunc in ipairs(" + DeferID + ") do ifunc() end;\t";
				else
					return "";
			default:
				Check = Check->Parent;
				if (!Check) {
					QCol->Doing("Debug", (int)this->Kind);
					QCol->Doing("Debug", this->ClassID);
					QCol->Error("Scope function defer error (internal error! Please report to Jeroen P. Broks)");
					exit(12345);
				}
			}
		} while (true);
	}
};
	

	class _TransProcess {
	public:
		//_Scope RootScope{};
		std::vector < Instruction > Instructions;
		Translation Trans{};
		std::vector<Scope> Scopes;
		std::map<std::string, std::vector<std::string>> Fields{};
		std::map<std::string, Property> RootProperties{};
		std::map<std::string, std::map<std::string, Property>> ClassProperty{};
		Scope RootScope;
		Scope GetScope() {
			auto lvl{ ScopeLevel() };
			if (lvl == 0) return RootScope; //ScopeKind::Root;						
			return Scopes[lvl - 1];
		}
		void PushScope(ScopeKind K) {
			//auto NS{ std::make_shared<_Scope>() };
			auto NS{ GetScope()->Breed() };
			NS->Kind = K;
			Scopes.push_back(NS);
		}
		uint64 ScopeLevel() { return Scopes.size(); };
		Scope GetScope(size_t lvl) {
			if (lvl == 0) return RootScope; //ScopeKind::Root;						
			return Scopes[lvl - 1];
		}
		ScopeKind ScopeK() { return GetScope()->Kind; }
		_TransProcess() {
			RootScope = std::make_shared<_Scope>();
			RootScope->Kind = ScopeKind::Root;
		}

		std::string Identifier(std::string _id,bool ignoreglobals=false) {
			if (_id[0] == '@') {
				_id = _id.substr(1);
				if (Trans->Classes->count(_id)) {
					auto CL{ Trans->Classes };
					return (*CL)[_id];
				}
				return "";
			}
			if (_id[0] == '$') _id = _id.substr(1);
			Trans2Upper(_id);
			for (size_t i = Scopes.size(); i > 0; --i) {
				size_t idx{ i - 1 };
				auto Sc{ GetScope(idx) };
				if (Sc->LocalVars->count(_id)) {
					auto LV{ Sc->LocalVars };
					return (*LV)[_id];
				}
			}
			if (ignoreglobals) return "";
			if (CoreGlobals.count(_id)) return CoreGlobals[_id];
			if (Trans->GlobalVar->count(_id)) { auto GV{ Trans->GlobalVar }; return (*GV)[_id]; }
			if (Trans->Classes->count(_id)) { auto CL{ Trans->Classes }; return (*CL)[_id]; }
			return ""; // Empty string just means unrecognized
		}
	};

	static std::string _TLError{ "" };
	std::string TranslationError() {
		return _TLError;
	}
#define TransError(Err) { _TLError=Err; _TLError += " in line #"+std::to_string(LineNumber)+" ("+srcfile+")"; return nullptr; }
#define TransAssert(Condition,Err) { if (!(Condition)) TransError(Err); }
#define BoolError(Err) { _TLError=Err; _TLError += " in line #"+std::to_string(LineNumber)+" ("+srcfile+")"; return false; }
#define BoolAssert(Condition,Err) { if (!(Condition)) BoolError(Err); }


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
		int TimeOut = 10000;
		while (pos < Line.size()) {
			auto ch{ Line[pos] };
			if (!(TimeOut--)) {
				TransError(TrSPrintF("Chopping timeout on position %d/%d (%c)", pos, (int)Line.size(), ch));
			}
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
					}
					FirstChar = false;
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
					}
					FirstChar = false;
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
					(ch == '_')
				};
				//std::cout << "Forming Word: " << ch << ">>" << GoOn << "\n"; // debug
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
						FormWord = "0."; FormWord += next;
						FormNumber = true;
						FormNumberHex = false;
						pos += 2;
					} else if (next == '_' || (next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z')) {
						FormingWord = true;
						FormWord = "."; FormWord += next;
						pos += 2;
						//std::cout << "INDEX! " << FormWord << "\n"; // debug
					} else if (next == '.') {
						Ret->Words.push_back(_Word::NewWord(".."));
						pos += 2;
					}
				} break;
				case '!':
					TransAssert(pos < Line.size() - 1, "! cannot be at the end of a line");
					if (Line[pos + 1] == '=') {
						Ret->Words.push_back(_Word::NewWord("!="));
						pos += 2;
					} else {
						Ret->Words.push_back(_Word::NewWord("!"));
						pos++;
					} break;
				case ',':
					Ret->Words.push_back(_Word::NewWord(","));
					pos++;
					break;
				case '"':
					InString = true;
					pos++;
					break;
				case '\'':
					InCharSeries = true;
					FirstChar = true;
					pos++;
					break;
				case '@': // Forces a class refereces
				case '$': // Forces are referrence to any other kind of identifyer
					FormingWord = true;
					FormWord = ch;
					pos++;
					break;
				case '=':
					Chat(pos << " = ");
					TransAssert(pos < Line.size() - 1, "There can never be an = at the end of a line");
					if (Line[pos + 1] == '=') {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "=="));
						pos += 2;
					} else {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "="));
						pos += 1;						 
					}
					break;
				case '>':
				case '<': {
					Chat(pos << " >< ");
					TransAssert(pos < Line.size() - 1, "There can never be a < or > at the end of a line");
					std::string W{ "" }; W += ch;
					if (Line[pos + 1] == '=') {
						W += '=';
						pos += 2;
					} else {						
						pos += 1;
					}
					Ret->Words.push_back(_Word::NewWord(WordKind::Operator, W));
					break;
				} break;
				case '+':
					TransAssert(pos < Line.size() - 1, "There can never be a + at the end of a line");
					if (Line[pos + 1] == '=') {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "+="));
						pos += 2;
					} else if (Line[pos + 1] == '+') {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "++"));
						pos += 2;
					} else {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "+"));
						pos++;
					} 
					break;
				case '-':
					TransAssert(pos < Line.size() - 1, "There can never be a - at the end of a line");
					if (Line[pos + 1] == '=') {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "-="));
						pos += 2;
					} else if (Line[pos + 1] == '-') {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "--"));
						pos += 2;
					} else {
						Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "-"));
						pos++;
					}
					break;
				case '^':
				case '*':
				case '/':
				case '%':
				case '[':
				case ']':
				case '{':
				case '}':
					//Chat("Plus/Minus/Times");
					if (FormingWord) {
						Ret->Words.push_back(_Word::NewWord(FormWord));
						FormWord = "";
						FormingWord = false;
					}
					Ret->Words.push_back(_Word::NewWord(WordKind::Operator, ch));
					pos++;
					break;
				case ':':
					TransAssert(pos < Line.size() - 1, ": cannot be at the end of the line");
					FormingWord = true;
					FormWord = "."; FormWord += Line[pos+1];
					pos += 2;
					break;
				case '&':
					TransAssert(pos < Line.size() - 1 && Line[pos + 1] == '&', "& is not an operator. && is");
					Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "&&"));
					pos += 2;
					break;
				case '|':
					TransAssert(pos < Line.size() - 1 && Line[pos + 1] == '|', "| is not an operator. || is");
					Ret->Words.push_back(_Word::NewWord(WordKind::Operator, "||"));
					pos += 2;
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
#ifdef TransDebug
		for (size_t p = 0; p < Ret->Words.size(); p++) {
			if (!Ret->Words[p]) QCol->Warn(TrSPrintF("\x7\x7There is null word on position %d/%d found!", p, Ret->Words.size()));
		}
#endif
		return Ret; 
	}

	static std::vector<Instruction> ChopCode(Slyvina::VecString sourcelines, std::string srcfile, Slyvina::JCR6::JT_Dir JD, bool debug, std::map<String, String>*Macros) {
		Chat("Chopping " << srcfile);
		std::vector<Instruction> Ret;
		for (size_t _ln = 0; _ln < sourcelines->size(); _ln++) {
			auto LineNumber{ _ln + 1 };
			Chat("=> Line " << LineNumber << "/" << sourcelines->size());
			size_t pos{0};
			if (Prefixed(Upper(Trim((*sourcelines)[_ln])), "#MACRO")) {
				auto ok{ true };
				auto trsl{ Trim((*sourcelines)[_ln]) };
				auto p{ IndexOf(trsl,' ') };
				if (p < 0 || Upper(trsl.substr(0, p)) != "#MACRO") ok = false;
				if (ok) {
					auto mdef{ trsl.substr(p + 1) };
					p = IndexOf(mdef, ' ');
					if (p <= 0) {
						auto key{ Upper(mdef) };
						if (Macros->count(key)) Macros->erase(key);
					} else {
						auto key{ Upper(mdef.substr(0,p)) };
						(*Macros)[key] = mdef.substr(p + 1);
					}
				} else { QCol->Warn("Invalid #MACRO definition"); }				
			} else {
				for (auto M : *Macros) {
					(*sourcelines)[_ln] = StCIReplace((*sourcelines)[_ln], M.first, M.second);
				}
				while (pos >= 0 && pos < (*sourcelines)[_ln].size()) {
					Chat("=> Postion: " << pos << "/" << (*sourcelines)[_ln].size());
					auto Chopped = Chop((*sourcelines)[_ln], LineNumber, pos, srcfile);
					if (!Chopped) return std::vector<Instruction>();
					Ret.push_back(Chopped);
				}
			}
		}
		return Ret;
	}

	static std::string GetWordKind(WordKind K) {
		static std::map<WordKind, std::string> GWK{
			{WordKind::KeyWord, "Keyword"},
			{WordKind::Identifier,"Identifier"},
			{WordKind::HaakjeOpenen,"("},
			{WordKind::HaakjeSluiten,")"},
			{WordKind::String,"String"},
			{WordKind::Number,"Number"},
			{WordKind::Comma,"Comma"},
			{WordKind::Operator,"Operator"},
			{WordKind::Field,"Field"}
		};
		if (GWK.count(K)) return GWK[K];
		return "WK" + std::to_string((int)K);
	}

	static std::shared_ptr<std::string> Expression(Translation T,Instruction Ins, size_t start,bool ignoreglobals=false) {
		std::string Ret{ "" };
		auto srcfile{ Ins->SourceFile };
		auto LineNumber{ Ins->LineNumber };
		auto kw_new{ 0u };
		for (size_t pos = start; pos < Ins->Words.size(); pos++) {
			if (Ret.size()) Ret += " ";
			auto W{ Ins->Words[pos] };
			switch (kw_new) {
			case 2:
				TransAssert(W->Kind == WordKind::Identifier, "NEW syntax error");
				Ret += TrSPrintF("Scyndi.New(\"%s\" ", W->UpWord.c_str());
				kw_new--;
				break;
			case 1:
				TransAssert(W->TheWord == "(","( expected for constructor parameters");
				if (pos + 1 < Ins->Words.size() && Ins->Words[pos + 1]->UpWord != ")") Ret += ",";
				kw_new--;
				break;
			default:
				switch (W->Kind) {
				case WordKind::Identifier: {
					auto WT{ Ins->ScopeData->Identifier(T,Ins->LineNumber,W->UpWord,ignoreglobals) };					
					//if (!WT.size()) for (size_t pos = start; pos < Ins->Words.size(); pos++) { std::cout << "Word #" << pos << ": " << Ins->Words[pos]->TheWord << "\n"; } // debug only
					TransAssert(WT.size(), "Unknown identifier " + W->TheWord);
					Ret += WT;
				} break;
				case WordKind::HaakjeOpenen:
				case WordKind::HaakjeSluiten:
					Ret += W->TheWord;
					break;
				case WordKind::String:
					Ret += '"';
					Ret += W->TheWord;
					Ret += '"';
					break;
				case WordKind::Field:
					Ret += W->TheWord;
					break;
				case WordKind::Number:
				case WordKind::Comma:
				case WordKind::Operator:
					if (W->TheWord == "!")
						Ret += " not ";
					else if (W->TheWord == "!=")
						Ret += " ~= ";
					else if (W->TheWord == "||")
						Ret += " or ";
					else if (W->TheWord == "&&")
						Ret += " and ";
					else
						Ret += W->TheWord;
					break;
				default:
					if (W->UpWord == "TRUE")
						Ret += "true";
					else if (W->UpWord == "FALSE")
						Ret += "false";
					else if (W->UpWord == "NIL")
						Ret += "nil";
					else if (W->UpWord == "INFINITY")
						Ret += " ... ";
					else if (W->UpWord == "DIV")
						Ret += " // ";
					else if (W->UpWord == "NEW")
						kw_new = 2;
					else
						TransError(TrSPrintF("Unexpected %s (W%03d) '%s'", GetWordKind(W->Kind).c_str(), pos + 1, W->TheWord.c_str()));
				}
				break;
			}
		}
		switch (kw_new) {
		case 2: TransError("Class expected after NEW");
		case 1: Ret += ")"; break;
		case 0: break;
		default: TransError(TrSPrintF("Internal error! (kw_new=%d) Please report!", kw_new));
		}
		return std::shared_ptr<std::string>(new std::string(Ret));
	}

	bool TransUse(String&Para,_TransProcess *Ret,Slyvina::JCR6::JT_Dir JD,bool debug,String srcfile, uint32&LineNumber,bool force,GINIE dat,std::vector<String>* UseDependencies,std::map<String,String>*Macros) {
		auto bcFile{ Para }; if (debug) bcFile += ".debug"; bcFile += ".stb";
		auto srFile{ Para };
		auto skip{ false };
		//VecString GetMacros{ nullptr };
		if (JD->EntryExists(Para + ".Scyndi")) {
			srFile += ".Scyndi";
		} else if (JD->EntryExists(Para + ".lua")) {
			BoolError("No support yet for the inclusion of lua files through #USE yet!");
		} else if (JD->EntryExists(bcFile)) {
			skip = true;
		} else if (JD->DirectoryExists(Para+".ScyndiBundle")) {			
			for (auto& JDI : JD->_Entries) {
				if (ExtractExt(Upper(JDI.first)) == "SCYNDI" && ExtractDir(Upper(JDI.first)) == Upper(Para + ".ScyndiBundle")) {
					auto inc{ StripExt(JDI.second->Name()) };
					QCol->Doing("= Entry", inc);
					if (!TransUse(inc, Ret, JD, debug, srcfile, LineNumber, force, dat, UseDependencies,Macros)) return false;
				}
			}
			return true;
		} else BoolError("No way found to get any data about #USE request for " + Para);
		if (skip) {
			auto bcj{ JCR6::JCR6_Dir(JD->Entry(bcFile)->MainFile) };
			auto g{ ParseGINIE(bcj->GetString("Configuration.ini")) };
			BoolAssert(g, "Parsing GINIE failed! Delete the STB file and try again! ");
			BoolAssert(Upper(g->Value("Translation", "Target")) == "LUA", "Target error");
			BoolAssert(g->Value("Lua", "Version") == Slyvina::NSLunatic::_Lunatic::LuaVersion(), TrSPrintF("This translation is for Lua version %s. However this version of Scyndi works with Lua version %s", g->Value("Lua", "Version").c_str(), NSLunatic::_Lunatic::LuaVersion().c_str()));
			for (auto& glob : *g->List("Globals", "-List-")) {
				auto sub{ g->Value("Globals",glob) };
				BoolAssert(sub.size(), TrSPrintF("No substitute found for global %s", glob.c_str()));
				(*Ret->Trans->GlobalVar)[glob] = sub;
			}
			auto GetMacros = g->Values("Macros");
			QCol->Doing("-> Macros", GetMacros->size()); // DEBUG ONLY!!!
			for (auto M : *GetMacros) { 
				QCol->Doing("-> Macro " + M, g->Value("Macros", M)); // DEBUG ONLY!!
				(*Macros)[M] = g->Value("Macros", M); 
			}
		} else {
			auto CR{ Compile(dat,JD,srFile,debug,force) };
			BoolAssert(CR, "Compilation returned NULL (internal error. Please report!)");
			BoolAssert(CR->Result != CompileResult::Fail, TrSPrintF("#USE request for '%s' failed", Para.c_str()));
			if (CR->Result == CompileResult::Skip) {
				Verb("Status", "Up-to-date");
				auto bcj{ JCR6::JCR6_Dir(JD->Entry(bcFile)->MainFile) };
				CR->Data = ParseGINIE(bcj->GetString("Configuration.ini"));
			} else {
				if (force) Verb("Status", "Forced"); else Verb("Status", "Outdated");
			}
			for (auto& glob : *CR->Data->List("Globals", "-List-")) {
				auto sub{ CR->Data->Value("Globals",glob) };
				BoolAssert(sub.size(), TrSPrintF("No substitute found for global %s", glob.c_str()));
				(*Ret->Trans->GlobalVar)[glob] = sub;
			}
			auto GetMacros = CR->Data->Values("Macros");
			QCol->Doing("-> Macros", GetMacros->size()," (in "+Para+"\n"); // DEBUG ONLY!!!
			for (auto M : *GetMacros) { 
				QCol->Doing("-> Macro " + M, CR->Data->Value("Macros", M)); // DEBUG ONLY!!
				(*Macros)[M] = CR->Data->Value("Macros", M); 
			}
		}		
		
		UseDependencies->push_back(Para);
		return true;
	}

	Translation Translate(Slyvina::VecString sourcelines, std::string srcfile, Slyvina::JCR6::JT_Dir JD, GINIE dat, bool debug, bool force) {
		std::string StaticRegister = "ScyndiStaticRegister_" + md5(srcfile) + md5(CurrentDate()) + md5(CurrentTime());
		std::vector<std::string> UseDependencies{};
		std::map<String, String>Macros{};
		Verb("Compiling", srcfile);
		_TLError = "";
		_TransProcess Ret;
		Ret.Trans = std::make_shared<_Translation>();
		//uint64 ScopeLevel{ 0 };
		
		// Chopping
		Ret.Instructions = ChopCode(sourcelines, srcfile, JD, debug, &Macros);
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
						IncChopped = ChopCode(isrc, srcfile, JD, debug, &Macros);
						Ret.Trans->RealIncludes->push_back(_file->TheWord);
					} else if (JD->EntryExists(_file->TheWord)) {
						auto isrc = JD->GetLines(_file->TheWord);
						IncChopped = ChopCode(isrc, srcfile, JD, debug, &Macros);
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
#pragma region "Pre-processing"
		// Pre-Processing
		Verb("Pre-processing", srcfile);
		std::map<std::string, Word> TransConfig;
		std::map<std::string, bool> Defs;
		std::string ScriptName{ "" };
		bool
			MuteByIfDef{ false },
			HaveIfDef{ false },
			HaveElse{ false },
			First{ false },
			HasInit{ false }; // If there's at least one init this must be true.
		for (auto ins : Ret.Instructions) {
			ins->ScopeLevel = Ret.ScopeLevel();
			ins->ScopeData = Ret.GetScope();
			ins->Scope = Ret.GetScope()->Kind;
			Chat("Preprocessing in instruction on line #" << ins->LineNumber);
#ifdef TransDebug
			for (size_t i = 0; i < ins->Words.size(); i++) { Chat("Word " << i + 1 << "/" << ins->Words.size()<<"> "<<ins->Words[i]->TheWord); }
#endif
			auto LineNumber = ins->LineNumber;
			auto DecScope{ false }; // needed this way to end declaration scopes abruptly
			//*
			//Chat(ins->Words.size());
			if (ins->Words.size() && ins->Words[0]->UpWord == "CONSTRUCTOR") {
				Chat("Constructor!");
				TransAssert(ins->Scope == ScopeKind::Class, TrSPrintF("(%d) Constructors can only be created in classes",(int)ins->Scope));
				auto dec = std::make_shared<_Declaration>();
				DecScope = true;
				if (ins->Words.size() == 1) {
					ins->Words.push_back(_Word::NewWord("("));
					ins->Words.push_back(_Word::NewWord(")"));
				}
				TransAssert(ins->Words[1]->TheWord == "(", "Constructor syntax error");
				dec->BoundToClass = ins->ScopeData->ClassID;
				dec->IsConstant = true;
				dec->IsFinal = false;
				dec->IsGet = false;
				dec->IsGlobal = false;
				dec->IsReadOnly = true;
				dec->IsRoot = false;
				dec->IsStatic = false;
				dec->Type = VarType::Void;
				ins->DecData = dec;
				ins->ForEachExpression = 0;
				ins->Kind = InsKind::StartMethod;
				Ret.PushScope(ScopeKind::FunctionBody);
				ins->NextScope = Ret.GetScope();
				ins->NextScope->DecData = dec;
			} else if (ins->Words.size() && ins->Words[0]->UpWord == "DESTRUCTOR") {
				TransAssert(ins->Words.size() == 1, "DESTRUCTOR accepts no kind of paramters at all!");
				auto dec = std::make_shared<_Declaration>();
				ins->Words.push_back(_Word::NewWord("("));
				ins->Words.push_back(_Word::NewWord(")"));
				dec->BoundToClass = ins->ScopeData->ClassID;
				dec->IsConstant = true;
				dec->IsFinal = false;
				dec->IsGet = false;
				dec->IsGlobal = false;
				dec->IsReadOnly = true;
				dec->IsRoot = false;
				dec->IsStatic = false;
				dec->Type = VarType::Void;
				ins->DecData = dec;
				ins->ForEachExpression = 0;
				ins->Kind = InsKind::StartMethod;
				Ret.PushScope(ScopeKind::FunctionBody);
				ins->NextScope = Ret.GetScope();
				ins->NextScope->DecData = dec;
				DecScope = true;
				//std::cout << " ???? DESTRUCTOR IGNORED ???\n";
			} else if (ins->Words.size() && ins->Words[0]->UpWord == "EXTERN") { 
				ins->Kind = InsKind::ExternImport; 
			} else if (ins->Words.size() && (ins->Words[0]->UpWord == "GLOBAL" || ins->Words[0]->UpWord == "STATIC" || ins->Words[0]->UpWord == "CONST" || ins->Words[0]->UpWord == "READONLY"|| ins->Words[0]->UpWord == "GET" || ins->Words[0]->UpWord == "SET" || Prefixed(ins->Words[0]->UpWord, "@") || _Declaration::S2E.count(ins->Words[0]->UpWord))) {
				Chat("Will this be a variable declaration or a function definition? (Line: " << ins->LineNumber << ")");
				TransAssert(ScriptName.size(), "Header first");
				DecScope = true;
				size_t pos{ 0 };
				auto dec = std::make_shared<_Declaration>();
				ins->DecData = dec;
				for (pos = 0; pos < ins->Words.size() && (!Prefixed(ins->Words[pos]->UpWord, "@")) && (!_Declaration::S2E.count(ins->Words[pos]->UpWord)); pos++) {
					Chat("=> " << ins->Words[0]->UpWord);
					if (ins->Words[pos]->UpWord == "GLOBAL") dec->IsGlobal = true;
					if (ins->Words[pos]->UpWord == "STATIC") dec->IsStatic = true;
					if (ins->Words[pos]->UpWord == "CONST") dec->IsConstant = true;
					if (ins->Words[pos]->UpWord == "READONLY") dec->IsReadOnly = true;
					if (ins->Words[pos]->UpWord == "GET") dec->IsGet = true;
					if (ins->Words[pos]->UpWord == "SET") dec->IsSet = true;
				}
				TransAssert(pos < ins->Words.size(), "Incomplete declaration");
				if (Prefixed(ins->Words[pos]->UpWord, "@")) {
					dec->Type = VarType::CustomClass;
					dec->CustomClass = ins->Words[pos]->UpWord.substr(1);
				} else {
					TransAssert(_Declaration::S2E.count(ins->Words[pos]->UpWord), "Type error (Intenal error! Please report) ");
					dec->Type = _Declaration::S2E[ins->Words[pos]->UpWord];
				}
				Chat("Type decided " << (int)dec->Type);
				auto CScope{ Ret.GetScope() };
				if (CScope->Kind == ScopeKind::Declaration) Ret.Scopes.pop_back();
				auto PScope{ Ret.GetScope() };
				//std::cout << "Dec: Line:" << ins->LineNumber << "; Pos:" << pos << "; Words:" << ins->Words.size() << "\n";
				if (pos == ins->Words.size() - 1) {
					ins->Kind = InsKind::StartDeclarationScope;
					Ret.PushScope(ScopeKind::Declaration);
					Ret.GetScope()->DecData = dec;
				} else {
					ins->ForEachExpression = pos + 1;
					if (pos + 2 >= ins->Words.size());
					if (ins->Words.size() > ins->ForEachExpression + 1 && ins->Words[ins->ForEachExpression + 1]->UpWord == "(") {
						ins->Kind = InsKind::DefineFunction;
						if (PScope->Kind == ScopeKind::Class) ins->Kind = InsKind::StartMethod;
						//TransError("Defining functions still in preparation");
						Ret.PushScope(ScopeKind::FunctionBody);
						Ret.GetScope()->DecData = dec;
						ins->NextScope = Ret.GetScope();
						TransAssert(!dec->IsGet, "GET is not allowed in function definition");
						TransAssert(!dec->IsSet, "SET is not allowed in function definition");
					} else if (dec->IsGet && dec->IsSet) {
						TransError("GET and SET conflict");
					} else  if (dec->IsGet) {
						ins->Kind = InsKind::PropertyGet;
						Ret.PushScope(ScopeKind::FunctionBody);
						Ret.GetScope()->DecData = dec;
						ins->NextScope = Ret.GetScope();						
					} else if (dec->IsSet) {
						ins->Kind = InsKind::PropertySet;
						Ret.PushScope(ScopeKind::FunctionBody);
						Ret.GetScope()->DecData = dec;
						ins->NextScope = Ret.GetScope();
					} else
						ins->Kind = InsKind::Declaration;
				}
				if (ins->Kind == InsKind::Declaration) TransAssert(dec->Type != VarType::Void, "Void reserved for functions only");

				if (dec->IsGlobal) {
					TransAssert(PScope->Kind == ScopeKind::Root, "Global declaration only possible in the root scope");
				} else if (PScope->Kind == ScopeKind::Root || (PScope->Kind == ScopeKind::Declaration && PScope->Parent->Kind == ScopeKind::Root)) {
					dec->IsRoot = true;
				} else if (PScope->Kind == ScopeKind::Class) {					
					dec->BoundToClass = ins->ScopeData->ClassID;
					//TransError("Class declarations not yet supported");
				
				} else if (PScope->Kind == ScopeKind::Group) {
					TransError("Group declarations not yet supported");
				}

				if (dec->Type == VarType::pLua) {
					TransAssert(!dec->IsConstant, "pLua variables can NOT be constant");
					TransAssert(!dec->IsReadOnly, "pLua variables can NOT be read-only");
				}
			} else if (Ret.GetScope()->Kind == ScopeKind::Declaration) {
				if (ins->Words.size() && ins->Words[0]->UpWord == "END") {
					Ret.Scopes.pop_back();
					DecScope = true;					
				} else if (ins->Words.size() && ins->Words[0]->Kind != WordKind::Identifier) {
					Ret.Scopes.pop_back();
					DecScope = false;
					//std::cout << "Ending";
				} else if (ins->Words.size() ) {
					//TransError("Scope style declarations not yet supported");
					ins->ForEachExpression = 0;
					ins->Kind = InsKind::Declaration;
					ins->DecData = Ret.GetScope()->DecData;
					DecScope = true;
					//std::cout << "Hello? Anybody home?\n";
					//std::cout << (ins->Words[0]->Kind == WordKind::Identifier) << ": "<<ins->Words[0]->UpWord<<"; ID(Line: " << ins->LineNumber << ")\n";
				}
			}
			//*/
			if (DecScope) {
				// Do NOTHING
			} else if (!ins->Words.size()) {
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
					//for (auto i = 0; i < ins->Words.size(); ++i) printf("%d/%d> %s\n", i, (int)ins->Words.size(),ins->Words[i]->TheWord.c_str());
					TransAssert(ins->Words.size() >= 3, "Incomplete #IF statement");
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
				} else if (Opdracht == "USE") {
					TransAssert(ins->Words.size() == 3, "#USE syntax error");
					TransAssert(ins->Words[2]->Kind == WordKind::String, "String expected to determine the dependency to load with #USE");
					Verb("Use request", Para);
					Ret.Trans->Data->AddNew("Dependencies", "List", Para);
					if (!TransUse(Para, &Ret, JD, debug, srcfile, LineNumber,force,dat,&UseDependencies,&Macros)) return nullptr;
					/* Original (in case this goes wrong)
					auto bcFile{ Para }; if (debug) bcFile += ".debug"; bcFile += ".stb";
					auto srFile{ Para }; 
					auto skip{ false };
					
					//auto cfFile{ bcFile + "/Configuration.ini" };
					if (JD->EntryExists(Para + ".Scyndi")) {
						srFile += ".Scyndi";
					} else if (JD->EntryExists(Para + ".lua")) {						
						TransError("No support yet for the inclusion of lua files through #USE yet!");
					} else if (JD->EntryExists(bcFile)) {
						skip = true;
					} else TransError("No way found to get any data about #USE request for " + Para);
					if (skip) {
						auto bcj{ JCR6::JCR6_Dir(JD->Entry(bcFile)->MainFile) };
						auto g{ ParseGINIE(bcj->GetString("Configuration.ini")) };
						TransAssert(g, "Parsing GINIE failed! Delete the STB file and try again! ");
						TransAssert(Upper(g->Value("Translation", "Target"))=="LUA", "Target error");
						TransAssert(g->Value("Lua", "Version") == Slyvina::NSLunatic::_Lunatic::LuaVersion(), TrSPrintF("This translation is for Lua version %s. However this version of Scyndi works with Lua version %s", g->Value("Lua", "Version").c_str(), NSLunatic::_Lunatic::LuaVersion().c_str()));
						for (auto& glob : *g->List("Globals", "-List-")) {
							auto sub{ g->Value("Globals",glob) };
							TransAssert(sub.size(), TrSPrintF("No substitute found for global %s", glob.c_str()));
							(*Ret.Trans->GlobalVar)[glob] = sub;
						}
					} else {
						auto CR{ Compile(dat,JD,srFile,debug,force) };
						TransAssert(CR, "Compilation returned NULL (internal error. Please report!)");
						TransAssert(CR->Result != CompileResult::Fail, TrSPrintF("#USE request for '%s' failed", Para.c_str()));
						if (CR->Result == CompileResult::Skip) {
							Verb("Status", "Up-to-date");
							auto bcj{ JCR6::JCR6_Dir(JD->Entry(bcFile)->MainFile) };							
							CR->Data = ParseGINIE(bcj->GetString("Configuration.ini"));
						} else {
							if (force) Verb("Status", "Forced"); else Verb("Status", "Outdated");
						}
						for (auto& glob : *CR->Data->List("Globals", "-List-")) {
							auto sub{ CR->Data->Value("Globals",glob) };
							TransAssert(sub.size(), TrSPrintF("No substitute found for global %s", glob.c_str()));
							(*Ret.Trans->GlobalVar)[glob] = sub;
						}
					}
					UseDependencies.push_back(Para);
					//*/
				} else {
					TransError("Unknown compiler directive #" + Opdracht);
				}
			} else if (!First) {
				if (!MuteByIfDef) {
					First = true;
					ins->Kind = InsKind::HeaderDefintion; // Script or Module
					if (ins->Words[0]->UpWord == "SCRIPT") {
						Ret.Trans->Kind = ScriptKind::Script;
						std::string _id = "MAINSCRIPT";
						if (ins->Words.size() > 2) {
							TransAssert(ins->Words[1]->Kind == WordKind::Identifier, "Identifier expected");
							_id = ins->Words[1]->UpWord;
							TransAssert(Ret.Identifier(_id) == "", "Script header creates duplicate identifier");
						}
						(*Ret.Trans->GlobalVar)[_id] = "Scyndi.CLASSES[\"" + _id + "\"]";
						ScriptName = _id;
					} else if (ins->Words[0]->UpWord == "MODULE") {
						Ret.Trans->Kind = ScriptKind::Script;
						std::string _sid = Upper(StripAll(srcfile));
						std::string _id{ "" };
						if (ins->Words.size() > 2) {
							TransAssert(ins->Words[1]->Kind == WordKind::Identifier, "Identifier expected");
							_id = ins->Words[1]->UpWord;
							TransAssert(Ret.Identifier(_id) == "", "Script header creates duplicate identifier");
						} else {
							for (size_t p = 0; p < _sid.size(); p++) {
								if (
									(_sid[p] >= 'A' && _sid[p] <= 'Z') ||
									(_sid[p] >= '0' && _sid[p] <= '9') ||
									(_sid[p] == '_')
									)
									_id += _sid[p];
								else if (_sid[p] == ' ')
									_id += '_';
								else
									TransError("Cannot generate valid identifier name from "+_sid);
							}
						}
						(*Ret.Trans->GlobalVar)[_id] = "Scyndi.CLASS[\"" + _id + "\"]";
						Ret.Trans->Data->Value("Globals", _id, (*Ret.Trans->GlobalVar)[_id]);
						Ret.Trans->Data->Add("Globals", "-list-", _id);
						ScriptName = _id;
					} else {
						TransError("Script header expected");
					}
				}
			} else if (MuteByIfDef) {
				ins->Kind = InsKind::MutedByIfDef;
			} else if (ins->Scope==ScopeKind::QuickMeta) {
				ins->Kind = InsKind::StartMetaMethod;
				if (ins->Words[0]->UpWord == "DESTRUCTOR") {
					ins->Words[0]->UpWord = "GC";
					ins->Words[0]->TheWord = "GC";
				} 
				if (ins->Words[0]->UpWord == "END") {
					ins->Kind = InsKind::EndScope;
					Ret.Scopes.pop_back();
				} else {
					TransAssert(MetaMethods.count(ins->Words[0]->UpWord), ins->Words[0]->UpWord + " is not a known metamethod for QUICKMETA");
					Ret.PushScope(ScopeKind::FunctionBody);
					ins->NextScope = Ret.GetScope();
					for (auto& K : MetaMethods[ins->Words[0]->UpWord]) {
						(*ins->NextScope->LocalVars)[K] = K;
					}
					(*ins->NextScope->LocalVars)["SELF"] = "self";
				}
			} else if (ins->Words[0]->UpWord == "INIT") {
				TransAssert(Ret.ScopeLevel() == 0, "INIT scopes can only be started from the root scope");
				TransAssert(ins->Words.size() == 1, "INIT does not take any parameters or anything");
				ins->Kind = InsKind::StartInit;
				Ret.PushScope(ScopeKind::Init);
				HasInit = true;
			} else if (ins->Words[0]->UpWord == "END") {
				TransAssert(ins->Words.size() == 1, "END does not take any parameters or anything");
				switch (Ret.ScopeK()) {
				case ScopeKind::Root:
					TransError("END without any start of a scope");
				case ScopeKind::Repeat:
					TransError("REPEAT scope can only be ended with either UNTIL, FOREVER or LOOPWHILE");
				case ScopeKind::Switch:
					TransError("CASEless SWITCH scope ended");
				case ScopeKind::Case:
				case ScopeKind::Default: {
					//ins->SwitchName = &Ret.GetScope()->Parent->Parent->switchID;
					auto SwS{ Ret.GetScope()->Parent };
					ins->Kind = InsKind::Case;
					ins->SwitchName = &SwS->switchID;
					Ret.Scopes.pop_back();
				} break;
				}
				ins->Kind = InsKind::EndScope;
				Ret.Scopes.pop_back();
			} else if (ins->Words[0]->UpWord == "SWITCH" || ins->Words[0]->UpWord == "SELECT") {
				auto S{ Ret.GetScope() };
				TransAssert(S->Kind != ScopeKind::Root, "Cannot start a SWITCH in the root scope");
				TransAssert(S->Kind != ScopeKind::Class, "Cannot start a SWITCH in a class scope");
				TransAssert(S->Kind != ScopeKind::Switch, "Double Switch");
				static size_t countswitch{ 0 };
				auto switchname{ TrSPrintF("_Scyndi_Switch_%08x_",countswitch++) }; switchname += md5(srcfile + switchname) + "_";
				ins->Kind = InsKind::Switch;
				Ret.PushScope(ScopeKind::Switch);
				auto NS{ Ret.GetScope() };
				NS->switchCases = std::make_shared<std::vector<VecString>>(); //NewVecString();
				NS->switchHasDefault = false;
				NS->switchID = switchname;
				ins->switchcase = NS->switchCases;
				ins->SwitchName = &NS->switchID;
				ins->switchHasDefault = &NS->switchHasDefault;
			} else if (ins->Words[0]->UpWord == "CASE") {
				TransAssert(Ret.GetScope()->Kind == ScopeKind::Switch || Ret.GetScope()->Kind == ScopeKind::Case, "CASE without SWITCH");
				if (Ret.GetScope()->Kind == ScopeKind::Case) Ret.Scopes.pop_back();
				Ret.PushScope(ScopeKind::Case);
				auto SwS{ Ret.GetScope()->Parent };
				ins->Kind = InsKind::Case;
				ins->SwitchName = &SwS->switchID;
				TransAssert(ins->Words.size() > 1, "CASE without values");
				auto CaseChain{ NewVecString() };
				SwS->switchCases->push_back(CaseChain);
				for (size_t p = 1; p < ins->Words.size(); p++) {
					//std::cout << "Case! Check " << p << " " << ins->Words.size() << (size_t)ins->Words[p].get()<<std::endl; // DEBUG only
					switch (ins->Words[p]->Kind) {
					case WordKind::Comma:
						break; // Comma's are optional now. You can place them if you think it makes your code more beautiful, but it's not needed.
					case WordKind::String:
						CaseChain->push_back(TrSPrintF("\"%s\"", ins->Words[p]->TheWord.c_str()));
						break;
					case WordKind::Number:
						for (size_t lp = 0; lp < ins->Words[p]->UpWord.size(); lp++) TransAssert(ins->Words[p]->UpWord[lp] != '.', "Only integers, strings, true and false can be used for casing");
						CaseChain->push_back(ins->Words[p]->UpWord);
						break;
					case WordKind::KeyWord:
						if (ins->Words[p]->UpWord == "TRUE" || ins->Words[p]->UpWord == "FALSE")
							CaseChain->push_back(ins->Words[p]->UpWord);
						else
							TransError("Invalid CASE value (keyword?)");
						break;
					default:
						TransError("Invalid CASE");
					}
				}
			} else if (ins->Words[0]->UpWord == "DEFAULT") {
				TransAssert(Ret.GetScope()->Kind == ScopeKind::Case, "DEFAULT without SWITCH or in a CASEless SWITCH");
				TransAssert(ins->Words.size(), "DEFAULT takes no further values");
				auto SwS{ Ret.GetScope()->Parent };
				SwS->switchHasDefault = true;
				ins->SwitchName = &SwS->switchID;
				Ret.Scopes.pop_back();
				Ret.PushScope(ScopeKind::Default);
				ins->Kind = InsKind::Default;
			} else if (ins->Words[0]->UpWord == "FALLTHROUGH") {
				TransAssert(Ret.GetScope()->Kind == ScopeKind::Case, "FALLTHROUGH without CASE");
				TransAssert(!Ret.GetScope()->caseFallThrough, "Duplicate FALLTHROUGH");
				Ret.GetScope()->caseFallThrough = true;
				ins->Kind = InsKind::FallThrough;
			} else if (ins->Words[0]->UpWord == "++") {
				ins->Kind = InsKind::Increment;
			} else if (ins->Words[ins->Words.size()-1]->TheWord=="++") {
				ins->Kind = InsKind::Increment;
				std::vector<Word> Nw{ _Word::NewWord(WordKind::Operator,"++") };
				for (size_t i = 0; i < ins->Words.size() - 1; i++) Nw.push_back(ins->Words[i]);
				ins->Words = Nw;
			} else if (ins->Words[0]->UpWord == "--") {
				ins->Kind = InsKind::Decrement;
			} else if (ins->Words[ins->Words.size() - 1]->TheWord == "--") {
				ins->Kind = InsKind::Decrement;
				std::vector<Word> Nw{ _Word::NewWord(WordKind::Operator,"--") };
				for (size_t i = 0; i < ins->Words.size() - 1; i++) Nw.push_back(ins->Words[i]);
				ins->Words = Nw;
			} else if (ins->Words[0]->UpWord == "RETURN") {
				ins->Kind = InsKind::Return;
			} else if (ins->Words[0]->UpWord == "DEFER") {
				TransAssert(ins->Scope == ScopeKind::FunctionBody || ins->Scope == ScopeKind::Method || ins->Scope == ScopeKind::Init, "Defer can only be used inside a function/method/init scope");
				ins->Kind = InsKind::Defer;
				Ret.PushScope(ScopeKind::Defer);
			} else if (ins->Words[0]->UpWord == "CLASS") {
				// start class
				TransAssert(ins->ScopeData->Kind == ScopeKind::Root, "Class can only be created in root scope");
				ins->Kind = InsKind::StartClass;
				if (ins->Words.size() < 2) TransError("Incomplete class defintion");
				TransAssert(ins->Words[1]->Kind == WordKind::Identifier, "Identifier for class expected");
				Ret.PushScope(ScopeKind::Class);
				auto SC{ Ret.GetScope() };
				SC->ClassID = ins->Words[1]->TheWord;
				(*Ret.Trans->GlobalVar)[SC->ClassID] = "SCYNDI.CLASSES[\"" + SC->ClassID + "\"]";
				Ret.Trans->Data->Add("CLASSES", "CLASS", SC->ClassID);
				(*SC->LocalVars)["SELF"] = "Scyndi.Class[\"" + SC->ClassID + "\"]";
				if (ins->Words.size() > 2) {
					TransAssert(ins->Words.size() == 4, "3 or more than 4 terms on a class? What are you doing?");
					TransAssert(ins->Words[2]->UpWord == "EXTENDS", "EXTENDS expected");
					TransError("Extended classes not yet supported");
				}
				Ret.Trans->Data->Value("Globals", SC->ClassID, (*SC->LocalVars)["SELF"]);
				Ret.Trans->Data->Add("Globals", "-list-", SC->ClassID);
			} else if (ins->Words[0]->UpWord == "EXTERN") {
				TransAssert(ins->Words.size() >= 3, TrSPrintF("EXTERN incomplete (%d/3)", ins->Words.size()));
				TransAssert(ins->Words[1]->Kind == WordKind::Identifier, "EXTERN expects identifier");
				TransAssert(ins->Words[2]->Kind == WordKind::String, "EXTERN expects string to define the pure external code");
				Ret.Trans->Data->Value("Globals", ins->Words[1]->UpWord, ins->Words[2]->TheWord);				
				Ret.Trans->Data->Add("Globals", "-list-", ins->Words[1]->UpWord);
				(*Ret.Trans->GlobalVar)[ins->Words[1]->UpWord] = ins->Words[2]->TheWord;
				QCol->Doing("- Extern", ins->Words[1]->TheWord);
			} else if (ins->Words[0]->UpWord == "QUICKMETA") {
				TransAssert(ins->Words.size() == 2, "QUICKMETA only needs an identifier name");
				TransAssert(ins->Words[1]->Kind == WordKind::Identifier, "Identifier expected to name QUICKMETA");
				auto QMName{ ins->Words[1]->UpWord };
				(*Ret.Trans->GlobalVar)[QMName] = "Scyndi.Globals[\"" + QMName + "\"]";				
				Ret.Trans->Data->Value("Globals", QMName, (*Ret.Trans->GlobalVar)[QMName]);
				Ret.Trans->Data->Add("Globals", "-list-", QMName);
				Ret.PushScope(ScopeKind::QuickMeta);
				ins->Kind = InsKind::StartQuickMeta;
			} else if (ins->Words[0]->UpWord == "REPEAT") {
				TransAssert(ins->Words.size() == 1, "REPEAT doesn't accept any parameters");
				ins->Kind = InsKind::Repeat;
				Ret.PushScope(ScopeKind::Repeat);
			} else if (ins->Words[0]->UpWord == "FOREVER") {
				TransAssert(ins->Words.size() == 1, "FOREVER doesn't accept any parameters");
				TransAssert(Ret.ScopeK() == ScopeKind::Repeat, "FOREVER without REPEAT");
				ins->Kind = InsKind::Forever;
				Ret.Scopes.pop_back();
			} else if (ins->Words[0]->UpWord == "UNTIL") {
				TransAssert(Ret.ScopeK() == ScopeKind::Repeat, "UNTIL without REPEAT");
				ins->Kind = InsKind::Until;
				Ret.Scopes.pop_back();
			} else if (ins->Words[0]->UpWord == "LOOPWHILE") {
				TransAssert(Ret.ScopeK() == ScopeKind::Repeat, "LOOPWHILE without REPEAT");
				ins->Kind = InsKind::LoopWhile;
				Ret.Scopes.pop_back();
			} else if (ins->Words[0]->UpWord=="BREAK") {
				ins->Kind = InsKind::Break;
				TransAssert(ins->Words.size() == 1, "BREAK accepts no parameters");
			} else if (ins->Words[0]->UpWord == "FOR") {
				TransAssert(ins->Words.size() > 1, "FOR without stuff");
				ins->Kind = InsKind::StartFor;
				Ret.PushScope(ScopeKind::ForLoop);
				std::vector < std::string > ForVars;
				bool isforeach{ false };
				size_t endexpression{ 0 };
				for (size_t wk = 1; true; wk += 2) {
					TransAssert(wk < ins->Words.size() - 1, "Incomplete FOR instruction");
					TransAssert(ins->Words[wk]->Kind == WordKind::Identifier, TrSPrintF("FOR syntax error! Identifier expected but got %s (%s)", GetWordKind(ins->Words[wk]->Kind).c_str(), ins->Words[wk]->TheWord.c_str()));
					auto varname{ ins->Words[wk]->UpWord }; if (varname[0] == '$') varname = varname.substr(1);
					TransAssert(varname[0] != '@', "You cannot use classes as FOR variable");
					ForVars.push_back(varname);
					if (ins->Words[wk + 1]->UpWord == "IN") { ins->Kind = InsKind::StartForEach; isforeach = true; endexpression = wk + 2; break; }
					if (ins->Words[wk + 1]->UpWord == "=") { endexpression = wk + 2; break; }
					TransAssert(ins->Words[wk + 1]->UpWord == ",", TrSPrintF("FOR syntax error! Unexpected %s(%s)! Expected ',', '=' or 'IN' instead! ", GetWordKind(ins->Words[wk + 1]->Kind).c_str(), ins->Words[wk + 1]->TheWord.c_str()));
				}
				TransAssert(ForVars.size(), "FOR without variables");
				if (!isforeach) {
					TransAssert(ForVars.size() == 1, "Too many variables declared for a regular FOR loop");
					//TransAssert(ins->Words.size() >= endexpression + 2, "Unfinished regular FOR");
				}
				for (auto& loc : ForVars) {
					static size_t count{ 0 };
					auto sv{ Ret.GetScope()->LocalVars };
					(*sv)[loc] = TrSPrintF("__Scyndi_For_Variable_%08x_%s", count++, md5(loc).c_str());
					Ret.GetScope()->LocalDeclaLine[loc] = ins->LineNumber;
					ins->ForVars.push_back(loc);
					(*ins->ForTrans)[loc] = (*sv)[loc];
					//std::cout << "FORVAR: ("<<(uint64)Ret.GetScope().get() << "): " << loc << " -> " << (*sv)[loc] << " (line " << Ret.GetScope()->LocalDeclaLine[loc] << ")" << std::endl;
				}
				ins->ForEachExpression = endexpression;
			} else if (ins->Words[0]->UpWord == "WHILE") {
				ins->Kind = InsKind::WhileStatement;
				Ret.PushScope(ScopeKind::WhileScope);
			} else if (ins->Words[0]->UpWord=="DO"){
				ins->Kind = InsKind::StartDo;
				Ret.PushScope(ScopeKind::Do);
			} else if (ins->Words[0]->UpWord == "IF") {
				ins->Kind = InsKind::IfStatement;
				Ret.PushScope(ScopeKind::IfScope);
			} else if (ins->Words[0]->UpWord == "ELSEIF" || ins->Words[0]->UpWord == "ELIF") {
				TransAssert(Ret.GetScope()->Kind == ScopeKind::IfScope || Ret.GetScope()->Kind == ScopeKind::ElIf, "ELSEIF without IF");
				Ret.Scopes.pop_back();
				Ret.PushScope(ScopeKind::ElIf);
				ins->Kind = InsKind::ElseIfStatement;
			} else if (ins->Words[0]->UpWord == "ELSE") {
				TransAssert(Ret.GetScope()->Kind == ScopeKind::IfScope || Ret.GetScope()->Kind == ScopeKind::ElIf, "ELSEIF without IF");
				TransAssert(ins->Words.size() == 1, "ELSE does not take any argumentation");
				Ret.Scopes.pop_back();
				Ret.PushScope(ScopeKind::ElseScope);
				ins->Kind = InsKind::ElseStatement;
			} else if (ins->Words[0]->Kind == WordKind::Identifier || ins->Words[0]->UpWord == "NEW") {
				switch (Ret.ScopeK()) {
				case ScopeKind::Root:
					TransError("General instruction not possible in root scope");
				case ScopeKind::Class:
					TransError("General instruction not possible in class scope");
				case ScopeKind::Group:
					TransError("General instruction not possible in group scope");
				case ScopeKind::QuickMeta:
					TransError("General instruction not possible in QuickMetaScope");
				}
				if (Ret.ScopeK() == ScopeKind::Declaration)
					ins->Kind = InsKind::Declaration;
				else
					ins->Kind = InsKind::General;
			} else {
				QCol->Error("The next kind of instruction is not yet understood, due to the translator not yet being finished (Line #" + std::to_string(LineNumber) + ")");
			}

		}
		{
			auto LineNumber = Ret.Instructions[Ret.Instructions.size() - 1]->LineNumber;
			TransAssert(Ret.ScopeLevel() == 0, TrSPrintF("Unclosed scope (%d)", (int)Ret.ScopeK()));
		}
		auto Trans{ &Ret.Trans->LuaSource };
		*Trans = "-- " + srcfile + "\n";
		*Trans += "--[[ Script Generated by Scyndi on " + CurrentDate() + ", " + CurrentTime() + "]]\n\n";
		if (debug) *Trans += "--[[ DEBUG TRANSLATION ]]--\n\n";
		*Trans += TrSPrintF("local %s = Scyndi.STARTCLASS(\"%s\",true,true,nil)\n", ScriptName.c_str(), ScriptName.c_str());
		*Trans += TrSPrintF("local %s = {}\n", StaticRegister.c_str());
		for (auto& dep : UseDependencies) *Trans += TrSPrintF("Scyndi.Use( \"%s\" )\n ", dep.c_str());
#pragma endregion

#pragma region "Class and group startups"
		// Class management
		// TODO!
		std::vector<std::string> ToSeal;
		for (auto Ins : Ret.Instructions) {
			auto LineNumber{ Ins->LineNumber };
			switch (Ins->Kind) {
			case InsKind::StartClass:
				ToSeal.push_back(Ins->Words[1]->UpWord);
				*Trans += "Scyndi.StartClass(\"";
				*Trans += Ins->Words[1]->UpWord + "\", ";
				*Trans += "true, true";
				// ,extends )
				if (Ins->Words.size() > 2) TransError("Extended classes not yet supported");
				*Trans += ")\n";
				(*Ret.Trans->GlobalVar)[Ins->Words[1]->UpWord] = "Scyndi.Classes." + Ins->Words[1]->UpWord;
				break;
			case InsKind::StartGroup:
				TransError("Groups not yet supported");
			}

		}
#pragma endregion

#pragma region "Declare non-locals"
		// Declaration management
		Verb("Managing", srcfile);
		for (auto Ins : Ret.Instructions) {
			auto LineNumber{ Ins->LineNumber };
			auto Dec{ Ins->DecData };
			if (Dec) {
				auto VarName{ Ins->Words[Ins->ForEachExpression]->UpWord };
				auto PluaName{ Ins->Words[Ins->ForEachExpression]->TheWord };			
				std::string Value{ "" };
				std::string DType{ "" };
				if (Dec->Type == VarType::CustomClass)
					DType = Dec->CustomClass;
				else {
					TransAssert(_Declaration::E2S(Dec->Type).size(), TrSPrintF("%03d:Internal error type unknown (Dec stage)", (int)Dec->Type));
					DType = _Declaration::E2S(Dec->Type);
				}
				if (VarName[0] == '$') VarName = VarName.substr(1);
				// Please note, everything not recongized as a variable declaration or function definition should be ignored.
				switch (Ins->Kind) {
				case InsKind::DefineFunction:
					// std::cout << "Defining function data: Gl:" << Dec->IsGlobal << "/Rt:" << Dec->IsRoot << std::endl;
					// Please note! NO CODE should be written yet, or translation later will mess up!
					if (Dec->BoundToClass.size()) {
						TransError("Methods and static functions not yet supported!");
					} else if (Dec->IsGlobal) {
						if (Dec->Type == VarType::pLua) {
							std::string Prefix{ "" }; if (TransConfig.count("PLUAPREFIX")) Prefix = TransConfig["PLUAPREFIX"]->TheWord;
							auto ref{ TrSPrintF("%s%s",Prefix.c_str(),PluaName.c_str()) };
							Ret.Trans->Data->Add("Globals", "-list-", VarName);
							Ret.Trans->Data->Value("Globals", VarName, ref);
							(*Ret.Trans->GlobalVar)[VarName] = ref;
						} else {
							auto ref{ TrSPrintF("Scyndi.Globals[\"%s\"]",VarName.c_str()) };
							Ret.Trans->Data->Add("Globals", "-list-", VarName);
							Ret.Trans->Data->Value("Globals", VarName, ref);
							(*Ret.Trans->GlobalVar)[VarName] = ref;
						}
					} else if (Dec->IsRoot) {
						//std::cout << "Root function: " << VarName << "\n"; // debug
						if (Dec->Type == VarType::pLua) {
							std::string Prefix{ "" }; if (TransConfig.count("PLUAPREFIX")) Prefix = TransConfig["PLUAPREFIX"]->TheWord;
							auto ref{ TrSPrintF("%s%s",Prefix.c_str(),PluaName.c_str()) };
							(*Ret.RootScope->LocalVars)[VarName] = ref;
							*Trans += "local " + ref+"\n"; // Will prevent trouble later! That's the only code that SHOULD be written.
							//std::cout << "Registered local plua " << VarName << " as " << ref << "\n"; // debug only
						} else {
							auto ref{ TrSPrintF("Scyndi.Class[\"%s\"][\"%s\"]",ScriptName.c_str(),VarName.c_str()) };
							(*Ret.RootScope->LocalVars)[VarName] = ref;
							//Scyndi.ADDMBER(ch,dtype,name,static,readonly,constant,value)
						}
					}
					break; 
					//TransError("Function definitions not yet supported");
				case InsKind::PropertySet:
				case InsKind::PropertyGet: {
					auto FClass{ Dec->BoundToClass };
					auto VarName{ Ins->Words[Ins->ForEachExpression]->UpWord };					
					if (Dec->IsGlobal) FClass = "..GLOBALS..";
					if (Dec->IsRoot) FClass = ScriptName;
					if (Ins->Kind == InsKind::PropertyGet) {
						TransAssert(!Ret.ClassProperty[FClass][VarName].hasget, "Dupe property-get ");
					} else {
						TransAssert(!Ret.ClassProperty[FClass][VarName].hasset, "Dupe property-set ");
					}
					Ret.ClassProperty[FClass][VarName].hasget = true;
					if (Dec->IsGlobal) {
						auto ref{ TrSPrintF("Scyndi.Globals[\"%s\"]",VarName.c_str()) };
						Ret.Trans->Data->Add("Globals", "-list-", VarName);
						Ret.Trans->Data->Value("Globals", VarName, ref);
						(*Ret.Trans->GlobalVar)[VarName] = ref;
					} else if (Dec->IsRoot) {
						auto ref{ TrSPrintF("Scyndi.Class[\"%s\"][\"%s\"]",ScriptName.c_str(),VarName.c_str()) };
						(*Ret.RootScope->LocalVars)[VarName] = ref;
					} else if (Dec->BoundToClass.size()) {
						auto ref{ TrSPrintF("Scyndi.Class[\"%s\"][\"%s\"]",Dec->BoundToClass.c_str(),VarName.c_str()) };
						if (Dec->IsStatic) {
							(*Ins->ScopeData->DecScope()->LocalVars)[VarName] = ref;
						} else {
							Ret.Fields[Upper(Dec->BoundToClass)].push_back(VarName);							
						}
					}
				} break;

				case InsKind::Declaration:
					if (Ins->Words.size() == Ins->ForEachExpression + 1) {
						switch (Dec->Type) {
						case VarType::Number:
						case VarType::Integer:
						case VarType::Byte:
							Value = "0";
							break;
						case VarType::Boolean:
							Value = "false";
							break;
						case VarType::String:
							Value = "\"\"";
							break;
						case VarType::Table:
							Value = "{}";
							break;
						default:
							Value = "nil";
							break;
						}
					} else {
						TransAssert(Ins->Words[Ins->ForEachExpression + 1]->TheWord == "=", "Variable declaration syntax error");
						TransAssert(Ins->Words.size() > Ins->ForEachExpression + 1, "Default value expected");
						auto EX{ Expression(Ret.Trans,Ins,Ins->ForEachExpression + 2) };
						if (!EX) return nullptr;
						Value = *EX;
					}
					// std::cout << "Dec In Class '" << Dec->BoundToClass << "';\n"; //debug
					if (Dec->BoundToClass.size()) {
						auto ref{ TrSPrintF("Scyndi.Class[\"%s\"][\"%s\"]",Dec->BoundToClass.c_str(),VarName.c_str()) };
						*Trans += TrSPrintF("Scyndi.ADDMBER(\"%s\",\"%s\",\"%s\",%s,%s,%s,%s)\n", Dec->BoundToClass.c_str(), DType.c_str(), VarName.c_str(), lboolstring(Dec->IsStatic), lboolstring(Dec->IsReadOnly).c_str(), Lower(boolstring(Dec->IsConstant)).c_str(), Value.c_str());
						if (Dec->IsStatic) {
							(*Ins->ScopeData->DecScope()->LocalVars)[VarName] = ref;
						} else {
							Ret.Fields[Upper(Dec->BoundToClass)].push_back(VarName);
						}
						if (Dec->IsGlobal) TransError("GLOBAL not allowed for class members");
					} else if (Dec->IsGlobal) {
						if (Dec->Type == VarType::pLua) {
							std::string Prefix{ "" }; if (TransConfig.count("PLUAPREFIX")) Prefix = TransConfig["PLUAPREFIX"]->TheWord;
							auto ref{ TrSPrintF("%s%s",Prefix.c_str(),PluaName.c_str()) };
							Ret.Trans->Data->Add("Globals", "-list-", VarName);
							Ret.Trans->Data->Value("Globals", VarName, ref);
							(*Ret.Trans->GlobalVar)[VarName] = ref;
							*Trans += TrSPrintF("%s = %s\n", ref.c_str(), Value.c_str());
						} else {
							auto ref{ TrSPrintF("Scyndi.Globals[\"%s\"]",VarName.c_str()) };
							Ret.Trans->Data->Add("Globals", "-list-", VarName);
							Ret.Trans->Data->Value("Globals", VarName, ref);
							(*Ret.Trans->GlobalVar)[VarName] = ref;
							//Scyndi.ADDMBER(ch,dtype,name,static,readonly,constant,value)
							*Trans += TrSPrintF("Scyndi.ADDMBER(\"..GLOBALS..\",\"%s\",\"%s\",true,%s,%s,%s)\n", DType.c_str(), VarName.c_str(), Lower(boolstring(Dec->IsReadOnly)).c_str(), Lower(boolstring(Dec->IsConstant)).c_str(), Value.c_str());
						}
					} else if (Dec->IsRoot) {
						if (Dec->Type == VarType::pLua) {
							std::string Prefix{ "" }; if (TransConfig.count("PLUAPREFIX")) Prefix = TransConfig["PLUAPREFIX"]->TheWord;
							auto ref{ TrSPrintF("%s%s",Prefix.c_str(),PluaName.c_str()) };
							*Trans += TrSPrintF("local %s = %s\n", ref.c_str(), Value.c_str());
							(*Ret.RootScope->LocalVars)[VarName] = ref;
						} else {
							auto ref{ TrSPrintF("Scyndi.Class[\"%s\"][\"%s\"]",ScriptName.c_str(),VarName.c_str()) };
							(*Ret.RootScope->LocalVars)[VarName] = ref;
							//Scyndi.ADDMBER(ch,dtype,name,static,readonly,constant,value)
							*Trans += TrSPrintF("Scyndi.ADDMBER(\"%s\",\"%s\",\"%s\",true,%s,%s,%s)\n", ScriptName.c_str(), DType.c_str(), VarName.c_str(), Lower(boolstring(Dec->IsReadOnly)).c_str(), Lower(boolstring(Dec->IsConstant)).c_str(), Value.c_str());
						}
					}
				}
			}
		}
#pragma endregion

#pragma region "Actual Translation"
		// Translate
		Verb("Translating", srcfile);
		auto InitTag{ TrSPrintF("__Scyndi__Init__%s",md5(srcfile + CurrentDate() + CurrentTime()).c_str()) };
		if (HasInit) *Trans += "\nlocal " + InitTag + " = {}\n";	
		for (auto& Ins : Ret.Instructions) {
			auto LineNumber{ Ins->LineNumber };
			if (Ins->Kind == InsKind::EndScope || Ins->Kind==InsKind::ElseIfStatement || Ins->Kind==InsKind::ElseStatement)
				for (size_t tab = 1; tab < Ins->ScopeLevel; tab++) *Trans += '\t';
			else
				for (size_t tab = 0; tab < Ins->ScopeLevel; tab++) *Trans += '\t';
			switch (Ins->Kind) {
				// Alright! Move along! There's nothing to see here.
				// These kind of instructions were valid to preprocessing and stuff, but have no value any more during translating itself.
				// It has ben taken care of by now!
			case InsKind::WhiteLine:
			case InsKind::CompilerDirective: // Not needed anymore! This has already been taken care of, remember?
			case InsKind::HeaderDefintion:
			case InsKind::StartDeclarationScope: // The declaration scoping is already taken care of while pre-processing.
			case InsKind::StartClass: // Classes have already been taken care of
			case InsKind::StartGroup: // Groups too
				break;
			case InsKind::StartInit:
				*Trans += InitTag + "[#" + InitTag + "+1]=function()\n";
				if (debug) *Trans += ("Scyndi.Debug.Push(\"Init Scope\") ");
				break;
			case InsKind::StartDo:
				*Trans += "do\n";
				if (debug) *Trans += ("Scyndi.Debug.Push(\"Do Scope\") ");
				break;
			case InsKind::StartFor:
			case InsKind::StartForEach: {
				DbgLineCheck;
				*Trans += "for ";
				for (size_t i = 0; i < Ins->ForVars.size(); i++) {
					if (i) *Trans += ", ";
					//auto tvar{ Ins->ScopeData->Identifier(Ret.Trans, Ins->LineNumber, Ins->ForVars[i], true) };
					auto getsm{ Ins->ForTrans };
					auto tvar{ (*getsm)[Ins->ForVars[i]] };
					TransAssert(tvar.size(),TrSPrintF("FOR variable '%s' didn't seem to have a translation. This is an internal error! Please report! ", Ins->ForVars[i].c_str()))
					*Trans += tvar;
				}
				switch (Ins->Kind) {
				case InsKind::StartFor: *Trans += " = "; break;
				case InsKind::StartForEach: *Trans += " in "; break;
				default: TransError("Internal error in FOR translation! Please report!"); break; // This should be impossible!
				}
				auto Ex{ Expression(Ret.Trans,Ins,Ins->ForEachExpression) };
				if (!Ex) return nullptr;
				*Trans += *Ex;
				*Trans += " do\n";
			} break;
			case InsKind::WhileStatement: {
				DbgLineCheck;
				*Trans += "while ";
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				*Trans += *Ex;
				*Trans += " do\n";
			} break;
			case InsKind::IfStatement: {
				DbgLineCheck;
				*Trans += "if ";
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				*Trans += *Ex;
				*Trans += " then\n";
			} break;
			case InsKind::ElseIfStatement: {
				DbgLineCheck;
				*Trans += "elseif ";
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				*Trans += *Ex;
				*Trans += " then\n";
			} break;
			case InsKind::ElseStatement:
				DbgLineCheck;
				*Trans += "else\n";
				break;
			case InsKind::EndScope:
				DbgLineCheck;
				switch (Ins->Scope) {
				case ScopeKind::Init:
				case ScopeKind::Defer:
					if (!Ins->ScopeData->DidReturn) {
						if (Ins->Scope != ScopeKind::Defer) *Trans += Ins->ScopeData->DeferLine();
						if (debug) *Trans += " Scyndi.Debug.Pop(); ";
						*Trans += "end\n";
					}
					break;
				case ScopeKind::Class:
				case ScopeKind::Group:
					break; // Will be taken care of at the close of the entire script
				case ScopeKind::ForLoop:
				case ScopeKind::IfScope:
				case ScopeKind::ElIf:
				case ScopeKind::ElseScope:
				case ScopeKind::WhileScope:
				case ScopeKind::Do:
					*Trans += "end\n";
					break;					
				case ScopeKind::Case:
				case ScopeKind::Default:
					*Trans += "end\t";
					*Trans += "::" + *Ins->SwitchName + "_End::\n";
					break;
				case ScopeKind::FunctionBody:
					if (!Ins->ScopeData->DidReturn) {
						if (debug) *Trans += " Scyndi.Debug.Pop(); ";
						if (Ins->Scope != ScopeKind::Defer) *Trans += Ins->ScopeData->DeferLine();
						TransAssert(Ins->ScopeData->DecData, "Function body check. Dec data is null (internal error. Please report)")
							switch (Ins->ScopeData->DecData->Type) {
							case VarType::Void:
							case VarType::Var:
							case VarType::Delegate:
							case VarType::CustomClass:
							case VarType::pLua:
							case VarType::UserData:
								break; // Base value would be 'nil' anyway!
							case VarType::String:
								*Trans += "return \"\"; ";
								break;
							case VarType::Number:
							case VarType::Byte:
							case VarType::Integer:
								*Trans += "return 0; ";
								break;
							case VarType::Table:
								*Trans += "return {}; ";
								break;
							case VarType::Boolean:
								*Trans += "return false; ";
								break;
							default:
								TransError("Function with unknown return type ended");

							}
					}
					*Trans += "end";
					if (Ins->ScopeData->DecData->IsGlobal || Ins->ScopeData->DecData->IsRoot || Ins->ScopeData->DecData->BoundToClass.size()) {
						if (Ins->ScopeData->DecData->Type != VarType::pLua) *Trans += ")";
					} else if (Ins->ScopeData->Parent && Ins->ScopeData->Parent->Kind == ScopeKind::QuickMeta) {
						*Trans += ",";
					}
					*Trans += "\n";
					break;
				case ScopeKind::QuickMeta:
					*Trans += "})) -- End QuickMeta!\n\n";
					break;
				default:
					TransError(TrSPrintF("Unknown scope kind (%03d)! Cannot end it (Internal error! Please report!)",(int)Ins->Scope));
					//break;
				}
				break;
			case InsKind::Declaration:
				TransAssert(Ins->DecData, "No DecData in translation (transphase/variable) - This is an internal error! Please report!");
				if (Ins->DecData->IsGlobal || Ins->DecData->IsRoot || Ins->DecData->BoundToClass.size()) break;
				{
					DbgLineCheck;
					auto VarName{ Ins->Words[Ins->ForEachExpression]->UpWord };
					auto PluaName{ Ins->Words[Ins->ForEachExpression]->TheWord };

					auto p{ Ins->ForEachExpression + 1 };
					std::string BaseValue;
					if (p >= Ins->Words.size()) {
						switch (Ins->DecData->Type) {
						case VarType::Boolean:
							BaseValue = "false"; break;
						case VarType::Byte:
						case VarType::Integer:
						case VarType::Number:
							BaseValue = "0"; break;
						case VarType::String:
							BaseValue = "\"\""; break;
						case VarType::Table:
							BaseValue = "{}"; break;
						default:
							BaseValue = "nil"; break;
						}
					} else {
						TransAssert(Ins->Words[p]->TheWord == "=", "Local declaration syntax error");
						p++;
						TransAssert(p < Ins->Words.size(), "Incomplete local declaration");
						auto Ex{ Expression(Ret.Trans,Ins,p) };
						if (!Ex) return nullptr;
						BaseValue = *Ex;
					}
					// Static Local
					if (Ins->DecData->IsStatic) {
						//TransError("Static locals not yet supported");
						static size_t count{ 0 };
						auto
							stname{ TrSPrintF("Static_%08x",count++) },
							fullstname{ StaticRegister + "_" + stname };
						*Trans += TrSPrintF("if not %s[\"%s\"] then ", StaticRegister.c_str(), stname.c_str());
						if (Ins->DecData->Type == VarType::pLua) {
							*Trans += TrSPrintF("%s = %s; ", fullstname.c_str(), BaseValue.c_str());
							(*Ins->ScopeData->LocalVars)[VarName] = fullstname;
						} else {
							auto ReadOnly{ Lower(boolstring(Ins->DecData->IsReadOnly || Ins->DecData->IsConstant)) };
							*Trans += TrSPrintF("Scyndi.ADDMBER(\"..GLOBALS..\", \"%s\", \"%s\", false, %s, %s, value);", _Declaration::E2S(Ins->DecData->Type).c_str(), fullstname.c_str(), ReadOnly.c_str(), ReadOnly.c_str(), BaseValue.c_str());
							(*Ins->ScopeData->LocalVars)[VarName] = TrSPrintF("Scyndi.Globals.%s", fullstname.c_str());
						}
						*Trans += " end\n";
						Ins->ScopeData->LocalDeclaLine[VarName] = Ins->LineNumber;
						break;
					}
					// Local
					if (Ins->DecData->Type == VarType::pLua) {
						*Trans += "local " + PluaName;
						if (BaseValue != "nil") *Trans += " = " + BaseValue;
						(*Ins->ScopeData->LocalVars)[VarName] = PluaName;
						Ins->ScopeData->LocalDeclaLine[VarName] = Ins->LineNumber;
					} else {
						if (!Ins->ScopeData->ScopeLoc.size()) {
							static size_t count{ 0 };
							Ins->ScopeData->ScopeLoc = TrSPrintF("__ScyndiLocals_%08x_%02d_", count++, (int)Ins->ScopeData->Kind);
							Ins->ScopeData->ScopeLoc += md5(Ins->ScopeData->ScopeLoc + srcfile);
							*Trans += "local " + Ins->ScopeData->ScopeLoc + " = Scyndi.CreateLocals(); ";
						}
						*Trans += TrSPrintF("Scyndi.DECLARELOCAL(%s, \"%s\", %s, \"%s\", %s);", Ins->ScopeData->ScopeLoc.c_str(), _Declaration::E2S(Ins->DecData->Type).c_str(), Lower(boolstring(Ins->DecData->IsReadOnly || Ins->DecData->IsConstant)).c_str(), VarName.c_str(), BaseValue.c_str());
						(*Ins->ScopeData->LocalVars)[VarName] = TrSPrintF("%s[\"%s\"]", Ins->ScopeData->ScopeLoc.c_str(), VarName.c_str());
						Ins->ScopeData->LocalDeclaLine[VarName] = Ins->LineNumber;
					}
					*Trans += "\n";
				}
				break;
			case InsKind::QuickMeta:
			case InsKind::StartQuickMeta:
				*Trans += TrSPrintF("Scyndi.ADDMBER(\"..GLOBALS..\", \"TABLE\", \"%s\", true, true, true, setmetatable({},{\n", Ins->Words[1]->UpWord.c_str());
				break;
			case InsKind::StartMetaMethod:
				// Start Meta Method
				*Trans += TrSPrintF("__%s = function(self", Lower(Ins->Words[0]->UpWord).c_str());
				for (auto& MMName : MetaMethods[Ins->Words[0]->UpWord]) *Trans += ", " + MMName;
				Ins->NextScope->DecData = std::make_shared<_Declaration>();
				Ins->DecData = Ins->NextScope->DecData;
				if (Ins->Words[0]->UpWord == "NEWINDEX")
					Ins->NextScope->DecData->Type = VarType::Void;
				else
					Ins->NextScope->DecData->Type = VarType::Var;
				*Trans += ")\n";
				break;
			case InsKind::PropertyGet: {
				auto dec{ Ins->DecData }; 
				auto fclass{ dec->BoundToClass };
				auto VarName{ Ins->Words[Ins->ForEachExpression]->UpWord };
				TransAssert(dec->Type != VarType::pLua, "PLUA cannot be used for properties!");
				if (dec->IsGlobal) fclass = "..GLOBALS..";
				if (dec->IsRoot) fclass = ScriptName;
				TransAssert(fclass.size(), "GET property not possible as a local");
				*Trans += TrSPrintF("Scyndi.ADDPROPERTY(\"%s\", \"%s\", %s, \"get\", function(self) \n", fclass.c_str(),VarName.c_str(),lboolstring(dec->IsStatic || dec->IsRoot || dec->IsGlobal));
				if (debug) *Trans += TrSPrintF("Scyndi.Debug.Push(\"Property(GET) %s.%s\") ",fclass.c_str(),VarName.c_str());
				if (Ins->DecData->BoundToClass.size() && (!Ins->DecData->IsStatic)) {
					for (auto& FLD : Ret.Fields[Upper(Ins->DecData->BoundToClass)]) {
						(*Ins->NextScope->LocalVars)[FLD] = TrSPrintF("self.%s", FLD.c_str());
					}
				}
			} break;
			case InsKind::PropertySet: {
				static size_t count{ 0 };
				auto dec{ Ins->DecData };
				auto fclass{ dec->BoundToClass };
				auto VarName{ Ins->Words[Ins->ForEachExpression]->UpWord };
				TransAssert(dec->Type != VarType::pLua, "PLUA cannot be used for properties!");
				if (dec->IsGlobal) fclass = "..GLOBALS..";
				if (dec->IsRoot) fclass = ScriptName;
				TransAssert(fclass.size(), "GET property not possible as a local");
				Ins->NextScope->ScopeLoc = TrSPrintF("Scyndi_Set_Property_%08x_%s", count++, md5(VarName).c_str());
				*Trans += TrSPrintF("Scyndi.ADDPROPERTY(\"%s\", \"%s\", %s, \"set\", function(self,_value) \n", fclass.c_str(), VarName.c_str(), lboolstring(dec->IsStatic || dec->IsRoot || dec->IsGlobal));
				if (debug) *Trans += TrSPrintF("Scyndi.Debug.Push(\"Property(SET) %s.%s\") ", fclass.c_str(), VarName.c_str());
				*Trans += Ins->NextScope->ScopeLoc; *Trans += " = Scyndi.CreateLocals()\n";
				*Trans += TrSPrintF("Scyndi.DECLARELOCAL(%s,\"%s\", false,\"Value\",_value); ", Ins->NextScope->ScopeLoc.c_str(), _Declaration::E2S(Ins->DecData->Type).c_str());
				(*Ins->NextScope->LocalVars)["VALUE"] = TrSPrintF("%s[\"VALUE\"]", Ins->NextScope->ScopeLoc.c_str());
				if (Ins->DecData->BoundToClass.size() && (!Ins->DecData->IsStatic)) {
					for (auto& FLD : Ret.Fields[Upper(Ins->DecData->BoundToClass)]) {
						(*Ins->NextScope->LocalVars)[FLD] = TrSPrintF("self.%s", FLD.c_str());
					}
				}
			} break;
			case InsKind::StartMethod:
			case InsKind::DefineFunction: {
				TransAssert(Ins->DecData, "No DecData in translation (transphase/function) - This is an internal error! Please report!");
				static size_t count = 0;
				auto FNamePos{ Ins->ForEachExpression };
				auto FName{ Ins->Words[FNamePos] };
				auto Arg1Pos{ FNamePos + 2 };
				auto Ending{ Ins->Words.size() };
				auto ScN{ TrSPrintF("ScyndiFuncScope_%08X_",count++) + FName->TheWord + md5(FName->TheWord + std::to_string(count)).c_str() };
				auto VarName{ FName->UpWord };
				auto PluaName{ FName->TheWord };
				std::string Prefix{ "" }; if (TransConfig.count("PLUAPREFIX")) Prefix = TransConfig["PLUAPREFIX"]->TheWord;

				std::string ArgLine{ "" };
				TransAssert(Ins->Words.size() >= Arg1Pos, "? Incomplete function definition ?");
				std::vector<Arg> Args{};
				auto Pos{ Arg1Pos };
				auto oscope{ Ins->ScopeData }, nscope{ Ins->NextScope };
				std::string pLuaLine{ "" };
				while (Pos < Ending && Ins->Words[Pos]->UpWord != ")") {
					//std::cout << VarName << ":\t" << Pos << "\t" << Ins->Words[Pos]->UpWord << "\t" << (Ins->Words[Pos]->UpWord == "PLUA") << "\n"; // debug only!
					if (Ins->Words[Pos]->Kind == WordKind::Identifier) {
						
						if (ArgLine.size()) ArgLine += ", "; ArgLine += TrSPrintF("Arg%d", Args.size());
						Args.push_back(Arg{ Ins->Words[Pos]->UpWord,TrSPrintF("%s[\"%s\"]",ScN,Ins->Words[Pos]->UpWord),"",VarType::Var,false });
						Pos++;
						TransAssert(Pos < Ending && (Ins->Words[Pos]->Kind == WordKind::Comma || Ins->Words[Pos]->TheWord == ")"), TrSPrintF("Syntax error in function defintion after (variant) argument #%d", Args.size()));
						Pos++;
					} else if (Ins->Words[Pos]->UpWord == "PLUA") {
						Pos++;
						TransAssert(Ins->DecData->BoundToClass == "", "Plua cannot be bound to classes");
						TransAssert(Ins->Words[Pos]->Kind == WordKind::Identifier, "Identifier for pLua expected");
						auto ArgName{ Ins->Words[Pos]->UpWord };
						auto PluaName{ Ins->Words[Pos]->TheWord }; PluaName = Prefix + PluaName;
						if (ArgLine.size()) ArgLine += ", "; ArgLine += PluaName;
						Pos++;
						if (Ins->Words[Pos]->UpWord == "=") {
							Pos++;
							switch (Ins->Words[Pos]->Kind) {
							case WordKind::Number:
								pLuaLine += TrSPrintF("%s = %s or %s; ", PluaName.c_str(), PluaName.c_str(), Ins->Words[Pos]->UpWord.c_str());
								break;
							case WordKind::String:
								pLuaLine += TrSPrintF("%s = %s or \"%s\"; ", PluaName.c_str(), PluaName.c_str(), Ins->Words[Pos]->UpWord.c_str());
								break;
							default:
								TransError("Only constant lines or strings can be used as base values for pPlua");
							}
							Pos++;
						}
						TransAssert(Pos < Ending && (Ins->Words[Pos]->Kind == WordKind::Comma || Ins->Words[Pos]->TheWord == ")"), "Syntax error in function definition after (plua) argument");
						(*Ins->NextScope->LocalVars)[ArgName] = PluaName;
						// std::cout << ArgName << " local -> " << PluaName << "\n"; // debug only
						Pos++;
					} else if (Ins->Words[Pos]->UpWord == "INT" || Ins->Words[Pos]->UpWord == "NUMBER" || Ins->Words[Pos]->UpWord == "BYTE") {
						auto DT{ Ins->Words[Pos]->UpWord };
						Pos++;
						auto A{ Arg{ Ins->Words[Pos]->UpWord,TrSPrintF("%s[\"%s\"]",ScN,Ins->Words[Pos]->UpWord),"",_Declaration::S2E[DT],false} };
						// std::cout << "Arg type " << (int)A.dType << "(" << DT << ")\n"; // debug
						if (ArgLine.size()) ArgLine += ", "; ArgLine += TrSPrintF("Arg%d", Args.size());
						Pos++;
						if (Ins->Words[Pos]->UpWord == "=") {
							Pos++;
							TransAssert(Ins->Words[Pos]->Kind == WordKind::Number, "Constant number expected");
							A.HasBaseValue = true;
							A.BaseValue = Ins->Words[Pos]->UpWord;
							Pos++;
						} else { A.BaseValue = "error('Numberic value expected for argument " + A.Name + "')"; }
						Args.push_back(A);
						TransAssert(Pos < Ending && (Ins->Words[Pos]->Kind == WordKind::Comma || Ins->Words[Pos]->TheWord == ")"), TrSPrintF("Syntax error in function definition after (numberic) argument #%d", Args.size()));
						Pos++;
					} else if (Ins->Words[Pos]->UpWord == "STRING") {
						auto DT{ Ins->Words[Pos]->UpWord };
						Pos++;
						auto A{ Arg{ Ins->Words[Pos]->UpWord,TrSPrintF("%s[\"%s\"]",ScN,Ins->Words[Pos]->UpWord),"",_Declaration::S2E[DT],false} };
						// std::cout << "Arg type " << (int)A.dType << "(" << DT << ")\n"; // debug
						if (ArgLine.size()) ArgLine += ", "; ArgLine += TrSPrintF("Arg%d", Args.size());
						Pos++;
						if (Ins->Words[Pos]->UpWord == "=") {
							Pos++;
							TransAssert(Ins->Words[Pos]->Kind == WordKind::String, "Constant string expected");
							A.HasBaseValue = true;
							A.BaseValue = Ins->Words[Pos]->TheWord; //TrSPrintF("\"%s\"", Ins->Words[Pos]->TheWord.c_str());
							Pos++;
						} else { A.BaseValue = "error('String value expected for argument " + A.Name + "')"; }
						Args.push_back(A);
						TransAssert(Pos < Ending && (Ins->Words[Pos]->Kind == WordKind::Comma || Ins->Words[Pos]->TheWord == ")"), TrSPrintF("Syntax error in function defintion after (string) argument #%d", Args.size()));
						Pos++;
					} else if (Ins->Words[Pos]->UpWord=="BOOL"){
						auto DT{ Ins->Words[Pos]->UpWord };
						Pos++;
						auto A{ Arg{ Ins->Words[Pos]->UpWord,TrSPrintF("%s[\"%s\"]",ScN,Ins->Words[Pos]->UpWord),"",_Declaration::S2E[DT],false} };
						// std::cout << "Arg type " << (int)A.dType << "(" << DT << ")\n"; // debug
						if (ArgLine.size()) ArgLine += ", "; ArgLine += TrSPrintF("Arg%d", Args.size());
						Pos++;
						if (Ins->Words[Pos]->UpWord == "=") {
							/*
							Pos++;
							TransAssert(Ins->Words[Pos]->Kind == WordKind::String, "Constant string expected");
							A.HasBaseValue = true;
							A.BaseValue = Ins->Words[Pos]->TheWord; //TrSPrintF("\"%s\"", Ins->Words[Pos]->TheWord.c_str());
							Pos++;
							*/
							TransError("Default values not possible for boolean arguments");
						} else { A.BaseValue = "false"; }
						Args.push_back(A);
						Pos++;
						TransAssert(Pos < Ending && (Ins->Words[Pos]->Kind == WordKind::Comma || Ins->Words[Pos]->TheWord == ")"), TrSPrintF("Syntax error in function defintion after (boolean) argument #%d", Args.size()));
					} else if (Ins->Words[Pos]->TheWord[0] == '@') {
						TransError("Custom class type as function argument not (yet) supported. Just use an untyped argument or a pLua in stead");
					} else if (Ins->Words[Pos]->UpWord == "DELEGATE" || Ins->Words[Pos]->UpWord == "TABLE" || Ins->Words[Pos]->UpWord == "BOOL") {
						auto DT{ Ins->Words[Pos]->UpWord };
						Pos++;
						auto A{ Arg{ Ins->Words[Pos]->UpWord,TrSPrintF("%s[\"%s\"]",ScN,Ins->Words[Pos]->UpWord),"",_Declaration::S2E[DT],false} };
						Args.push_back(A);
						Pos++;
						TransAssert(Pos < Ending && (Ins->Words[Pos]->Kind == WordKind::Comma || Ins->Words[Pos]->TheWord == ")"), TrSPrintF("Syntax error in function defintion after (%s) argument #%d", DT.c_str(), Args.size()));
						Pos++;
					} else if (Ins->Words[Pos]->UpWord=="INFINITY") {
						if (ArgLine.size()) ArgLine += ", "; ArgLine += "...";
						Pos++;
						TransAssert(Ins->Words.size() > Pos && Ins->Words[Pos]->UpWord == ")", "Syntax error after infinite parameters");
						Pos++;
					} else {
						std::cout << Pos << std::endl; // debug
						TransError("Syntax error");
					}
				}
				switch (oscope->Kind) {
				case ScopeKind::Class:
					if (Ins->DecData->IsStatic)
						*Trans += TrSPrintF("Scyndi.ADDMBER(\"%s\",\"DELEGATE\",\"%s\",true,true,true,function (%s) ", Ins->DecData->BoundToClass.c_str(), VarName.c_str(), ArgLine.c_str());
					else if (ArgLine.size())
						*Trans += TrSPrintF("Scyndi.ADDMETHOD(\"%s\", \"%s\", %s, function(self,%s)", Ins->DecData->BoundToClass.c_str(), VarName.c_str(), lboolstring(Ins->DecData->IsFinal).c_str(), ArgLine.c_str());
					else {
						*Trans += TrSPrintF("Scyndi.ADDMETHOD(\"%s\", \"%s\", %s, function(self)", Ins->DecData->BoundToClass.c_str(), VarName.c_str(), lboolstring(Ins->DecData->IsFinal).c_str());
					}
					break;
				case ScopeKind::Root:
					if (Ins->DecData->IsGlobal) {
						if (Ins->DecData->Type == VarType::pLua) {
							auto ref{ TrSPrintF("%s%s",Prefix.c_str(),PluaName.c_str()) };
							// I need to improve this
							//Ret.Trans->Data->Add("Globals", "-list-", VarName);
							//Ret.Trans->Data->Value("Globals", VarName, ref);
							//(*Ret.Trans->GlobalVar)[VarName] = ref;
							*Trans += "function " + ref + "(" + ArgLine + ") ";

						} else {
							*Trans += TrSPrintF("Scyndi.ADDMBER(\"..GLOBALS..\",\"DELEGATE\",\"%s\",true,true,true,function (%s) ", VarName.c_str(), ArgLine.c_str());
							//auto ref{ TrSPrintF("Scyndi.Globals[\"%s\"]",VarName.c_str()) };
							//Ret.Trans->Data->Add("Globals", "-list-", VarName);
							//Ret.Trans->Data->Value("Globals", VarName, ref);
							//(*Ret.Trans->GlobalVar)[VarName] = ref;						
						}
					} else if (Ins->DecData->IsRoot) {
						if (Ins->DecData->Type == VarType::pLua) {
							auto ref{ TrSPrintF("%s%s",Prefix.c_str(),PluaName.c_str()) };
							//rootfunc
							//(*Ret.RootScope->LocalVars)[VarName] = ref;
							*Trans += ref + "= function(" + ArgLine + ") ";
						} else {
							*Trans += TrSPrintF("Scyndi.ADDMBER(\"%s\",\"DELEGATE\",\"%s\",true,true,true,function (%s) ", ScriptName.c_str(), VarName.c_str(), ArgLine.c_str());
							//auto ref{ TrSPrintF("Scyndi.Class[\"%s\"][\"%s\"]",ScriptName.c_str(),VarName.c_str()) };
							//(*Ret.RootScope->LocalVars)[VarName] = ref;
						}
					} else {
						TransError("This kind of function, is not yet supported")
					}
					break;
				default:
					std::cout << (int)Ret.RootScope->Kind << "\n"; // debug only
					TransError(TrSPrintF("(SC%d) Local functions not yet implemented", (int)oscope->Kind));
				}
				if (debug) *Trans += TrSPrintF("Scyndi.Debug.Push(\"%s\")",VarName.c_str());
				DbgLineCheck;
				if (Args.size()) {
					//std::cout << "Function " << VarName << " has " << Args.size() << " argument(s)\n"; // debug only
					Ins->NextScope->ScopeLoc = ScN + "_Locals";
					*Trans += "local " + Ins->NextScope->ScopeLoc + " = Scyndi.CreateLocals(); ";
					for (size_t ap = 0; ap < Args.size(); ap++) {
						std::string IValue{ "nil" };
						auto Ag{ &Args[ap] };
						switch (Ag->dType) {
						case VarType::Boolean:
							if (Ag->HasBaseValue) {
								if (Upper(Ag->BaseValue) == "TRUE") IValue = "true";
								else if (Upper(Ag->BaseValue) == "FALSE") IValue = "false";
								else TransError("Invalid base value for boolean");
							} else IValue = "false";
							break;
						case VarType::Byte:
						case VarType::Integer:
						case VarType::Number:
							if (Ag->HasBaseValue) IValue = Ag->BaseValue; else IValue = "error('Numberic value expected for argument \""+Ag->Name+"\"')";
							//std::cout << VarName << ": num " << Ag->Name << " = " << IValue<<"\n";
							break;
						case VarType::pLua: // this should not be possible as pLua vars are handled differently
							if (Ag->HasBaseValue) TransError("Plua cannot hold a base value");
							break;
						case VarType::String:
							if (Ag->HasBaseValue) IValue = TrSPrintF("\"%s\"", Ag->BaseValue); else IValue="error('String expected for argument \""+Ag->Name+"\"')"; //IValue = "\"\"";
							break;
						default:
							if (Ag->HasBaseValue) TransError("Base value not permitted for that type");
							IValue = "nil";
							break;
						}
						*Trans += TrSPrintF("Scyndi.DECLARELOCAL(%s,\"%s\", false,\"%s\",Arg%d or %s); ", Ins->NextScope->ScopeLoc.c_str(), _Declaration::E2S(Args[ap].dType).c_str(), Args[ap].Name.c_str(), ap, IValue.c_str());
						(*Ins->NextScope->LocalVars)[Upper(Ag->Name)] = TrSPrintF("%s[\"%s\"]", Ins->NextScope->ScopeLoc.c_str(), Ag->Name.c_str());
					}
				}
				*Trans += pLuaLine;
				*Trans += "\n";
				if (Ins->Kind == InsKind::StartMethod) {
					(*Ins->NextScope->LocalVars)["SELF"] = "self";
					//std::cout << "Fields for "<< Upper(Ins->DecData->BoundToClass)<<"\n";
					for (auto& FLD : Ret.Fields[Upper(Ins->DecData->BoundToClass)]) {						
						(*Ins->NextScope->LocalVars)[FLD] = TrSPrintF("self.%s", FLD.c_str());
					}
				}
				//TransError("Function defs not yet complete"); // security
			} break;
			case InsKind::General: {
				DbgLineCheck;
				auto Ex{ Expression(Ret.Trans,Ins,0) };
				if (!Ex) return nullptr;
				*Trans += *Ex;
				*Trans += '\n';
			} break;
			case InsKind::Increment: {			
				DbgLineCheck;
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				*Trans += *Ex;
				*Trans += " = Scyndi.Inc(";
				*Trans += *Ex;
				*Trans += ")\n";
			} break;
			case InsKind::Decrement: {
				DbgLineCheck;
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				*Trans += *Ex;
				*Trans += " = Scyndi.Dec(";
				*Trans += *Ex;
				*Trans += ")\n";
			} break;
			case InsKind::Switch: {
				DbgLineCheck;
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				auto SwName{ *Ins->SwitchName };
				auto SwVar{ *Ins->SwitchName }; SwVar += "_CheckVar"; 
				*Trans += "local " + SwVar + "= "; *Trans += *Ex; *Trans += ";\t";
				for (size_t i = 0; i < Ins->switchcase->size(); i++) {
					auto CaseChain{ (*Ins->switchcase)[i] };
					for (auto MyCase : *CaseChain) {
						*Trans += "if " + SwVar + " == " + MyCase + " then goto " + SwName + "_Case_" + std::to_string(i) + " end; ";
					}
				}
				if (*Ins->switchHasDefault) *Trans += "goto " + SwName + "_Default"; else *Trans += "goto " + SwName + "_End";
				*Trans += "\n";
			} break;
			case InsKind::FallThrough:
				break; // No more purpose at this point!
			case InsKind::Case: {
				static std::map<std::string, size_t> CaseCount{};
				if (!CaseCount.count(*Ins->SwitchName)) CaseCount[*Ins->SwitchName] = 0;
				if (CaseCount[*Ins->SwitchName]) {
					if (!(Ins->ScopeData->caseFallThrough || Ins->ScopeData->DidReturn)) *Trans += "goto " + *Ins->SwitchName + "_End;\t";
					*Trans += "end;\t";
				}
				*Trans += TrSPrintF("::%s_Case_%d:: do ", Ins->SwitchName->c_str(), CaseCount[*Ins->SwitchName]++);
				// std::cout << *Ins->SwitchName << ":\tCase count: " << CaseCount[*Ins->SwitchName] << std::endl; // debug only
			} break;
			case InsKind::Default: {
				if (!(Ins->ScopeData->caseFallThrough || Ins->ScopeData->DidReturn)) *Trans += "goto " + (*Ins->SwitchName) + "_End;\t";
				*Trans += "end;\t";
				*Trans += TrSPrintF("::%s_Default:: do ", Ins->SwitchName->c_str());
			} break;
			case InsKind::Forever: 
				DbgLineCheck;
				*Trans += "until false\n";
				break;
			case InsKind::Until:
			case InsKind::LoopWhile: {
				DbgLineCheck;
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				*Trans += "until ";
				if (Ins->Kind == InsKind::LoopWhile)
					*Trans += TrSPrintF("not(%s)", Ex->c_str());
				else
					*Trans += Ex->c_str();
				*Trans += "\n";
			} break;
			case InsKind::Repeat:
				*Trans += "repeat\n";
				break;
			case InsKind::Return: {
				DbgLineCheck;
				if (Ins->Scope != ScopeKind::Defer) *Trans += Ins->ScopeData->DeferLine();
				if (debug) *Trans += " Scyndi.Debug.Pop(); ";
				// TODO: If there are any defers, take care of them first!
				auto Sc{ Ins->ScopeData };
				auto fKind{ Sc->FunctionScopeType() };
				if (fKind == VarType::Void) {
					TransAssert(Ins->Words.size() == 1, "Void functions (which includes, Init, Defers, Constructors and Destructors) cannot return any values");
					*Trans += "return;\n";
					Sc->DidReturn = true;
					break;
				}
				TransAssert(Ins->Words.size() > 1, "Return without data");
				auto Ex{ Expression(Ret.Trans,Ins,1) };
				if (!Ex) return nullptr;
				*Trans += "return ";
				switch (fKind) {
				case VarType::CustomClass:
				case VarType::pLua:
				case VarType::Var:
				case VarType::UserData:
					*Trans += *Ex;
					break;
				case VarType::Byte:
				case VarType::Integer:
				case VarType::Number:
					//std::cout << *Ex << "\n"; // debug
					if (!Ex->size())
						*Trans += "0"; 
					else {
						//*Trans += TrSPrintF("Scyndi.WantValue(\"%s\",%s)", _Declaration::E2S(fKind).c_str(), Ex->c_str());
						*Trans += "Scyndi.WantValue(\"";
						*Trans += _Declaration::E2S(fKind);
						*Trans += "\", ";
						*Trans += *Ex;
						*Trans += ")";
					}
					break;
				case VarType::String:
					if (!Ex->size()) *Trans += "\"\""; else {
						// *Trans += TrSPrintF("Scyndi.WantValue(\"STRING\",%s)", Ex->c_str());
						*Trans += "Scyndi.WantValue(\"STRING\",";
						*Trans += *Ex;
						*Trans += ")";
					}
					break;
				case VarType::Boolean:
					if (!Ex->size()) *Trans += "false"; else {
						*Trans += "Scyndi.WantValue(\"BOOL\",";
						*Trans += Ex->c_str();
						*Trans += ")";
					}
					break;
				case VarType::Delegate:
					if (!Ex->size()) *Trans += "nil"; else *Trans += TrSPrintF("Scyndi.WantValue(\"DELEGATE\",%s)", Ex->c_str());
					break;
				case VarType::Table:
					if (!Ex->size()) *Trans += "{}"; else *Trans += TrSPrintF("Scyndi.WantValue(\"TABLE\",%s)", Ex->c_str());
					break;
				default:
					TransError(TrSPrintF("Unknown function return type (%d)", (int)fKind));
				}
				Sc->DidReturn = true;
				*Trans += "\n";
			} break;
			case InsKind::Defer: {
				static size_t DeferCount{ 0 };
				if (!Ins->ScopeData->DeferID.size()) {
					Ins->ScopeData->DeferID = TrSPrintF("Scyndi_Defer_%08x_%s", DeferCount++, md5(srcfile + std::to_string(Ins->LineNumber)).c_str());
					*Trans += "local " + Ins->ScopeData->DeferID + " = {}\t";
				}
				*Trans += Ins->ScopeData->DeferID + "[ #" + Ins->ScopeData->DeferID + " + 1 ] = function()\n";
			} break;
			case InsKind::Break:
				*Trans += "break\n";
				break;
			case InsKind::ExternImport:
				break;
			case InsKind::MutedByIfDef:
				break;
			default:
				TransError(TrSPrintF("Unknown instruction kind (%d) (Internal error. Please report!)",(int)Ins->Kind));
				//break;
			}
		}
#pragma endregion

#pragma region "Last closure stuff added to the translation"
		*Trans += "\n\n";
		for (auto& Seal : ToSeal) *Trans += "Scyndi.Seal(\"" + Seal + "\");\n";
		if (HasInit) {
			*Trans += "\n\nfor _,ifunc in ipairs(" + InitTag + ") do ifunc() end; " + InitTag + " = nil";
		}
		Ret.Trans->Data->Value("Translation", "Target", "Lua");
		Ret.Trans->Data->Value("Translation", "Origin", "Scyndi");
		Ret.Trans->Data->Value("Translation", "Debug", boolstring(debug));
		for (auto M : Macros) Ret.Trans->Data->Value("Macros", M.first, M.second);
#pragma endregion
		return Ret.Trans;
	}

	Translation Translate(std::string source, std::string srcfile, Slyvina::JCR6::JT_Dir JD, GINIE Dat,bool debug, bool force) {
		return Translate(Split(source, '\n'), srcfile, JD, Dat, debug);
	}

}
