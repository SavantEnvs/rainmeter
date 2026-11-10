// Fuzzes Rainmeter's StringParser (Common/StringParser), the tokenizer behind option parsing,
// driving it through every Consume* entry point, including the formula-backed number readers.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "MathParser.h"
#include "StringParser.h"
#include "wide.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 4) return -1;

	const auto option = static_cast<StringParser::Option>(data[0] & 0xF);
	const WCHAR delimiter = static_cast<WCHAR>(data[1] & 0x7F);
	const unsigned startOp = data[2];
	const std::wstring text = WidenBytes(data + 3, size - 3);

	MathParser math;
	StringParser parser(text);

	for (unsigned step = 0; step < 64 && !parser.IsConsumed(); ++step)
	{
		switch ((startOp + step) % 22)
		{
		case 0: parser.ConsumeUntil(delimiter, option); break;
		case 1: parser.ConsumeUntilOrRest(delimiter, option); break;
		case 2: parser.ConsumeUntil(L",;|", option); break;
		case 3: parser.ConsumeUntilOrRest(L"()\"'", option); break;
		case 4: parser.ConsumeUntilLast(delimiter, L'|'); break;
		case 5: parser.ConsumeDouble(option); break;
		case 6: parser.ConsumeRestDouble(option); break;
		case 7: parser.ConsumeInt(option); break;
		case 8: parser.ConsumeRestInt(option); break;
		case 9: parser.ConsumeUInt(option); break;
		case 10: parser.ConsumeHexByte(option); break;
		case 11: parser.ConsumeDoubleOrFormula(math, option); break;
		case 12: parser.ConsumeIntOrFormula(math, option); break;
		case 13: parser.ConsumeRestDoubleOrFormula(math, option); break;
		case 14: parser.ConsumeRestUIntOrFormula(math, option); break;
		case 15: parser.Consume(L"Meter", option); break;
		case 16: parser.ConsumeRest(L"Style", option); break;
		case 17: parser.ConsumeSuffix(L"x"); break;
		case 18: parser.ConsumeSuffixFromLast(delimiter); break;
		case 19: parser.Consume(delimiter, option); break;
		case 20: parser.ConsumeWhitespace(); break;
		default: parser.ConsumeRest(option); break;
		}
	}

	std::vector<std::wstring> tokens;
	StringParser::Split(text, delimiter, tokens, option);
	StringParser::ForEachToken(text, delimiter, [](std::wstring_view) {}, option);

	return 0;
}
