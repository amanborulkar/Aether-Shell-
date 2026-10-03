# Aether Shell

Aether Shell is a small Unix shell written in C17 as a learning project. It reads a line with arrow-key editing and history, splits it into pipeline stages and arguments, applies I/O redirection, and runs the commands with fork, execvp and waitpid. Only libc and POSIX are used.

## Build

    make          # build ./aether and ./aetherd
    make clean    # remove objects and binaries
    make run      # build, then run ./aether

## Built-ins

- `cd [dir]` — change directory (defaults to `$HOME`, or `/` if unset)
- `pwd` — print the current directory
- `history` — list the last 100 commands; `!!` repeats the most recent one
- `help` — list the built-ins
- `exit [n]` — leave the shell

## Remote Shell (aetherd)

`aetherd` is a Unix domain socket daemon. It accepts connections and starts a fresh `aether` process for each one, with the connection as the shell's stdin, stdout and stderr. Each connection gets its own shell process.

Build: `make` (builds both binaries)

Run the daemon: `./aetherd`, or `./aetherd -s /tmp/custom.sock` (default socket: `/tmp/aetherd.sock`; `make run-daemon` also works)

Connect: `nc -U /tmp/aetherd.sock`

Stop: Ctrl+C on the daemon (graceful) or `kill <pid>`

The socket is created with mode 0600, so only the owner can connect. `aether` is looked up on `PATH`, then as `./aether` in the daemon's working directory.

## Status

Milestone 5: aetherd socket daemon on top of the shell (REPL, parser, pipes, redirection, history, signals)