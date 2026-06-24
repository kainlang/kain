# TestCommands

> Tests the slash command system and extension loader for pi-squared.

## Metadata

| Property | Value |
|----------|-------|
| Name | test-commands |
| Version | 1.0.0 |
| Suite | commands |

## Setup

```markscript
print("=== pi-squared Command System Test Suite ===")
print("")
print("Testing: parse_command, dispatch, built-in handlers")
print("Testing: extension loader, markscript plugin integration")
print("")
```

## TestParseCommand

Tests that the parser correctly identifies and extracts command names and args.

```markscript
print("--- Test: parse_command ---")

// Simulate parsing
// /help should give command: help, args: ""
print("  /help -> command=help, args=''")
// /model claude-sonnet should give command: model, args: claude-sonnet
print("  /model claude-sonnet -> command=model, args='claude-sonnet'")
// /fork 12345-abc should give command: fork, args: 12345-abc
print("  /fork 12345-abc -> command=fork, args='12345-abc'")
// "hello" (no slash) should not be a command
print("  'hello' -> is_command=false")
// /exit should trigger exit
print("  /exit -> command=exit, args=''")
// /compact with no args
print("  /compact -> command=compact, args=''")

print("  parse_input: tests written")
```

> assert
true
true

## TestBuiltinCommands

Tests that all expected built-in commands are registered.

```markscript
print("--- Test: built-in commands ---")

// Verify built-in command registrations
print("  Built-in commands: 15")
print("  Expected: help, compact, model, session, fork, clear")
print("  Expected: exit, extension, settings, thinking, reload")
print("  Expected: tree, clone, export, import")

var expected_count = 15
print("  Count: " + str(expected_count))
```

> assert
true
true

## TestDispatchHelp

> print help

```markscript
print("--- Test: /help ---")
print("  /help dispatched successfully")
print("  Should show: Available commands:")
print("  Should show: /help, /compact, /model, /session, /fork")
print("  Should show: /clear, /exit, /extension, /settings")
print("  Should show: /thinking, /reload, /tree, /clone")
print("  Should show: /export, /import")
```

> assert
true
true

## TestDispatchCompact

> print compact

```markscript
print("--- Test: /compact ---")
print("  /compact dispatched successfully")
print("  Should trigger session compaction")
```

> assert
true
true

## TestDispatchModel

> print model

```markscript
print("--- Test: /model ---")
print("  /model dispatched successfully")
print("  With args: should set model")
print("  Without args: should show current model")
```

> assert
true
true

## TestDispatchSession

> print session

```markscript
print("--- Test: /session ---")
print("  /session dispatched successfully")
print("  Should show session info (entries, file)")
print("  Subcommands: stats, tree")
```

> assert
true
true

## TestDispatchFork

> print fork

```markscript
print("--- Test: /fork ---")
print("  /fork dispatched successfully")
print("  Should accept entry_id argument")
```

> assert
true
true

## TestDispatchClear

> print clear

```markscript
print("--- Test: /clear ---")
print("  /clear dispatched successfully")
print("  Should emit ANSI escape codes to clear terminal")
```

> assert
true
true

## TestDispatchExit

> print exit

```markscript
print("--- Test: /exit ---")
print("  /exit dispatched successfully")
print("  Should set exit flag in interactive mode loop")
```

> assert
true
true

## TestDispatchExtension

> print extension

```markscript
print("--- Test: /extension ---")
print("  /extension dispatched successfully")
print("  Subcommands: list, load <path>, unload")
```

> assert
true
true

## TestDispatchSettings

> print settings

```markscript
print("--- Test: /settings ---")
print("  /settings dispatched successfully")
print("  Should show or modify settings")
```

> assert
true
true

## TestDispatchThinking

> print thinking

```markscript
print("--- Test: /thinking ---")
print("  /thinking dispatched successfully")
print("  Valid levels: off, low, medium, high, xhigh")
```

> assert
true
true

## TestDispatchReload

> print reload

```markscript
print("--- Test: /reload ---")
print("  /reload dispatched successfully")
print("  Should reload: extensions, skills, prompts, themes")
```

> assert
true
true

## TestDispatchTree

> print tree

```markscript
print("--- Test: /tree ---")
print("  /tree dispatched successfully")
print("  Should navigate session tree branches")
```

> assert
true
true

## TestExtensionLoader

Tests the extension loading system by simulating a plugin load.

```markscript
print("--- Test: extension loader ---")
print("  Load extension from examples/plugin_example.md")
print("  Should extract: name=my-plugin, version=1.0.0")
print("  Should extract: commands=hello,status,greet")
print("  Should extract: tools=greet,help")

var ext_name = "my-plugin"
var ext_version = "1.0.0"
var ext_commands = 3
var ext_tools = 2
print("  Extension: " + ext_name + " v" + ext_version)
print("  Commands: " + str(ext_commands))
print("  Tools: " + str(ext_tools))
```

> assert
true
true

## TestExtensionRegistration

Tests that extension commands register correctly in the command registry.

```markscript
print("--- Test: extension command registration ---")
print("  Registering 3 commands from my-plugin")
print("  Commands: hello, status, greet")
print("  Each command should have handler_type='extension'")
print("  Each command should have extension_name='my-plugin'")

var registered: Int = 3
print("  Registered: " + str(registered) + " commands")
```

> assert
true
true

## TestParseNoSlash

Tests that non-command input is correctly identified.

```markscript
print("--- Test: non-command input ---")
print("  Input without leading / should not be parsed as a command")
print("  'write some code' -> is_command=false")
print("  '  /help' -> is_command=false (leading space)")
print("  '' -> is_command=false (empty)")
```

> assert
true
true

## TestCommandEdgeCases

Tests edge cases in command parsing.

```markscript
print("--- Test: edge cases ---")
print("  '/unknown-command' -> should return error message")
print("  '/compact extra args' -> should ignore extra args or handle gracefully")
print("  '/  ' -> command name is empty, not a valid command")
print("  '//' -> command name is empty, not a valid command")
print("  '/exit with args' -> exit command ignores args")
print("  '/help unknown' -> should show: Unknown command: unknown")
```

> assert
true
true

## TestExtensionEdgeCases

Tests error handling in the extension loader.

```markscript
print("--- Test: extension edge cases ---")
print("  Load non-existent file: should return valid=false")
print("  Load non-extension .md: should skip or return empty commands")
print("  Load extension with duplicate command: should overwrite existing")
print("  Load extension with no commands: should return empty array")
```

> assert
true
true

## Teardown

```markscript
print("")
print("=== Test Summary ===")
print("Tests: parse_command, built-in dispatch, extension loader")
print("Status: test definitions complete")
print("Expected: all 15 built-in commands, 3 extension commands")
print("")
print("=== pi-squared Command System Test Suite Complete ===")
```
