#!/bin/sh
# Usage: run.sh <compiler> <programs-dir> [compiler flags...]
# Each program starts with `/* exit: N */` (compile, run, expect exit code N)
# or `/* error: text */` (expect compilation to fail with `text` in stderr).
# If <name>.out exists next to the program, its stdout must match that file exactly, or, if
# <name>.tol also exists, numerically within that relative tolerance (absolute for |x| < 1).
compiler=$1; dir=$2; shift 2
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

# same_output <expected> <actual> <tolfile>: exact diff, or token-wise numeric compare if tolfile exists.
same_output() {
  [ -f "$3" ] || { diff -q "$1" "$2" >/dev/null; return; }
  awk -v tol="$(cat "$3")" 'NR==FNR { want[FNR]=$0; n=FNR; next }
    { nx=split(want[FNR], x, " "); ny=split($0, y, " "); if (nx!=ny) bad=1
      for (i=1; i<=nx; i++) if (x[i]!=y[i]) { d=x[i]-y[i]; if (d<0) d=-d; m=x[i]<0?-x[i]:x[i]; if (m<1) m=1; if (d>tol*m) bad=1 } }
    END { if (FNR!=n) bad=1; exit bad }' "$1" "$2"
}
for f in "$dir"/*.c; do
  name=$(basename "$f")
  header=$(head -n 1 "$f")
  case "$header" in
    "/* exit: "*) want=${header#/* exit: }; want=${want% */}
      if "$compiler" "$@" "$f" -o "$tmp/prog" 2>"$tmp/err"; then
        "$tmp/prog" >"$tmp/out"; got=$?
        expected="${f%.c}.out"
        if [ "$got" != "$want" ]; then ok=0; echo "FAIL $name: exit $got, want $want"
        elif [ -f "$expected" ] && ! same_output "$expected" "$tmp/out" "${f%.c}.tol"; then
          ok=0; echo "FAIL $name: stdout differs:"; diff "$expected" "$tmp/out" | head -5
        else ok=1; fi
      else ok=0; echo "FAIL $name: compile error: $(cat "$tmp/err")"; fi ;;
    "/* error: "*) want=${header#/* error: }; want=${want% */}
      if "$compiler" "$@" "$f" -o "$tmp/prog" 2>"$tmp/err"; then ok=0; echo "FAIL $name: expected compile error"
      elif grep -qF -- "$want" "$tmp/err"; then ok=1
      else ok=0; echo "FAIL $name: stderr lacks '$want': $(cat "$tmp/err")"; fi ;;
    *) ok=0; echo "FAIL $name: missing '/* exit: N */' or '/* error: ... */' header" ;;
  esac
  [ "$ok" = 1 ] && pass=$((pass+1)) || fail=$((fail+1))
done
echo "integration: $pass passed, $fail failed"
[ "$fail" = 0 ]
