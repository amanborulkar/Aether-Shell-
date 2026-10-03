#!/bin/bash
set -u

cd "$(dirname "$0")/../.." || exit 1
AETHER=./aether
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

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
run_shell_err() {
    printf '%s\n' "$@" | "$AETHER" 2>&1 >/dev/null
}
run_status() {
    printf '%s\n' "$@" | "$AETHER" >/dev/null 2>&1
    echo $?
}

check_eq "alive" "$(run_shell 'ls >' 'echo alive')" "missing redirect filename: shell survives"

check_eq "alive" "$(run_shell "ls > $TMP/a > $TMP/b" 'echo alive')" "duplicate redirect: shell survives"
[ ! -e "$TMP/a" ] && [ ! -e "$TMP/b" ] \
    && pass "duplicate redirect creates no files" \
    || fail "duplicate redirect creates no files" "no files" "$(ls "$TMP")"

check_eq "alive" "$(run_shell 'ls |' 'echo alive')" "trailing pipe: shell survives"
check_eq "alive" "$(run_shell '| ls' 'echo alive')" "leading pipe: shell survives"
check_eq "alive" "$(run_shell 'ls || ls' 'echo alive')" "empty pipeline stage: shell survives"

check_eq "aether: /nonexistent/path: No such file or directory" \
    "$(run_shell_err 'cat < /nonexistent/path')" \
    "unreadable input file reports an error"
check_eq "alive" "$(run_shell 'cat < /nonexistent/path' 'echo alive')" \
    "unreadable input file: shell survives"

check_eq "alive" "$(run_shell 'ls >' "ls > $TMP/a > $TMP/b" 'ls |' '| ls' \
    'cat < /nonexistent/path' 'echo alive')" "echo alive after all error cases"
check_eq "0" "$(run_status 'ls >' '| ls' 'cat < /nonexistent/path')" \
    "shell exits 0 after error cases"

echo "OK: $0 ($PASS_COUNT checks)"
