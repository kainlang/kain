# native_cli_surface

Exercises the boring native utility contract directly in an LLVM-built Kain executable:

- forwarded `args()`
- `cwd()`
- stdout and stderr raw writes
- `read_dir(...)`
- `path_join`, `path_parent`, `path_file_name`, `path_extension`, `path_stem`
- `path_is_file`, `path_is_dir`
- `create_dir_all`, `copy_file`, and `remove_file`

The validation harness runs this fixture with concrete argv entries and checks both exit status and captured output.
