#!/bin/sh
# Usage: run.sh <compiler> <programs-dir>
# Each program starts with `/* exit: N */` (compile, run, expect exit code N)
# or `/* error: text */` (expect compilation to fail with `text` in stderr).
compiler=$1; dir=$2
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0
for f in "$dir"/*.c; do
  name=$(basename "$f")
  header=$(head -n 1 "$f")
  case "$header" in
    "/* exit: "*) want=${header#/* exit: }; want=${want% */}
      if "$compiler" "$f" -o "$tmp/prog" 2>"$tmp/err"; then
        "$tmp/prog"; got=$?
        [ "$got" = "$want" ] && ok=1 || { ok=0; echo "FAIL $name: exit $got, want $want"; }
      else ok=0; echo "FAIL $name: compile error: $(cat "$tmp/err")"; fi ;;
    "/* error: "*) want=${header#/* error: }; want=${want% */}
      if "$compiler" "$f" -o "$tmp/prog" 2>"$tmp/err"; then ok=0; echo "FAIL $name: expected compile error"
      elif grep -qF -- "$want" "$tmp/err"; then ok=1
      else ok=0; echo "FAIL $name: stderr lacks '$want': $(cat "$tmp/err")"; fi ;;
    *) ok=0; echo "FAIL $name: missing '/* exit: N */' or '/* error: ... */' header" ;;
  esac
  [ "$ok" = 1 ] && pass=$((pass+1)) || fail=$((fail+1))
done
echo "integration: $pass passed, $fail failed"
[ "$fail" = 0 ]
