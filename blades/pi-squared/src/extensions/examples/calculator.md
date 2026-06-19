# CalculatorPlugin

A markscript-powered calculator for pi-squared. Demonstrates the Command-to-Tool bridge: `/calc 2+2` parses and evaluates math expressions through the markscript IVT handler.

## Metadata
| Property | Value |
|----------|-------|
| Name | calculator |
| Version | 1.0.0 |
| Description | Evaluate math expressions in the TUI |
| Tools | evaluate, simplify |
| API | calc |

## Commands
| Command | Description | Usage |
|---------|-------------|-------|
| calc | Evaluate a math expression | /calc <expression> |
| calc-help | Show calculator help | /calc-help |

## Tools
| Name | Description | Handler |
|------|-------------|---------|
| evaluate | Evaluate a math expression string | 201 |
| simplify | Simplify an algebraic expression | 202 |

## Widgets
| Widget | Type | Width | Update | Refresh Action |
|--------|------|-------|--------|----------------|
| calc_panel | info | 30 | 5000 | refresh calculator display |

## History
| Step | Expression | Result |
|------|------------|--------|
| 1 | 2 + 2 | 4 |
| 2 | (15 - 3) * 2 | 24 |
| 3 | 100 / 5 | 20 |
| 4 | 7 * 8 - 5 | 51 |
| 5 | (3 + 7) * (10 - 4) | 60 |

## Defaults
| Property | Value |
|----------|-------|
| default_base | 10 |
| max_expression_length | 256 |

## Handler

> evaluate expression

The handler above is triggered when the calculator tool is dispatched. The markscript VM accumulates the phrase hash from `> evaluate expression` and returns to the pi-squared dispatch loop with handler_id=201. The pi-squared plugin registry catches this dispatch, parses the argument string from the command, and routes the result back as the tool output.

```kain
// Kain implementation of the calculator eval function
// This fenced code block is extracted by the VM as a CodeBlockRecord
// and stored for future compilation through Kain's LLVM backend.

fn calc_eval(expr: String) -> Int:
    // Simple expression parser for arithmetic
    // Supports: +, -, *, /, parentheses
    var pos: Int = 0
    return parse_expr(expr, pos)

fn parse_expr(s: String, p: Int) -> Int:
    var result = parse_term(s, p)
    while p < len(s):
        let ch = text_substring_string(s, p, 1)
        if ch == "+":
            p = p + 1
            let rhs = parse_term(s, p)
            result = result + rhs
        elif ch == "-":
            p = p + 1
            let rhs = parse_term(s, p)
            result = result - rhs
        else: break
    return result

fn parse_term(s: String, p: Int) -> Int:
    var result = parse_factor(s, p)
    while p < len(s):
        let ch = text_substring_string(s, p, 1)
        if ch == "*":
            p = p + 1
            let rhs = parse_factor(s, p)
            result = result * rhs
        elif ch == "/":
            p = p + 1
            let rhs = parse_factor(s, p)
            if rhs != 0: result = result / rhs
        else: break
    return result

fn parse_factor(s: String, p: Int) -> Int:
    let ch = text_substring_string(s, p, 1)
    if ch == "(":
        p = p + 1
        let result = parse_expr(s, p)
        if p < len(s) and text_substring_string(s, p, 1) == ")":
            p = p + 1
        return result
    return parse_number(s, p)

fn parse_number(s: String, p: Int) -> Int:
    var result: Int = 0
    var neg: Bool = false
    if p < len(s) and text_substring_string(s, p, 1) == "-":
        neg = true
        p = p + 1
    while p < len(s):
        let ch = text_substring_string(s, p, 1)
        let cv = text_ord(ch)
        if cv >= 48 and cv <= 57:
            result = result * 10 + (cv - 48)
            p = p + 1
        else: break
    if neg: result = -result
    return result
```

## Test Cases
| Input | Expected | Operation |
|-------|----------|-----------|
| 2+2 | 4 | addition |
| 10-3 | 7 | subtraction |
| 6*7 | 42 | multiplication |
| 100/5 | 20 | division |
| (3+5)*2 | 16 | parentheses |
| 10/3 | 3 | integer division |
| -5+10 | 5 | negative numbers |

> verify test cases run correctly

## Performance
| Metric | Value | Unit |
|--------|-------|------|
| ParseTime | 0.02 | ms |
| EvalError | 0.00 | % |
| MaxDepth | 32 | nesting |
| SupportedOps | 5 | + - * / () |

## Known Issues
| Issue | Description | Workaround |
|-------|-------------|------------|
| Float | No float support yet | Use integer math |
| Negation | Leading minus only | Wrap in parentheses |
