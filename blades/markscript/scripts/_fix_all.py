#!/usr/bin/env python3
"""Apply all JIT fixes, handling CRLF properly."""
with open('src/jit.kn', 'rb') as f:
    content = f.read()

# Normalize to LF for processing
was_crlf = b'\r\n' in content
if was_crlf:
    content = content.replace(b'\r\n', b'\n')

changes = 0

# Fix 1: emit_epilogue
old1 = b'fn emit_epilogue(arr: Array<Int>) -> Array<Int>:\n    emit_rr(arr, X64_REX_W, 0x89, 4, 5)  // mov rsp, rbp  (discard operand stack)\n    push(arr, X64_POP_RBX)               // restore RBX\n    push(arr, X64_POP_RBP)               // restore RBP\n    push(arr, X64_RET)                   // return (result in RAX)\n    return arr'
new1 = b'fn emit_epilogue(arr: Array<Int>) -> Array<Int>:\n    // mov rsp, rbp - discard operand stack by restoring RSP to frame base\n    // Encoding: REX.W 89 /r where reg=RBP(source), rm=RSP(dest) = 48 89 EC\n    push(arr, X64_REX_W)\n    push(arr, 0x89)\n    push(arr, 0xEC)  // ModRM: mod=11, reg=5(RBP), rm=4(RSP)\n    push(arr, X64_POP_RBX)               // restore RBX\n    push(arr, X64_POP_RBP)               // restore RBP\n    push(arr, X64_RET)                   // return (result in RAX)\n    return arr'
if old1 in content:
    content = content.replace(old1, new1, 1)
    changes += 1
    print('Fix 1: emit_epilogue corrected')
else:
    print('Fix 1 FAILED: emit_epilogue pattern not found')

# Fix 2: emit_add_rbp
old2 = b'fn emit_add_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    // Pop b into RBX, pop a into RAX\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)  // mov rbx, [top-1]\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false) // mov rax, [top-2]\n    emit_rr(arr, X64_REX_W, 0x01, 0, 3)                    // add rax, rbx\n    // Store result at [top-2] position (new top after 2 pops)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)   // mov [result_slot], rax\n    return arr'
new2 = b'fn emit_add_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    // Pop b into RBX, pop a into RAX\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)  // mov rbx, [top-1]\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false) // mov rax, [top-2]\n    // ADD r/m64, r64: reg=source(r64), rm=dest(r/m64)\n    // Want: RAX += RBX -> dest=RAX(rm=0), src=RBX(reg=3) -> ModRM(3,3,0)=0xD8\n    push(arr, X64_REX_W)\n    push(arr, 0x01)\n    push(arr, 0xD8)  // ModRM: mod=11, reg=3(RBX), rm=0(RAX) = ADD RAX, RBX\n    // Store result at [top-2] position (new top after 2 pops)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)   // mov [result_slot], rax\n    return arr'
if old2 in content:
    content = content.replace(old2, new2, 1)
    changes += 1
    print('Fix 2: emit_add_rbp corrected')
else:
    print('Fix 2 FAILED: emit_add_rbp pattern not found')

# Fix 3: emit_sub_rbp
old3 = b'fn emit_sub_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false)\n    emit_rr(arr, X64_REX_W, 0x29, 0, 3)                    // sub rax, rbx\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)\n    return arr'
new3 = b'fn emit_sub_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false)\n    // SUB r/m64, r64: reg=source(r64), rm=dest(r/m64)\n    // Want: RAX -= RBX -> dest=RAX(rm=0), src=RBX(reg=3) -> ModRM(3,3,0)=0xD8\n    push(arr, X64_REX_W)\n    push(arr, 0x29)\n    push(arr, 0xD8)  // ModRM: mod=11, reg=3(RBX), rm=0(RAX) = SUB RAX, RBX\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)\n    return arr'
if old3 in content:
    content = content.replace(old3, new3, 1)
    changes += 1
    print('Fix 3: emit_sub_rbp corrected')
else:
    print('Fix 3 FAILED: emit_sub_rbp pattern not found')

# Fix 4: FixupEntry field name is_jmp -> kind
old4 = b'push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, is_jmp: true })'
new4 = b'push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 0 })'
if old4 in content:
    content = content.replace(old4, new4, 1)
    changes += 1
    print('Fix 4: is_jmp=true -> kind=0')

