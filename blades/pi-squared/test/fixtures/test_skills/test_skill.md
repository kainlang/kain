---
name: test-skill
description: A test skill for pi-squared testing
when_to_use: testing and validation
tags: test, validation, fixture
---

# Test Skill

This is a test skill used for pi-squared testing and validation.

## Usage

Use this skill when:

- Running automated tests of pi-squared's skills subsystem
- Validating frontmatter parsing
- Testing skill loading and formatting

## Example

```kain
use resources::skills
let skill = load_skill("test/fixtures/test_skills/test_skill.md")
match skill:
    Some(s) => println("Loaded: " + s.frontmatter.name)
    None => println("Failed to load skill")
```
