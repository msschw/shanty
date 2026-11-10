#!/bin/sh
# SH-701: single documented entry point for the host test sweep (Epic SH-E7).
# Builds core/adapters/presentation/tests in host mode and runs every QtTest
# suite, then prints a per-suite Totals summary table and reflects the result
# in the exit code (0 = gate green, 1 = any failure).
#
# Usage: scripts/run-tests.sh [BUILD-TREE]
#   BUILD-TREE   build directory, absolute or relative to the repo root.
#                Default: build-host.
#   SHANTY_QMAKE  qmake to use. Default: /usr/bin/qmake if it exists and is
#                executable, otherwise plain `qmake` from $PATH (SDK side).
#
# Idempotent and re-runnable: the build tree is NEVER deleted (qmake/make
# regenerate what is missing inside it), so the script is safe to point at a
# dedicated tree you keep between runs. Nothing outside BUILD-TREE is touched
# except one temporary log (auto-removed).
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tree=${1:-build-host}
case $tree in
    /*) ;;  # absolute
    *)  tree=$repo/$tree ;;
esac

if [ -n "${SHANTY_QMAKE:-}" ]; then
    qmake=$SHANTY_QMAKE
elif [ -x /usr/bin/qmake ]; then
    qmake=/usr/bin/qmake
else
    qmake=qmake
fi

mkdir -p "$tree"
tmplog=$(mktemp "${TMPDIR:-/tmp}/shanty-check.XXXXXX.log")
trap 'rm -f "$tmplog"' EXIT INT TERM

echo "== Shanty host test sweep =="
echo "repo:   $repo"
echo "tree:   $tree (kept; not cleaned)"
echo "qmake:  $qmake"

echo "== [1/3] qmake ../shanty.pro CONFIG+=onHost =="
(cd "$tree" && exec "$qmake" "$repo/shanty.pro" "CONFIG+=onHost")

echo "== [2/3] make (core, adapters, presentation, tests) =="
make -C "$tree" -j4

echo "== [3/3] tests: make -k check (log: $tmplog, shown below) =="
status=0
(cd "$tree/tests" && exec make -k check) >>"$tmplog" 2>&1 || status=$?
cat "$tmplog"

printf '\n== Per-suite Totals summary ==\n'
awk_status=0
awk '
    /Start testing of/ { name = $(NF-1) }   # "********* Start testing of TestX *********"
    /^Totals:/ {
        gsub(/,/, "")
        printf "%-23s pass %5s  fail %3s  skip %3s  black %3s  %s\n", \
            name, $2, $4, $6, $8, $NF
        suites++
        passed  += $2
        failed  += $4
        skipped += $6
        if ($4 > 0 || $6 > 0 || $8 > 0) bad++
    }
    END {
        printf "%-23s pass %5d  fail %3d  skip %3d  %s\n", \
            "TOTAL (" suites " suites)", passed, failed, skipped, ""
        if (suites == 0) {
            print "RESULT: FAIL — no test suites were run (check the build log)"
            exit 1
        }
        if (bad > 0 || failed > 0 || skipped > 0) {
            printf "RESULT: FAIL — %d case(s) failed, %d skipped\n", failed, skipped
            exit 1
        }
        printf "RESULT: PASS — %d suites, %d cases, 0 failed, 0 skipped\n", suites, passed
        exit 0
    }
' "$tmplog" > "$tmplog.sum" || awk_status=$?
cat "$tmplog.sum"

echo "== Sweep result: make check status=$status, summary lint=$awk_status =="
if [ "$status" -ne 0 ] || [ "$awk_status" -ne 0 ]; then
    echo "SH-701 automated gate: FAILED"
    exit 1
fi
echo "SH-701 automated gate: PASSED"