old4b = b'push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, is_jmp: false })'
new4b = b'push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
if old4b in content:
    n = content.count(old4b)
    content = content.replace(old4b, new4b, n)
    changes += 1
    print(f'Fix 4b: is_jmp=false -> kind=1 ({n} occurrences)')

# Fix 5: apply_fixups rip_after calculation
old5 = b'        // For JMP: RIP after the 5-byte instruction = patch_at + 5\n        // For Jcc: RIP after the 6-byte sequence = patch_at + 6\n        let rip_after = if f.kind == 0: f.patch_at + 5 else: f.patch_at + 6'
new5 = b'        // RIP after instruction = patch_at + 4 for both cases:\n        //   JMP (5 bytes): 0xE9 at patch_at-1, disp at patch_at..patch_at+3, RIP = (patch_at-1)+5 = patch_at+4\n        //   Jcc (6 bytes): 0x0F at patch_at-2, 0x8x at patch_at-1, disp at patch_at..patch_at+3, RIP = (patch_at-2)+6 = patch_at+4\n        let rip_after = f.patch_at + 4'
if old5 in content:
    content = content.replace(old5, new5, 1)
    changes += 1
    print('Fix 5: apply_fixups rip_after corrected')
else:
    print('Fix 5 FAILED: apply_fixups pattern not found')
    # Try to find it with a simpler match
    if b'f.patch_at + 5' in content:
        print('  (but f.patch_at + 5 IS present in file - different surrounding context)')

# Fix 6: Add emit_jcc_rel32 helper after emit_jn_rbp
old6 = b'fn emit_jn_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_pop_rbp(arr, rsp_off)                          // mov rax, [stack top]\n    emit_rr(arr, X64_REX_W, 0x85, 0, 0)                 // test rax, rax\n    return arr\n\n// ===========================================================================\n//  FIXUP RESOLUTION'
new6 = b'fn emit_jn_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_pop_rbp(arr, rsp_off)                          // mov rax, [stack top]\n    emit_rr(arr, X64_REX_W, 0x85, 0, 0)                 // test rax, rax\n    return arr\n\n// Emit the Jcc opcode + rel32 after the pop+test emitted by emit_jz_rbp/emit_jn_rbp.\n// cc: 0x84 = JZ (je), 0x88 = JS (js)\nfn emit_jcc_rel32(arr: Array<Int>, cc: Int, rel: Int) -> Array<Int>:\n    push(arr, 0x0F)\n    push(arr, cc)\n    emit_u32(arr, rel)\n    return arr\n\n// ===========================================================================\n//  FIXUP RESOLUTION'
if old6 in content:
    content = content.replace(old6, new6, 1)
    changes += 1
    print('Fix 6: emit_jcc_rel32 helper added')
else:
    print('Fix 6 FAILED: emit_jn_rbp pattern not found')

