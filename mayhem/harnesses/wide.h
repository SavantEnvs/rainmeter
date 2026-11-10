// Turns fuzzer bytes into the UTF-16 code units Rainmeter sees on Windows, held in wchar_t.
// UTF-8 sequences decode to code units (supplementary planes to surrogate pairs, lone surrogates
// stay encodable as 3-byte sequences); bytes that do not start a valid sequence map to themselves.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

inline std::wstring WidenBytes(const uint8_t* data, size_t size)
{
	std::wstring out;
	out.reserve(size);

	size_t i = 0;
	while (i < size)
	{
		const uint8_t b = data[i];
		size_t extra = 0;
		uint32_t cp = b;
		if (b >= 0xF0 && b <= 0xF4) { extra = 3; cp = b & 0x07; }
		else if (b >= 0xE0) { extra = (b < 0xF0) ? 2 : 0; cp = b & 0x0F; }
		else if (b >= 0xC2 && b < 0xE0) { extra = 1; cp = b & 0x1F; }

		bool ok = extra != 0 && i + extra < size;
		for (size_t k = 1; ok && k <= extra; ++k)
		{
			const uint8_t c = data[i + k];
			if ((c & 0xC0) != 0x80) ok = false;
			cp = (cp << 6) | (c & 0x3F);
		}

		if (!ok || (extra == 3 && cp > 0x10FFFF))
		{
			out.push_back(static_cast<wchar_t>(b));
			++i;
			continue;
		}

		if (cp >= 0x10000)
		{
			cp -= 0x10000;
			out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
			out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
		}
		else
		{
			out.push_back(static_cast<wchar_t>(cp));
		}
		i += extra + 1;
	}

	return out;
}
