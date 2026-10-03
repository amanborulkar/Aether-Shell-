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
check_line() {
    if printf '%s\n' "$2" | grep -qxF -- "$1"; then
        pass "$3"
    else
        fail "$3" "line: $1" "$2"
    fi
}

run_shell() {
    printf '%s\n' "$@" | "$AETHER" 2>/dev/null | tail -n +2 | sed 's/aether> //g'
}
run_shell_err() {
    printf '%s\n' "$@" | "$AETHER" 2>&1 >/dev/null
}

out=$(run_shell 'echo one' 'echo two' 'echo three' 'history')
check_line "1 echo one" "$out" "history lists the first command"
check_line "2 echo two" "$out" "history lists the second command"
check_line "3 echo three" "$out" "history lists the third command"
check_line "4 history" "$out" "history records the history command itself"

cwd=$(pwd -P)
out=$(run_shell 'pwd' '!!')
count=$(printf '%s\n' "$out" | grep -cxF -- "$cwd")
check_eq "2" "$count" "!! runs pwd a second time"
check_line "pwd" "$out" "!! prints the expanded command"

check_eq "aether: !!: event not found" "$(run_shell_err '!!')" "!! with empty history reports an error"
check_eq "alive" "$(run_shell '!!' 'echo alive')" "shell survives !! with empty history"

echo "OK: $0 ($PASS_COUNT checks)"
