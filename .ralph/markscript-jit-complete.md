Complete the MarkScript JIT with full bytecode coverage:

1. Fix semicolons in jit_selftest (Kain uses newlines, not semicolons)
2. Convert to RBP-relative operand stack (no native push/pop)
3. Add emit_mov_rbp_disp helper
4. Implement all 20 opcodes: LOAD_VAR, STORE_VAR, EXECUTE_CALL, OP_CALL, JMP, JZ, JN with two-pass fixups
5. Add variable storage area at fixed offset below RBP
6. Add jit-run subcommand to CLI
7. Wire cmd_jit_run in main.kn

Checklist:
- [ ] Fix semicolons in jit_selftest
- [ ] Add emit_mov_rbp_disp helper
- [ ] Add emit_mov_rax_imm64 helper (just mov, no push)
- [ ] Rewrite emit_prologue/epilogue for RBP-relative
- [ ] Rewrite emit_push_imm64 → RBP-relative store
- [ ] Rewrite emit_pop_drop → RBP-relative load
- [ ] Rewrite emit_add/sub/mul/div for RBP-relative
- [ ] Rewrite emit_halt → pop top to RAX, then epilogue
- [ ] Implement emit_dup for RBP-relative
- [ ] Add two-pass jump support (native_offsets, fixups)
- [ ] Rewrite emit_jmp/jz/jn for RBP-relative + fixups
- [ ] Implement LOAD_VAR with variable store area
- [ ] Implement STORE_VAR with variable store area
- [ ] Handle EXECUTE_CALL and OP_CALL as skips (not halts)
- [ ] Handle OP_RET as skip
- [ ] Handle ENTER_DOMAIN, ROUTINE_HEADER, PUSH_PARAM as skips
- [ ] Handle PUSH_MATRIX, FENCED_CODE as skips
- [ ] Fix jit_compile_block to thread rsp_offset through cases
- [ ] Add apply_fixups function
- [ ] Add jit-run to cli.kn (SUBCOMMAND_JIT_RUN, is_subcommand, needs_filepath, parse_args, subcommand_name)
- [ ] Add cmd_jit_run to main.kn
- [ ] Run kain check to verify compilation