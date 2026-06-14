# MyPlugin

> A markscript-powered pi-squared plugin that demonstrates the slash command
> and tool extension system.

## Metadata

| Property | Value |
|----------|-------|
| Name | my-plugin |
| Version | 1.0.0 |
| Commands | /hello, /status, /greet |
| Tools | greet, help |

## Commands

| Command | Description | Usage |
|---------|-------------|-------|
| hello | Say hello to the user | /hello [name] |
| status | Show plugin status | /status |
| greet | Greet with a custom message | /greet <message> |

## HelloCommand

This section implements the `/hello` command. When pi-squared dispatches
`/hello`, it triggers the intent below.

> print hello, world!

```markscript
print("Hello from MyPlugin 1.0.0!")
print("The /hello command was dispatched successfully.")
print("")
print("You can pass a name: /hello Alice")
print("This would print: Hello, Alice!")
```

## StatusCommand

> print status

```markscript
print("=== MyPlugin Status ===")
print("Version: 1.0.0")
print("Plugin: my-plugin")
print("Status: active")
print("Commands: hello, status, greet")
print("Tools: greet, help")
```

## GreetCommand

> print greet

```markscript
print("=== Greet Tool ===")
print("The greet tool lets you send a greeting message.")
print("Usage: usage of greet tool with a message")
```

## Tools

| Tool | Description | Arguments |
|------|-------------|-----------|
| greet | Send a greeting | message: String |
| help | Get help for this plugin | command: String |

## Help

> print help

```markscript
print("=== MyPlugin Help ===")
print("Available commands:")
print("  /hello [name]  - Say hello")
print("  /status        - Show plugin status")
print("  /greet <msg>   - Greet with message")
print("")
print("Available tools:")
print("  greet(message) - Send a greeting")
print("  help(command)  - Get tool help")
```
