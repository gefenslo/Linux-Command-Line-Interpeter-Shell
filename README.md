# Lab 2: Simple Shell and Utilities

This project implements a simple Unix-like shell (`myshell`), a pipe demonstration program (`mypipe`), and a signal-handling loop program (`looper`). It is written in C and is intended for educational purposes, demonstrating process management, piping, signal handling, and command parsing.

## Components

### myshell
A custom shell that supports:
- Running external commands
- Input/output redirection
- Built-in commands:
  - `cd <dir>`: Change directory
  - `quit`: Exit the shell
  - Signal management commands:
    - `stop <pid>`: Send SIGSTOP to a process
    - `wakeup <pid>`: Send SIGCONT to a process
    - `ice <pid>`: Send SIGINT to a process
    - `nuke <pid>`: Send SIGKILL to a process group
- Foreground/background execution (blocking/non-blocking)
- Debug mode (`-d` flag): Prints debug information

### mypipe
A simple demonstration of using pipes and fork:
- Parent process writes a message to a pipe
- Child process reads the message and prints it
- Usage: `./mypipe <message>`

### looper
A program that installs custom signal handlers for SIGINT, SIGTSTP, and SIGCONT, and prints a message when a signal is received. It runs an infinite loop and is useful for testing signal delivery from the shell.

### lineParser.c / lineParser.h
Implements command line parsing for the shell, supporting argument splitting, redirection, and command chaining.

## Building

To build all programs, run:

```
make
```

This will produce the following executables:
- `myshell`
- `mypipe`
- `looper`

To clean up compiled binaries:

```
make clean
```

## Usage

- Start the shell:
  ```
  ./myshell
  ```
- Run the pipe demo:
  ```
  ./mypipe "Hello, world!"
  ```
- Run the looper:
  ```
  ./looper
  ```

## Files
- `myShell.c`: Main shell implementation
- `myPipe.c`: Pipe demo program
- `looper.c`: Signal handling loop
- `lineParser.c`, `lineParser.h`: Command line parser
- `Makefile`: Build instructions
- `test_input.txt`: Example input for testing

## Author
Gefen Slodownik
---

