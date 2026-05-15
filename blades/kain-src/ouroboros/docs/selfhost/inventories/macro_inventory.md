# Self-Host Macro Inventory

## Global Classification

| Macro | Count | Classification |
|---|---:|---|
| `addr_of` | 1 | review |
| `allow` | 5 | review |
| `arg` | 54 | preserve |
| `assert` | 240 | reject |
| `assert_eq` | 184 | reject |
| `cfg` | 146 | preserve |
| `command` | 7 | preserve |
| `debug_assert` | 1 | reject |
| `default` | 1 | review |
| `derive` | 266 | preserve |
| `derive::ClapParser` | 1 | preserve |
| `derive::Clone` | 239 | preserve |
| `derive::Copy` | 46 | preserve |
| `derive::Debug` | 261 | preserve |
| `derive::Default` | 23 | preserve |
| `derive::Deserialize` | 34 | preserve |
| `derive::Eq` | 44 | preserve |
| `derive::Error` | 2 | preserve |
| `derive::Hash` | 7 | preserve |
| `derive::Logos` | 1 | preserve |
| `derive::Ord` | 1 | preserve |
| `derive::Parser` | 1 | preserve |
| `derive::PartialEq` | 140 | preserve |
| `derive::PartialOrd` | 1 | preserve |
| `derive::Serialize` | 42 | preserve |
| `derive::Subcommand` | 1 | preserve |
| `derive::clap::Subcommand` | 1 | preserve |
| `env` | 3 | review |
| `eprint` | 2 | lower_directly |
| `eprintln` | 188 | lower_directly |
| `error` | 18 | preserve |
| `format` | 1595 | lower_directly |
| `from` | 2 | preserve |
| `json` | 2 | review |
| `logos` | 1 | review |
| `matches` | 151 | lower_directly |
| `option_env` | 8 | review |
| `panic` | 87 | reject |
| `print` | 3 | lower_directly |
| `println` | 395 | lower_directly |
| `regex` | 10 | review |
| `serde` | 35 | review |
| `test` | 161 | preserve |
| `token` | 108 | review |
| `tower_lsp` | 1 | review |
| `unreachable` | 5 | reject |
| `vec` | 371 | lower_directly |
| `write` | 70 | lower_directly |
| `writeln` | 37 | lower_directly |

## kain-core

### Bang macros

- `assert!` — 91
- `assert_eq!` — 94
- `debug_assert!` — 1
- `eprintln!` — 3
- `format!` — 219
- `json!` — 2
- `matches!` — 39
- `panic!` — 16
- `print!` — 3
- `println!` — 10
- `unreachable!` — 4
- `vec!` — 129
- `write!` — 70

### Attribute macros

- `allow` — 3
- `cfg` — 19
- `default` — 1
- `derive` — 159
- `derive::Clone` — 158
- `derive::Copy` — 22
- `derive::Debug` — 156
- `derive::Default` — 4
- `derive::Deserialize` — 7
- `derive::Eq` — 27
- `derive::Error` — 1
- `derive::Hash` — 5
- `derive::Logos` — 1
- `derive::Ord` — 1
- `derive::PartialEq` — 122
- `derive::PartialOrd` — 1
- `derive::Serialize` — 7
- `error` — 11
- `from` — 1
- `logos` — 1
- `regex` — 10
- `test` — 61
- `token` — 108

## kain-import

### Bang macros

- `assert!` — 88
- `assert_eq!` — 69
- `format!` — 76
- `matches!` — 71
- `panic!` — 69
- `vec!` — 74

### Attribute macros

- `allow` — 1
- `cfg` — 21
- `derive` — 27
- `derive::Clone` — 26
- `derive::Copy` — 10
- `derive::Debug` — 27
- `derive::Default` — 9
- `derive::Deserialize` — 5
- `derive::Eq` — 8
- `derive::Error` — 1
- `derive::Hash` — 1
- `derive::PartialEq` — 8
- `derive::Serialize` — 5
- `error` — 7
- `from` — 1
- `test` — 75

## kain-sys-codegen

### Bang macros

- `addr_of!` — 1
- `assert!` — 29
- `assert_eq!` — 5
- `cfg!` — 3
- `eprintln!` — 16
- `format!` — 664
- `matches!` — 11
- `println!` — 3
- `vec!` — 72

### Attribute macros

- `cfg` — 2
- `derive` — 28
- `derive::Clone` — 28
- `derive::Copy` — 11
- `derive::Debug` — 28
- `derive::Default` — 2
- `derive::Deserialize` — 9
- `derive::Eq` — 7
- `derive::Hash` — 1
- `derive::PartialEq` — 8
- `derive::Serialize` — 9
- `serde` — 3
- `test` — 5

## cli

### Bang macros

- `assert!` — 32
- `assert_eq!` — 16
- `cfg!` — 9
- `env!` — 3
- `eprint!` — 2
- `eprintln!` — 169
- `format!` — 636
- `matches!` — 30
- `option_env!` — 8
- `panic!` — 2
- `println!` — 382
- `unreachable!` — 1
- `vec!` — 96
- `writeln!` — 37

### Attribute macros

- `allow` — 1
- `arg` — 54
- `cfg` — 92
- `command` — 7
- `derive` — 52
- `derive::ClapParser` — 1
- `derive::Clone` — 27
- `derive::Copy` — 3
- `derive::Debug` — 50
- `derive::Default` — 8
- `derive::Deserialize` — 13
- `derive::Eq` — 2
- `derive::Parser` — 1
- `derive::PartialEq` — 2
- `derive::Serialize` — 21
- `derive::Subcommand` — 1
- `derive::clap::Subcommand` — 1
- `serde` — 32
- `test` — 20
- `tower_lsp` — 1
