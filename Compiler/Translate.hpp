#pragma once
#include <Slyvina.hpp>
#include <JCR6_Core.hpp>

namespace Scyndi {

	struct _Translation {
		std::string
			LuaSource{ "" },
			Headers{ "" }; // Needed for imports with global definitions
	};
	typedef std::shared_ptr<_Translation> Translation;

	/// <summary>
	/// Returns an empty string if the last translation was succesful, otherwise it will return the error message
	/// </summary>
	/// <returns></returns>
	std::string TranslationError();

	Translation Translate(Slyvina::VecString sourcelines, std::string srcfile = "", Slyvina::JCR6::JT_Dir JD = nullptr);
	Translation Translate(std::string source, std::string srcfile = "", Slyvina::JCR6::JT_Dir JD = nullptr);

}
