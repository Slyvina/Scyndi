#pragma once
#include <string>
#include <map>

namespace Scyndi {
	extern std::map<std::string, std::string> CoreGlobals;
}


// As the .cpp file where the actual defintion takes place is generated by a Lua script prior to compiling, 
// there's no point in having that in the github reposity, as it would only lead to conflicts. 
// The header file doesn't change though, and as such, here it is. :)