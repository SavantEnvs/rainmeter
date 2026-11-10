#!/usr/bin/env bash
#
# mayhem/test.sh — RUNS the pre-built oracle (mayhem/build.sh compiled it with normal flags, no sanitizers):
#   * upstream's RainLexer golden-file runner over RainLexer/Test/Cases/*.ini (styled-output diffs)
#   * known-answer probes for MathParser / StringParser / CharacterEntityReference, compared here
# Does not build. A missing binary is a failure.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

LEXER=mayhem/build/test/lexer_test
KAT=mayhem/build/test/kat
passed=0
failed=0

if [ ! -x "$LEXER" ] || [ ! -x "$KAT" ]; then
  echo "oracle binaries missing (mayhem/build.sh should have built them)" >&2
  emit_ctrf "rainlexer-golden+kat" 0 1
  exit $?
fi

# --- RainLexer golden files: last line is "<N> passed, <M> failed" ---
lex_out="$("$LEXER" RainLexer/Config/Languages.xml RainLexer/Test/Cases/*.ini 2>&1)"
echo "$lex_out" | tail -n 5
lex_line="$(printf '%s\n' "$lex_out" | grep -E '^[0-9]+ passed, [0-9]+ failed' | tail -n 1)"
if [[ "$lex_line" =~ ^([0-9]+)\ passed,\ ([0-9]+)\ failed ]]; then
  lp="${BASH_REMATCH[1]}"; lf="${BASH_REMATCH[2]}"
  if [ "$lp" -lt 21 ]; then lf=$(( lf + 1 )); echo "FAIL: expected at least 21 lexer cases, ran $lp" >&2; fi
  passed=$(( passed + lp )); failed=$(( failed + lf ))
else
  echo "FAIL: no lexer summary line" >&2
  failed=$(( failed + 1 ))
fi

# --- known answers ---
declare -A want=(
  [calc.paren]="10"
  [calc.round]="1.56"
  [calc.clamp]="3"
  [calc.hex]="10"
  [calc.binary]="3"
  [calc.var]="15"
  [calc.ctx]="30"
  [calc.div0]="error:Division by 0"
  [calc.unmatched]="error:Internal error"
  [calc.bracket]="3 tail=tail"
  [calc.bounded]="3.1"
  [sp.consume]="1 1 1"
  [sp.split]="3 [one] [two] [three]"
  [sp.numbers]="12 31"
  [entity]="<b> & AB C &bogus;"
)
declare -A got=()
while IFS= read -r line; do
  got["${line%% *}"]="${line#* }"
done < <("$KAT" 2>&1)

for k in "${!want[@]}"; do
  if [ "${got[$k]-__missing__}" == "${want[$k]}" ]; then
    passed=$(( passed + 1 ))
  else
    failed=$(( failed + 1 ))
    echo "FAIL $k: want [${want[$k]}] got [${got[$k]-__missing__}]" >&2
  fi
done

emit_ctrf "rainlexer-golden+kat" "$passed" "$failed"
