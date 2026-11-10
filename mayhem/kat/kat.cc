// Known-answer probe for the platform-neutral Rainmeter parsers, built with the project's normal flags.
// Prints "<name> <value>" lines; mayhem/test.sh compares them against expected values taken from
// Common/MathParser_Test.cpp and Common/StringParser_Test.cpp.
#include <cstdio>
#include <string>
#include <vector>

#include "CharacterEntityReference.h"
#include "MathParser.h"
#include "StringParser.h"

namespace {

bool Resolve(const WCHAR* str, int len, double* value, void* context)
{
	if (wcsncmp(str, L"a", len) == 0) { *value = 10.0; return true; }
	if (wcsncmp(str, L"bbb", len) == 0) { *value = 20.0; return true; }
	if (wcsncmp(str, L"ccc_", len) == 0 && context == (void*)1) { *value = 30.0; return true; }
	return false;
}

void Calc(const char* name, const WCHAR* formula, MathParser& parser)
{
	double value = 0.0;
	const WCHAR* error = parser.Parse(formula, &value);
	if (error == nullptr) printf("%s %g\n", name, value);
	else printf("%s error:%ls\n", name, error);
}

}  // namespace

int main()
{
	MathParser plain;
	MathParser withVars(Resolve);
	MathParser withContext(Resolve, (void*)1);

	Calc("calc.paren", L"-(-5-5)", plain);
	Calc("calc.round", L"round(1.555, 2)", plain);
	Calc("calc.clamp", L"clamp(6, -2, 3)", plain);
	Calc("calc.hex", L"0xA", plain);
	Calc("calc.binary", L"0b11", plain);
	Calc("calc.var", L"a + 5", withVars);
	Calc("calc.ctx", L"(ccc_)", withContext);
	Calc("calc.div0", L"1 / 0", plain);
	Calc("calc.unmatched", L"((1)", plain);

	{
		MathParser parser;
		double value = 0.0;
		const WCHAR* end = nullptr;
		const WCHAR* formula = L"(1 + min(2, 3))tail";
		parser.Parse(formula, &value, MathParser::ParseMode::MatchingClosingBracket, &end);
		printf("calc.bracket %g tail=%ls\n", value, end);
	}

	{
		const std::wstring buffer(L"3.123456");
		MathParser parser;
		double value = 0.0;
		parser.Parse(std::wstring_view(buffer.data(), 3), &value);
		printf("calc.bounded %g\n", value);
	}

	{
		StringParser p(L"PrefixSuffix");
		const bool a = p.Consume(L"Prefix");
		const bool b = p.ConsumeRest(L"Suffix");
		printf("sp.consume %d %d %d\n", a, b, p.IsConsumed());
	}

	{
		std::vector<std::wstring> tokens;
		StringParser::Split(L" one , two,,three ", L',', tokens);
		printf("sp.split %zu", tokens.size());
		for (const auto& t : tokens) printf(" [%ls]", t.c_str());
		printf("\n");
	}

	{
		StringParser p(L"12, 1F ,abc");
		const auto a = p.ConsumeInt();
		p.Consume(L',', StringParser::SkipWhitespace);
		const auto b = p.ConsumeHexByte(StringParser::SkipWhitespace);
		printf("sp.numbers %d %u\n", a.value_or(-1), b.value_or(0));
	}

	{
		std::wstring text = L"&lt;b&gt; &amp; &#65;&#x42; \\u0043 &bogus;";
		CharacterEntityReference::Decode(text, 1, true);
		printf("entity %ls\n", text.c_str());
	}

	return 0;
}
