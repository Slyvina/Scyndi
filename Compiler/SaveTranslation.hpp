#pragma once
#include <JCR6_Write.hpp>
#include "Translate.hpp"

namespace Scyndi {
	bool SaveTranslation(Translation Trans, Slyvina::JCR6::JT_Create Out, std::string Storage);
}