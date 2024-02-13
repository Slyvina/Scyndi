// Lic:
// Scyndi
// Keyword list
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
// Version: 23.11.01
// EndLic

#include <string>
#include <vector>
#include <map>

namespace Scyndi {
	std::vector < std::string > KeyWords{

		// Headers
		"SCRIPT",
		"MODULE",

		// Class
		"CLASS",
		"GROUP",
		"NEW",

		"CONSTRUCTOR",
		"DESTRUCTOR",

		// Init
		"INIT",

		// Declaration
		"STATIC",
		"ABSTRACT",
		"FINAL",
		"GLOBAL",

		// Return
		"RETURN",

		// Basic data types
		"VOID",
		"BYTE",
		"INT", "INTEGER",
		"BOOL", "BOOLEAN",
		"NUMBER",
		"STRING",
		"FUNCTION", "DELEGATE",
		"USERDATA",
		"VAR",
		"PLUA",

		// Infinite arguments
		"INFINITY",

		// Math
		"DIV", // This because the '//' Lua itself uses for that is in Scyndi reserved for comments

		// Loops and other scope based stuff
		"FOR",
		"WHILE",
		"IF",
		"ELSE",
		"ELSEIF","ELIF",
		"REPEAT",
		"UNTIL",
		"LOOPWHILE",
		"FOREVER",
		"END",
		"DO",
		"BREAK",

		// Switch
		"SWITCH", "SELECT",
		"CASE",
		"DEFAULT",
		"FALLTHROUGH"

		// For
		"IN",
		//"TO",
		//"STEP"

		// Value Based Keywords
		"NIL",
		"TRUE",
		"FALSE"

	};


	std::vector < std::string > Operators {
		"!",
		"%",
		"^",
		"&&",
		"*",
		"(",")",
		"-",
		",",
		"[",
		"]",
		"=",
		"!=",
		"==",
		"||",
		"/",
		"%",
		"<",">",
		"<=",">="
	};

	// Please note that I did only put in a few metamethods here.
	// This is only for quick operations after all
	std::map < std::string, std::vector < std::string > > MetaMethods{
		{"NEWINDEX",{"KEY","VALUE"}},
		{"INDEX",{"KEY"}},
		{"GC",{}},
		{"TOSTRING",{}}
	};
}