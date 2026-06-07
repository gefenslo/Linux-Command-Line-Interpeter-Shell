# Simple Shell and Utilities

This project implements a Unix-like shell and a set of small helper programs for testing process management, signals, redirection, pipes, and shell history. It is written in C and is intended as a compact, reusable shell project.

## Components

### `myshell`
The main shell program. It supports:
- External commands via `fork()` and `execvp()`
- Foreground and background execution
- Input and output redirection
- A built-in `cd <dir>` command
- A built-in `quit` command
- Signal commands:
  - `stop <pid>` sends `SIGSTOP`
  - `wakeup <pid>` sends `SIGCONT`
  - `ice <pid>` sends `SIGINT`
  - `nuke <pid>` sends `SIGKILL` to a process group
- A process list command:
  - `procs`
- History support:
  - `history`
  - `!!`
  - `!n`
- One-pipe command execution between two processes
- Debug mode with the `-d` flag

### `mypipeline`
A standalone pipe demonstration program. It shows how to create a pipe between two child processes and runs the equivalent of:
- `ps -xl | grep 5`

### `looper`
A helper program for testing signal handling. It installs custom handlers for `SIGINT`, `SIGTSTP`, and `SIGCONT`, prints which signal was received, and then lets the default signal action take effect.

### `lineParser.c` / `lineParser.h`
Command-line parsing helpers used by `myshell` for argument splitting, redirection, blocking/background execution, and pipe chaining.

## Building

Build the main executables with:

```bash
make myshell
make mypipeline
make looper
```

Or build everything with:

```bash
make
```

To remove generated binaries:

```bash
make clean
```

## Usage

Start the shell:

```bash
./myshell
```

Run the shell in debug mode:

```bash
./myshell -d
```

Run the pipe demo:

```bash
./mypipeline
```

Run the signal-test helper:

```bash
./looper
```

Example shell commands:

```bash
ls
ls &
cat < input.txt
echo hello > output.txt
ls | wc -l
history
!!
!3
procs
stop 12345
```

## Files
- `myShell.c`: Shell implementation
- `myPipeLine.c`: Standalone pipe demonstration program
- `looper.c`: Signal-handling test program
- `lineParser.c`, `lineParser.h`: Parsing helpers
- `Makefile`: Build rules for the executables

## Author
Gefen Slodownik

