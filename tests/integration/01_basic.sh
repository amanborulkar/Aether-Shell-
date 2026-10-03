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
check_has() {
    if printf '%s\n' "$2" | grep -qF -- "$1"; then
        pass "$3"
    else
        fail "$3" "contains: $1" "$2"
    fi
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

out=$(run_shell 'pwd')
[ -n "$out" ] && pass "pwd prints a non-empty path" || fail "pwd prints a non-empty path" "non-empty" "$out"
check_eq "$(pwd -P)" "$out" "pwd prints the current directory"

check_eq "hello" "$(run_shell 'echo hello')" "echo hello"
check_eq "hi there" "$(run_shell 'echo hi there')" "echo hi there"

check_has "built-ins" "$(run_shell 'help')" "help mentions built-ins"

check_eq "aether: notacommand_xyz: No such file or directory" \
    "$(run_shell_err 'notacommand_xyz' 'echo still-alive')" \
    "unknown command reports error on stderr"
check_eq "still-alive" "$(run_shell 'notacommand_xyz' 'echo still-alive')" \
    "shell survives an unknown command"

check_eq "ok" "$(run_shell '' '   ' 'echo ok')" "empty and whitespace-only lines are skipped"
check_eq "0" "$(run_status '' '   ')" "empty input lines do not crash the shell"

check_eq "0" "$(run_status 'exit')" "exit returns 0"
check_eq "3" "$(run_status 'exit 3')" "exit n returns n"
check_eq "0" "$(run_status 'echo bye')" "EOF exits with status 0"

echo "OK: $0 ($PASS_COUNT checks)"
