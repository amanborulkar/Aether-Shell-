#!/bin/bash
set -u

cd "$(dirname "$0")/../.." || exit 1
AETHER=./aether

PASS_COUNT=0

pass() { echo "PASS: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() {
    echo "FAIL: $1"
    echo "  expected: $2"
    echo "  got:      $3"
    exit 1
}
check_eq() {
    [ "$1" = "$2" ] && pass "$3" || fail "$3" "$1" "$2"
}

run_shell() {
    printf '%s\n' "$@" | "$AETHER" 2>/dev/null | tail -n +2 | sed 's/aether> //g'
}

check_eq "hi" "$(run_shell 'echo hi | cat')" "echo hi | cat"

check_eq "a" "$(run_shell 'printf a\nb\nc\n | sort | head -n 1')" "three-stage pipeline with sort and head"

n=$(run_shell 'ls src | wc -l' | tr -d ' ')
case $n in
    ''|*[!0-9]*) fail "ls src | wc -l prints a positive integer" "positive integer" "$n" ;;
    *) [ "$n" -gt 0 ] && pass "ls src | wc -l prints a positive integer" \
           || fail "ls src | wc -l prints a positive integer" "positive integer" "$n" ;;
esac

check_eq "x" "$(run_shell 'echo x | cat | cat | cat')" "four-stage pipeline"

check_eq $'0\nalive' "$(run_shell 'notacommand | wc -l' 'echo alive')" \
    "missing command in a pipeline does not kill the shell"

echo "OK: $0 ($PASS_COUNT checks)"
