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

run_shell "echo hello > $TMP/file" >/dev/null
check_eq "hello" "$(cat "$TMP/file" 2>/dev/null)" "> creates the file with its content"

run_shell "echo world >> $TMP/file" >/dev/null
check_eq $'hello\nworld' "$(cat "$TMP/file")" ">> appends a second line"

check_eq "2" "$(run_shell "wc -l < $TMP/file" | tr -d ' ')" "< feeds the file to stdin"

run_shell "cat < $TMP/file > $TMP/copy" >/dev/null
if cmp -s "$TMP/file" "$TMP/copy"; then
    pass "< and > together copy a file"
else
    fail "< and > together copy a file" "$(cat "$TMP/file")" "$(cat "$TMP/copy" 2>&1)"
fi

out=$(run_shell "ls /nonexistent 2> $TMP/err.txt")
check_eq "" "$out" "2> keeps stderr off stdout"
check_has "nonexistent" "$(cat "$TMP/err.txt" 2>&1)" "2> writes the error to the file"

out=$(run_shell "ls /nonexistent > $TMP/out.txt 2>&1")
check_eq "" "$out" "> file 2>&1 keeps both streams off stdout"
check_has "nonexistent" "$(cat "$TMP/out.txt" 2>&1)" "> file 2>&1 sends stderr into the file"

# 2>&1 before > — stderr goes to stdout-at-that-moment (original), stdout goes to file
out=$(run_shell "ls /nonexistent $TMP/out.txt 2>&1 > $TMP/o2.txt")
check_has "nonexistent" "$out" "2>&1 before > leaves stderr on stdout"
if [ -e "$TMP/o2.txt" ] && [ -s "$TMP/o2.txt" ]; then
    pass "2>&1 before > writes stdout to the file"
    check_has "out.txt" "$(cat "$TMP/o2.txt")" "2>&1 before > file contains the stdout listing"
else
    fail "2>&1 before > writes stdout to the file" "non-empty file" "$(ls -la "$TMP/o2.txt" 2>&1)"
fi

out=$(run_shell "ls /nonexistent $TMP/out.txt &> $TMP/both.txt")
check_eq "" "$out" "&> keeps both streams off stdout"
both=$(cat "$TMP/both.txt" 2>&1)
check_has "nonexistent" "$both" "&> captures stderr"
check_has "out.txt" "$both" "&> captures stdout"

run_shell "echo hi | cat > $TMP/pipe_redir.txt" >/dev/null
check_eq "hi" "$(cat "$TMP/pipe_redir.txt" 2>&1)" "pipe combined with redirect"

echo "OK: $0 ($PASS_COUNT checks)"
