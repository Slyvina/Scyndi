// Lic:
// Scyndi
// Config
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
#include <iostream>
#include "Config.hpp"
using namespace Slyvina::Units;
namespace Scyndi {
	
	static ParsedArg _Args;

	void RegArgs(Slyvina::Units::FlagConfig& cfg, int cnt, char** args) {
		_Args = ParseArg(cnt, args, cfg);
		/*
		for (auto& b : _Args.bool_flags) std::cout << "bool " << b.first << " = " << b.second << std::endl;
		for (auto& b : _Args.string_flags) std::cout << "str   " << b.first << " = " << b.second << std::endl;
		for (auto& b : _Args.int_flags) std::cout << "int   " << b.first << " = " << b.second << std::endl;
		for (auto& f : _Args.arguments) std::cout << "file " << f << std::endl;
		//*/
	}

	size_t NumFiles() { return _Args.arguments.size(); }

	std::vector<std::string>* Files() { return &_Args.arguments; }

	bool WantProject() { return !_Args.bool_flags["sl"]; }
	bool WantForce() { return _Args.bool_flags["force"]; }
	bool WantDebug() { return _Args.bool_flags["dbg"]; }

}