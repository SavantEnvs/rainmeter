// Minimal stand-in for <Windows.h>, just enough for Rainmeter's platform-neutral parsers
// (Common/MathParser, StringParser, CharacterEntityReference and RainLexer) to compile on Linux.
#pragma once

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include <wctype.h>

typedef wchar_t WCHAR;
typedef wchar_t TCHAR;
typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef uint32_t DWORD;
typedef int BOOL;

#define TEXT(s) L##s
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#define _wcsnicmp wcsncasecmp
#define _wcsicmp wcscasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp

#define _TRUNCATE ((size_t)-1)

#ifdef __cplusplus
#include <stdarg.h>
#include <string>

// MSVC's wide printf reads %s as a wide string, glibc's as a narrow one: map %s to %ls.
inline int _snwprintf_s(wchar_t* buffer, size_t count, const wchar_t* format, ...)
{
	std::wstring fixed;
	for (const wchar_t* p = format; *p; ++p)
	{
		if (p[0] == L'%' && p[1] == L's') { fixed += L"%ls"; ++p; }
		else if (p[0] == L'%' && p[1] == L'%') { fixed += L"%%"; ++p; }
		else fixed += *p;
	}

	va_list args;
	va_start(args, format);
	const int n = vswprintf(buffer, count, fixed.c_str(), args);
	va_end(args);
	if (n < 0 && count > 0) buffer[count - 1] = L'\0';
	return n;
}
#endif
