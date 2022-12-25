#include "Config.hpp"
using namespace Slyvina::Units;
namespace Scyndi {
	
	static ParsedArg _Args;

	void RegArgs(Slyvina::Units::FlagConfig& cfg, int cnt, char** args) {
		_Args = ParseArg(cnt, args, cfg);
	}

	size_t NumFiles() { return _Args.arguments.size(); }

	std::vector<std::string>* Files() { return &_Args.arguments; }

}