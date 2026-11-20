#!/bin/sh
# Golden-file tests.  ./tests/run_tests.sh            -> run
#                     ./tests/run_tests.sh --update   -> regenerate golden files
cd "$(dirname "$0")/.." || exit 2
BIN=./minorb
fail=0; n=0
check() {  # check <name> <actual-file> <expected-file>
  n=$((n+1))
  if [ "$UPDATE" = 1 ]; then cp "$2" "$3"; return; fi
  if cmp -s "$2" "$3"; then :; else echo "FAIL: $1"; diff "$3" "$2" | head -10; fail=$((fail+1)); fi
}
[ "$1" = "--update" ] && UPDATE=1
tmp=$(mktemp)
for f in tests/valid/*.b; do
  b=$(basename "$f" .b)
  $BIN --check "$f" > "$tmp"
  if [ -s "$tmp" ]; then echo "FAIL: $b should be error-free:"; cat "$tmp"; fail=$((fail+1)); fi
  $BIN --tokens "$f" > "$tmp" 2>/dev/null; check "$b.tokens" "$tmp" "tests/expected/$b.tokens"
  $BIN --ast "$f" > "$tmp" 2>/dev/null;    check "$b.ast"    "$tmp" "tests/expected/$b.ast"
  $BIN --html "$f" > "$tmp" 2>/dev/null;   check "$b.html"   "$tmp" "tests/expected/$b.html"
done
for f in tests/invalid/*.b; do
  b=$(basename "$f" .b)
  $BIN --check "$f" > "$tmp"; rc=$?
  [ "$UPDATE" != 1 ] && [ $rc -ne 1 ] && { echo "FAIL: $b should exit with 1 (got $rc)"; fail=$((fail+1)); }
  check "$b.diag" "$tmp" "tests/expected/$b.diag"
  $BIN --ast "$f" > "$tmp" 2>/dev/null;   check "$b.ast" "$tmp" "tests/expected/$b.ast"
  $BIN --ansi "$f" > "$tmp" 2>/dev/null;  check "$b.ansi" "$tmp" "tests/expected/$b.ansi"
done
rm -f "$tmp"
[ "$UPDATE" = 1 ] && { echo "golden files updated"; exit 0; }
[ $fail -eq 0 ] && echo "all $n checks passed" || { echo "$fail failure(s)"; exit 1; }
