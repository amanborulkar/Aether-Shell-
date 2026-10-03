#!/bin/bash
set -u
cd "$(dirname "$0")/.." || exit 1

make aether >/dev/null || { echo "Build failed"; exit 1; }

PASS=0
FAIL=0
FAILED_SCRIPTS=""

for script in tests/integration/*.sh; do
    echo "=== $script ==="
    if bash "$script"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        FAILED_SCRIPTS="$FAILED_SCRIPTS $script"
    fi
    echo
done

echo "========================================="
echo "$PASS passed, $FAIL failed"
if [ "$FAIL" -ne 0 ]; then
    echo "Failed scripts:$FAILED_SCRIPTS"
    exit 1
fi
echo "ALL INTEGRATION TESTS PASSED"
