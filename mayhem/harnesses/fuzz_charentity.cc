// Fuzzes HTML character-reference / \uXXXX decoding (Common/CharacterEntityReference), used by the
// WebParser measure on downloaded page text.
#include <cstddef>
#include <cstdint>

#include "CharacterEntityReference.h"
#include "wide.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size == 0) return -1;

	std::wstring text = WidenBytes(data + 1, size - 1);
	CharacterEntityReference::Decode(text, data[0] & 3, (data[0] & 4) != 0);

	return 0;
}
