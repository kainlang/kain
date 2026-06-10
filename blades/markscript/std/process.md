# Process

Markscript process management — spawn commands, capture output, handle exit codes.
Dispatches through the IVT to Kain's `std::process` bridge.

---

## run

Execute a command synchronously and capture its output.

> run "echo hello world"

```markscript
# Run a command and capture stdout
push("echo hello world")
call("run")
# Result is the command's stdout on the stack
```

---

## spawn

Launch a command asynchronously (non-blocking). Returns immediately.

> spawn "long_running_server --port 8080"

```markscript
# Start a background process
push("long_running_server --port 8080")
call("spawn")
# Returns immediately with process handle info
```

---

## exec

Execute a command with arguments. Equivalent to run with arguments joined.

> run "git status --short"

```markscript
# Run a git command
push("git status --short")
call("run")
```

---

## pipe

Chain two commands together. The output of the first feeds the input of the second.

> run "cat data.txt | grep error | wc -l"

```markscript
# Pipe commands through the shell
push("cat data.txt | grep error | wc -l")
call("run")
```

---

## env

Run a command with environment variables set.

> run "set FOO=bar && my_command"

```markscript
# Set env var and run command (Windows syntax)
push("set MARKS=1 && my_command.exe")
call("run")
```

---

## timeout

Run a command with a timeout. Kills the process if it exceeds the limit.

> run "timeout 5 slow_command"

```markscript
# Run with a 5-second timeout (platform-dependent)
push("timeout 5 slow_command.exe")
call("run")
```

---

## exit_code

Run a command and check its exit code.

> run "test_runner --suite integration"
> assert exit_code 0

```markscript
# Run a test suite and check it passed
push("test_runner --suite integration")
call("run")
# Exit code 0 means success
```

---

## parallel

Run multiple commands in parallel using spawn for each.

> spawn "server_a --port 8001"
> spawn "server_b --port 8002"
> spawn "worker --queue default"

```markscript
# Launch three services in parallel
push("server_a --port 8001")
call("spawn")
push("server_b --port 8002")
call("spawn")
push("worker --queue default")
call("spawn")
```
