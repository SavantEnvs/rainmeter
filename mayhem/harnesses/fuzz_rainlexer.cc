// Fuzzes RainLexer, the Scintilla/Notepad++ lexer for Rainmeter skin .ini files (RainLexer/RainLexer/Lexer.cpp).
//
// Uses upstream's own in-memory Scintilla document (RainLexer/Test/TestDocument.h) and the keyword
// lists from its shipped Languages.xml (turned into keywords.inc by build.sh). It enforces that styles
// and fold levels are never written outside the document, both when styling the whole document and
// when styling it in chunks, resuming at the start of the line styling stopped in (how Scintilla styles
// on demand). That chunked styling equals whole-document styling is checked on upstream's golden cases
// by mayhem/test.sh, not here: on malformed input the two legitimately differ, and an abort in this
// file would be reported as a harness defect rather than a lexer one.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "ILexer.h"
#include "Scintilla.h"

#include "TestDocument.h"

namespace RainLexer {
Scintilla::ILexer5* SCI_METHOD CreateLexer(const char* name);
}

namespace {

const char* const kKeywords[] = {
#include "keywords.inc"
};

constexpr Sci_Position kChunkSizes[] = { 0, 1, 7, 64, 997 };

Scintilla::ILexer5* g_Lexer = nullptr;

void LexIncremental(TestDocument& doc, Sci_Position chunk)
{
	const Sci_Position length = doc.Length();
	Sci_Position target = 0;

	while (target < length)
	{
		target = std::min(target + chunk, length);

		const Sci_Position start = doc.LineStart(doc.LineFromPosition(doc.EndStyled()));
		g_Lexer->Lex(start, target - start, 0, &doc);
		g_Lexer->Fold(start, target - start, 0, &doc);
	}
}

}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	g_Lexer = RainLexer::CreateLexer("Rainmeter");
	if (g_Lexer == nullptr) abort();

	for (int i = 0; i < 9; ++i)
	{
		g_Lexer->WordListSet(i, kKeywords[i]);
	}

	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2) return -1;

	const Sci_Position chunk = kChunkSizes[data[0] % (sizeof(kChunkSizes) / sizeof(kChunkSizes[0]))];
	const std::string text(reinterpret_cast<const char*>(data + 1), size - 1);

	TestDocument whole(text);
	g_Lexer->Lex(0, whole.Length(), 0, &whole);
	g_Lexer->Fold(0, whole.Length(), 0, &whole);

	if (whole.StylesOutOfRange() != 0 || whole.LevelsOutOfRange() != 0) abort();

	if (chunk != 0)
	{
		TestDocument piecewise(text);
		LexIncremental(piecewise, chunk);

		if (piecewise.StylesOutOfRange() != 0 || piecewise.LevelsOutOfRange() != 0) abort();
	}

	return 0;
}
