// Fuzzes Rainmeter's Calc formula parser (Common/MathParser) with and without a variable resolver.
#include <cstddef>
#include <cstdint>

#include "MathParser.h"
#include "wide.h"

namespace {

bool ResolveVariable(const WCHAR* str, int len, double* value, void* context)
{
	if (context == nullptr || len <= 0) return false;

	*value = static_cast<double>(len) + static_cast<double>(str[0] % 7);
	return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size == 0) return -1;

	const std::wstring formula = WidenBytes(data + 1, size - 1);
	const bool withResolver = (data[0] & 1) != 0;

	MathParser parser(withResolver ? &ResolveVariable : nullptr, withResolver ? (void*)1 : nullptr);
	double result = 0.0;

	switch ((data[0] >> 1) & 3)
	{
	case 0:
		parser.Check(formula);
		break;

	case 1:
		parser.CheckedParse(formula, &result);
		break;

	case 2:
		parser.Parse(formula, &result);
		break;

	default:
		{
			const WCHAR* parseEnd = nullptr;
			parser.Parse(formula, &result, MathParser::ParseMode::MatchingClosingBracket, &parseEnd);
		}
		break;
	}

	return 0;
}
