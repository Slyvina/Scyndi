#pragma once
#include <SlyvArgParse.hpp>

namespace Scyndi {
	void RegArgs(Slyvina::Units::FlagConfig& cfg, int cnt, char** args);
	size_t NumFiles();
	std::vector<std::string>* Files();
}