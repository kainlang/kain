BROKEN - no frontmatter

This skill file intentionally lacks YAML frontmatter to test error handling
in the skill loading subsystem. It should be rejected by load_skill().

The load_skill function checks for opening --- delimiter and returns none
if the file does not start with ---.
