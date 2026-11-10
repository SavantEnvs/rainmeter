#!/usr/bin/env bash
#
# mayhem/build.sh — build the fuzz harnesses and the known-answer test binaries.
#
# Rainmeter itself is a Windows desktop app, so only its platform-neutral parsers are built here, on
# Linux, behind a tiny <Windows.h> stand-in (mayhem/shim/Windows.h):
#   Common/MathParser.cpp, Common/StringParser.cpp, Common/CharacterEntityReference.cpp  (wchar_t strings)
#   RainLexer/RainLexer/Lexer.cpp + Lexilla's WordList.cxx                               (Scintilla lexer)
# Everything is built out of tree into mayhem/build (gitignored; recreated on every run).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
rm -rf "$B"
mkdir -p "$B/shim/wrl" "$B/gen" "$B/obj" "$B/test"

# Headers Common/StdAfx.h includes for the Windows build; none are needed by the parsers.
for h in crtdbg.h Commctrl.h d2d1_1.h d2d1helper.h dwrite_1.h Shlobj.h Uxtheme.h wincodec.h VersionHelpers.h wrl/client.h; do
  : > "$B/shim/$h"
done

# The lexer's word lists come from the Notepad++ configuration it ships with, like upstream's test runner.
python3 - "$SRC/RainLexer/Config/Languages.xml" "$B/gen/keywords.inc" <<'PY'
import re, sys
xml = open(sys.argv[1], encoding="utf-8").read()
out = []
for i in range(9):
    m = re.search(r'<Keywords name="%d">(.*?)</Keywords>' % i, xml, re.S)
    if not m or not m.group(1).strip():
        sys.exit("keyword list %d missing from %s" % (i, sys.argv[1]))
    out.append('R"KW(%s)KW",' % m.group(1))
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY

INC=(-I"$SRC/mayhem/shim" -I"$B/shim" -I"$B/gen" -I"$SRC/Common" -I"$SRC/ThirdParty"
     -I"$SRC/RainLexer/RainLexer" -I"$SRC/RainLexer/Test"
     -I"$SRC/RainLexer/ThirdParty/Scintilla/include" -I"$SRC/RainLexer/ThirdParty/lexilla/include"
     -I"$SRC/RainLexer/ThirdParty/lexilla/lexlib")
LEXLIB="$SRC/RainLexer/ThirdParty/lexilla/lexlib"
PARSERS=("$SRC/Common/MathParser.cpp" "$SRC/Common/StringParser.cpp" "$SRC/Common/CharacterEntityReference.cpp")
LEXER=("$SRC/RainLexer/RainLexer/Lexer.cpp" "$LEXLIB/WordList.cxx")
CXXSTD=(-std=c++20 -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -w)

# 1) Fuzz build: the parsers themselves carry the sanitizers, SanCov and DWARF <= 3.
#    MathParser.cpp and StringParser.cpp do their bitwise/shift operators and number narrowing by casting
#    a double to long long/int and shifting by an unchecked amount. Those two UBSan checks, float-cast-overflow
#    and shift-exponent, fire on nearly every formula that uses such an operator and the result has no memory
#    effect (the value is a bounded integer or a double), so they are relaxed on those two files only; ASan and every other
#    UBSan check stay on and halting. The operator/value stack index errors are still reported.
FUZZ=($SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1)
objs=()
for f in "${PARSERS[@]}" "${LEXER[@]}"; do
  o="$B/obj/$(basename "$f").o"
  relax=()
  case "$f" in
    */MathParser.cpp|*/StringParser.cpp) relax=(-fno-sanitize=float-cast-overflow,shift-exponent) ;;
  esac
  "$CXX" "${FUZZ[@]}" "${relax[@]}" "${CXXSTD[@]}" -x c++ "${INC[@]}" -c "$f" -o "$o" &
  objs+=("$o")
  while [ "$(jobs -rp | wc -l)" -ge "$MAYHEM_JOBS" ]; do wait -n; done
done
"$CXX" "${FUZZ[@]}" -c "$SRC/mayhem/lsan_off.cc" -o "$B/obj/lsan_off.o" &
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/obj/standalone_main.o" &
wait

link_target() {   # <harness> <objects...>
  local name="$1"; shift
  "$CXX" "${FUZZ[@]}" "${CXXSTD[@]}" "${INC[@]}" $LIB_FUZZING_ENGINE \
    "$SRC/mayhem/harnesses/$name.cc" "$@" "$B/obj/lsan_off.o" -o "$SRC/$name"
  "$CXX" "${FUZZ[@]}" "${CXXSTD[@]}" "${INC[@]}" \
    "$SRC/mayhem/harnesses/$name.cc" "$@" "$B/obj/lsan_off.o" "$B/obj/standalone_main.o" -o "$SRC/$name-standalone"
}
P="$B/obj"
link_target fuzz_mathparser   "$P/MathParser.cpp.o"
link_target fuzz_stringparser "$P/StringParser.cpp.o" "$P/MathParser.cpp.o"
link_target fuzz_charentity   "$P/CharacterEntityReference.cpp.o"
link_target fuzz_rainlexer    "$P/Lexer.cpp.o" "$P/WordList.cxx.o"

# 2) Oracle build: the project's normal flags (no sanitizers, no fuzzer instrumentation). Asserts stay on,
#    as in upstream's own lexer test build.
NORMAL=(-O2 $COVERAGE_FLAGS "${CXXSTD[@]}")
"$CXX" "${NORMAL[@]}" "${INC[@]}" "$SRC/RainLexer/Test/TestRunner.cpp" "${LEXER[@]}" -o "$B/test/lexer_test" &
"$CXX" "${NORMAL[@]}" "${INC[@]}" "$SRC/mayhem/kat/kat.cc" "${PARSERS[@]}" -o "$B/test/kat" &
wait