# Fix 7 & 8: JZ and JN forward fixups use emit_jcc_rel32
old7 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 84\n                    push(code_arr, 0x0F)\n                    push(code_arr, 0x84)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
new7 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 84\n                    code_arr = emit_jcc_rel32(code_arr, 0x84, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
if old7 in content:
    content = content.replace(old7, new7, 1)
    changes += 1
    print('Fix 7: JZ forward fixup uses emit_jcc_rel32')

old8 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 88\n                    push(code_arr, 0x0F)\n                    push(code_arr, 0x88)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
new8 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 88\n                    code_arr = emit_jcc_rel32(code_arr, 0x88, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
if old8 in content:
    content = content.replace(old8, new8, 1)
    changes += 1
    print('Fix 8: JN forward fixup uses emit_jcc_rel32')

# Fix 9: jit_selftest with null checks
old9_anchor = b'pub fn jit_selftest() -> Int with Unsafe:'
new9 = b'''fn run_jit_test(label: String, bc: Array<Int>, expected: Int) -> Int with Unsafe:
    let r = jit_compile_block(bc, 0, len(bc))
    if r.code_size < 1:
        println("[JIT] FAIL " + label + ": compile failed - " + r.error)
        return 1
    if ptr_to_int(r.code_ptr) == 0:
        println("[JIT] FAIL " + label + ": null code_ptr")
        return 1
    println("[JIT] " + label + ": " + str(len(bc)) + " bc ops, " + str(r.code_size) + " bytes")
    let res = call_jit(r.code_ptr)
    if res != expected:
        println("[JIT] FAIL " + label + ": got " + str(res) + ", expected " + str(expected))
        return 1
    return 0

pub fn jit_selftest() -> Int with Unsafe:
    println("[JIT] Running self-tests...")

    // Test 0: just halt
    if run_jit_test("halt", [0], 0) != 0:
        return 1

    // Test 1: push 42, halt
    if run_jit_test("push 42", [7, 42, 0], 42) != 0:
        return 1

    // Test 2: 1+2=3
    if run_jit_test("1+2", [7, 1, 7, 2, 14, 0], 3) != 0:
        return 1

    // Test 3: 100-30=70
    if run_jit_test("100-30", [7, 100, 7, 30, 15, 0], 70) != 0:
        return 1

    // Test 4: 7*6=42
    if run_jit_test("7*6", [7, 7, 7, 6, 16, 0], 42) != 0:
        return 1

    // Test 5: 100/5=20
    if run_jit_test("100/5", [7, 100, 7, 5, 17, 0], 20) != 0:
        return 1

    // Test 6: dup + pop
    if run_jit_test("dup+pop", [7, 42, 9, 8, 0], 42) != 0:
        return 1

    // Test 7: LOAD_VAR + STORE_VAR
    if run_jit_test("load/store var", [7, 100, 18, 1, 19, 2, 0], 100) != 0:
        return 1

    // Test 8: EXECUTE_CALL skip
    if run_jit_test("exec_call skip", [7, 42, 3, 12345, 4, 0], 42) != 0:
        return 1

    // Test 9: OP_CALL skip
    if run_jit_test("op_call skip", [7, 77, 10, 0], 77) != 0:
        return 1

    // Test 10: OP_RET skip
    if run_jit_test("ret skip", [7, 55, 11, 0], 55) != 0:
        return 1

    // Test 11: ENTER_DOMAIN + ROUTINE_HEADER skip
    if run_jit_test("domain+header", [1, 7777, 2, 8888, 7, 33, 0], 33) != 0:
        return 1

    // Test 12: FENCED_CODE skip
    if run_jit_test("fenced code", [6, 100, 200, 0], 0) != 0:
        return 1

    // Test 13: PUSH_MATRIX skip + push 42
    if run_jit_test("matrix skip+push", [5, 0, 2, 2, 4, 1, 0, 0, 0, 1, 0, 7, 42, 0], 42) != 0:
        return 1

    // Test 14: JMP forward skip
    if run_jit_test("jmp forward", [7, 99, 12, 5, 7, 1, 0], 99) != 0:
        return 1

    // Test 15: JZ skip on non-zero
    if run_jit_test("jz non-zero", [7, 5, 13, 5, 7, 88, 0], 88) != 0:
        return 1

    // Test 16: JN skip on positive
    if run_jit_test("jn positive", [7, 5, 20, 5, 7, 77, 0], 77) != 0:
        return 1

    // Test 17: Store then load same variable
    if run_jit_test("store+load var", [7, 123, 19, 0, 18, 0, 8, 0], 123) != 0:
        return 1

    println("[JIT] All self-tests passed!")
    return 0'''

if old9_anchor in content:
    # Find the start of the selftest function
    idx = content.find(old9_anchor)
    # Find the matching return 0 that ends the function
    # Look for 'return 0' after the function start
    end_markers = [b'\n    return 0\n', b'\n    return 0\r\n']
    end_idx = -1
    for em in end_markers:
        pos = content.find(em, idx)
        if pos > 0:
            end_idx = pos + len(em)
            break
    
    if end_idx > idx:
        content = content[:idx] + new9 + content[end_idx:]
        changes += 1
        print('Fix 9: jit_selftest rewritten with run_jit_test helper')
    else:
        print('Fix 9 FAILED: could not find end of jit_selftest')
else:
    print('Fix 9 FAILED: jit_selftest anchor not found')

# Write back with original line endings
if was_crlf:
    content = content.replace(b'\n', b'\r\n')

with open('src/jit.kn', 'wb') as f:
    f.write(content)
print(f'\nApplied {changes} fix(es), {len(content)} bytes written')
