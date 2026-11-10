#!/bin/sh
# SH-101: host test runner. Builds core, adapters and the test suites and
# runs `make -k check`, which executes every QtTest suite off-Sailfish.
set -e
cd "$(dirname "$0")/.."
echo "== Building shanty (host mode: core/adapters/tests) =="
rm -rf build-host
mkdir -p build-host
(cd build-host && qmake ../shanty.pro "CONFIG+=onHost" && make -j4)
echo "== Running tests (expected RED during Phase A; -k keeps going) =="
(cd build-host/tests && make -k check 2>&1) > /tmp/shanty-check.log || true
awk '/Start testing of/{sub(/.*Start testing of /,""); sub(/ .*/,""); name=$0}
     /^Totals:/{print name " -> " $0}' /tmp/shanty-check.log
