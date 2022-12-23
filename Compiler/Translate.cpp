#include <SlyvString.hpp>

#include "Translate.hpp"
#include "ScyndiGlobals.hpp"

using namespace Slyvina;
using namespace Slyvina::Units;

namespace Scyndi {



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