#!/usr/bin/env python3
"""Apply all JIT fixes at once to avoid CRLF matching issues."""
with open('src/jit.kn', 'rb') as f:
    content = f.read()

changes = 0

# Fix 1: emit_epilogue - swap from emit_rr (mov rbp,rsp) to push encoding (mov rsp,rbp)
old1 = b'fn emit_epilogue(arr: Array<Int>) -> Array<Int>:\n    emit_rr(arr, X64_REX_W, 0x89, 4, 5)  // mov rsp, rbp  (discard operand stack)\n    push(arr, X64_POP_RBX)               // restore RBX\n    push(arr, X64_POP_RBP)               // restore RBP\n    push(arr, X64_RET)                   // return (result in RAX)\n    return arr'
new1 = b'fn emit_epilogue(arr: Array<Int>) -> Array<Int>:\n    // mov rsp, rbp - discard operand stack by restoring RSP to frame base\n    // Encoding: REX.W 89 /r where reg=RBP(source), rm=RSP(dest) = 48 89 EC\n    push(arr, X64_REX_W)\n    push(arr, 0x89)\n    push(arr, 0xEC)  // ModRM: mod=11, reg=5(RBP), rm=4(RSP)\n    push(arr, X64_POP_RBX)               // restore RBX\n    push(arr, X64_POP_RBP)               // restore RBP\n    push(arr, X64_RET)                   // return (result in RAX)\n    return arr'
if old1 in content:
    content = content.replace(old1, new1, 1)
    changes += 1
    print('Fix 1: emit_epilogue corrected')

# Fix 2: emit_add_rbp - swap reg/rm so result goes to RAX not RBX
old2 = b'fn emit_add_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    // Pop b into RBX, pop a into RAX\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)  // mov rbx, [top-1]\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false) // mov rax, [top-2]\n    emit_rr(arr, X64_REX_W, 0x01, 0, 3)                    // add rax, rbx\n    // Store result at [top-2] position (new top after 2 pops)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)   // mov [result_slot], rax\n    return arr'
new2 = b'fn emit_add_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    // Pop b into RBX, pop a into RAX\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)  // mov rbx, [top-1]\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false) // mov rax, [top-2]\n    // ADD r/m64, r64: reg=source(r64), rm=dest(r/m64)\n    // Want: RAX += RBX -> dest=RAX(rm=0), src=RBX(reg=3) -> ModRM(3,3,0)=0xD8\n    push(arr, X64_REX_W)\n    push(arr, 0x01)\n    push(arr, 0xD8)  // ModRM: mod=11, reg=3(RBX), rm=0(RAX) = ADD RAX, RBX\n    // Store result at [top-2] position (new top after 2 pops)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)   // mov [result_slot], rax\n    return arr'
if old2 in content:
    content = content.replace(old2, new2, 1)
    changes += 1
    print('Fix 2: emit_add_rbp corrected')

# Fix 3: emit_sub_rbp - same reg/rm swap
old3 = b'fn emit_sub_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false)\n    emit_rr(arr, X64_REX_W, 0x29, 0, 3)                    // sub rax, rbx\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)\n    return arr'
new3 = b'fn emit_sub_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_mov_rbp_disp(arr, 3, -8 - (rsp_off - 8), false)\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), false)\n    // SUB r/m64, r64: reg=source(r64), rm=dest(r/m64)\n    // Want: RAX -= RBX -> dest=RAX(rm=0), src=RBX(reg=3) -> ModRM(3,3,0)=0xD8\n    push(arr, X64_REX_W)\n    push(arr, 0x29)\n    push(arr, 0xD8)  // ModRM: mod=11, reg=3(RBX), rm=0(RAX) = SUB RAX, RBX\n    emit_mov_rbp_disp(arr, 0, -8 - (rsp_off - 16), true)\n    return arr'
if old3 in content:
    content = content.replace(old3, new3, 1)
    changes += 1
    print('Fix 3: emit_sub_rbp corrected')

# Fix 4: FixupEntry field name is_jmp -> kind
old4 = b'push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, is_jmp: true })'
new4 = b'push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 0 })'
count4 = content.count(old4)
if count4 > 0:
    content = content.replace(old4, new4, count4)
    changes += 1
    print(f'Fix 4: is_jmp=true -> kind=0 ({count4} occurrences)')

old4b = b'push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, is_jmp: false })'
count4b = content.count(old4b)
if count4b > 0:
    new4b = new4.replace(b'kind: 0', b'kind: 1')
    content = content.replace(old4b, new4b, count4b)
    changes += 1
    print(f'Fix 4b: is_jmp=false -> kind=1 ({count4b} occurrences)')

# Fix 5: apply_fixups - use patch_at + 4 for both JMP and Jcc
old5 = b'        // For JMP: RIP after the 5-byte instruction = patch_at + 5\n        // For Jcc: RIP after the 6-byte sequence = patch_at + 6\n        let rip_after = if f.kind == 0: f.patch_at + 5 else: f.patch_at + 6'
new5 = b'        // RIP after instruction = patch_at + 4 for both cases:\n        //   JMP (5 bytes): 0xE9 at patch_at-1, disp at patch_at..patch_at+3, RIP = (patch_at-1)+5 = patch_at+4\n        //   Jcc (6 bytes): 0x0F at patch_at-2, 0x8x at patch_at-1, disp at patch_at..patch_at+3, RIP = (patch_at-2)+6 = patch_at+4\n        let rip_after = f.patch_at + 4'
if old5 in content:
    content = content.replace(old5, new5, 1)
    changes += 1
    print('Fix 5: apply_fixups rip_after corrected')

# Fix 6: emit_jcc_rel32 helper (add after emit_jn_rbp)
old6 = b'fn emit_jn_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_pop_rbp(arr, rsp_off)                          // mov rax, [stack top]\n    emit_rr(arr, X64_REX_W, 0x85, 0, 0)                 // test rax, rax\n    return arr\n\n// ===========================================================================\n//  FIXUP RESOLUTION'
new6 = b'fn emit_jn_rbp(arr: Array<Int>, rsp_off: Int) -> Array<Int>:\n    emit_pop_rbp(arr, rsp_off)                          // mov rax, [stack top]\n    emit_rr(arr, X64_REX_W, 0x85, 0, 0)                 // test rax, rax\n    return arr\n\n// Emit the Jcc opcode + rel32 after the pop+test emitted by emit_jz_rbp/emit_jn_rbp.\n// cc: 0x84 = JZ (je), 0x88 = JS (js)\nfn emit_jcc_rel32(arr: Array<Int>, cc: Int, rel: Int) -> Array<Int>:\n    push(arr, 0x0F)\n    push(arr, cc)\n    emit_u32(arr, rel)\n    return arr\n\n// ===========================================================================\n//  FIXUP RESOLUTION'
if old6 in content:
    content = content.replace(old6, new6, 1)
    changes += 1
    print('Fix 6: emit_jcc_rel32 helper added')

# Fix 7: JZ forward fixup - use emit_jcc_rel32 instead of inline push
old7 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 84\n                    push(code_arr, 0x0F)\n                    push(code_arr, 0x84)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
new7 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 84\n                    code_arr = emit_jcc_rel32(code_arr, 0x84, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
if old7 in content:
    content = content.replace(old7, new7, 1)
    changes += 1
    print('Fix 7: JZ forward fixup uses emit_jcc_rel32')

# Fix 8: JN forward fixup - use emit_jcc_rel32 instead of inline push
old8 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 88\n                    push(code_arr, 0x0F)\n                    push(code_arr, 0x88)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(code_arr, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
new8 = b'                    let patch_start = len(code_arr) + 2  // +2 for 0F 88\n                    code_arr = emit_jcc_rel32(code_arr, 0x88, 0)\n                    push(fixups, FixupEntry { patch_at: patch_start, target_ip: target, kind: 1 })'
if old8 in content:
    content = content.replace(old8, new8, 1)
    changes += 1
    print('Fix 8: JN forward fixup uses emit_jcc_rel32')

# Fix 9: jit_selftest - add defensive null-checks, replace with helper-based approach
old9 = b'pub fn jit_selftest() -> Int with Unsafe:\n    // Test 0: just halt\n    let test0: Array<Int> = [0]\n    let r0 = jit_compile_block(test0, 0, 1)\n    let res0 = call_jit(r0.code_ptr)\n    println("[JIT] halt: " + str(res0) + " (" + str(r0.code_size) + " bytes)")\n    if res0 != 0:\n        return 1\n\n    // Test 1: push 42, halt\n    let test1: Array<Int> = [7, 42, 0]\n    let r1 = jit_compile_block(test1, 0, 3)\n    let res1 = call_jit(r1.code_ptr)\n    println("[JIT] push 42: " + str(res1) + " (" + str(r1.code_size) + " bytes)")\n    if res1 != 42:\n        return 1\n\n    // Test 2: 1+2=3\n    let test2: Array<Int> = [7, 1, 7, 2, 14, 0]\n    let r2 = jit_compile_block(test2, 0, 6)\n    let res2 = call_jit(r2.code_ptr)\n    println("[JIT] 1+2: " + str(res2) + " (" + str(r2.code_size) + " bytes)")\n    if res2 != 3:\n        println("[JIT] FAIL: 1+2")\n        return 1\n\n    // Test 3: 100-30=70\n    let test3: Array<Int> = [7, 100, 7, 30, 15, 0]\n    let r3 = jit_compile_block(test3, 0, 6)\n    let res3 = call_jit(r3.code_ptr)\n    println("[JIT] 100-30: " + str(res3) + " (" + str(r3.code_size) + " bytes)")\n    if res3 != 70:\n        println("[JIT] FAIL: 100-30")\n        return 1\n\n    // Test 4: 7*6=42\n    let test4: Array<Int> = [7, 7, 7, 6, 16, 0]\n    let r4 = jit_compile_block(test4, 0, 6)\n    let res4 = call_jit(r4.code_ptr)\n    println("[JIT] 7*6: " + str(res4) + " (" + str(r4.code_size) + " bytes)")\n    if res4 != 42:\n        println("[JIT] FAIL: 7*6")\n        return 1\n\n    // Test 5: 100/5=20\n    let test5: Array<Int> = [7, 100, 7, 5, 17, 0]\n    let r5 = jit_compile_block(test5, 0, 6)\n    let res5 = call_jit(r5.code_ptr)\n    println("[JIT] 100/5: " + str(res5) + " (" + str(r5.code_size) + " bytes)")\n    if res5 != 20:\n        println("[JIT] FAIL: 100/5")\n        return 1\n\n    // Test 6: dup + pop\n    let test6: Array<Int> = [7, 42, 9, 8, 0]\n    let r6 = jit_compile_block(test6, 0, 5)\n    let res6 = call_jit(r6.code_ptr)\n    println("[JIT] dup+pop: " + str(res6) + " (" + str(r6.code_size) + " bytes)")\n    if res6 != 42:\n        println("[JIT] FAIL: dup")\n        return 1\n\n    // Test 7: LOAD_VAR + STORE_VAR\n    let test7: Array<Int> = [7, 100, 18, 1, 19, 2, 0]\n    let r7 = jit_compile_block(test7, 0, 7)\n    let res7 = call_jit(r7.code_ptr)\n    println("[JIT] load/store var: " + str(res7) + " (" + str(r7.code_size) + " bytes)")\n    if res7 != 100:\n        println("[JIT] FAIL: var")\n        return 1\n\n    // Test 8: EXECUTE_CALL skip\n    let test8: Array<Int> = [7, 42, 3, 12345, 4, 0]\n    let r8 = jit_compile_block(test8, 0, 6)\n    let res8 = call_jit(r8.code_ptr)\n    println("[JIT] exec_call skip: " + str(res8) + " (" + str(r8.code_size) + " bytes)")\n    if res8 != 42:\n        println("[JIT] FAIL: exec_call")\n        return 1\n\n    // Test 9: OP_CALL skip\n    let test9: Array<Int> = [7, 77, 10, 0]\n    let r9 = jit_compile_block(test9, 0, 4)\n    let res9 = call_jit(r9.code_ptr)\n    println("[JIT] op_call skip: " + str(res9) + " (" + str(r9.code_size) + " bytes)")\n    if res9 != 77:\n        println("[JIT] FAIL: op_call")\n        return 1\n\n    // Test 10: OP_RET skip\n    let test10: Array<Int> = [7, 55, 11, 0]\n    let r10 = jit_compile_block(test10, 0, 4)\n    let res10 = call_jit(r10.code_ptr)\n    println("[JIT] ret skip: " + str(res10) + " (" + str(r10.code_size) + " bytes)")\n    if res10 != 55:\n        println("[JIT] FAIL: ret")\n        return 1\n\n    // Test 11: ENTER_DOMAIN + ROUTINE_HEADER skip\n    let test11: Array<Int> = [1, 7777, 2, 8888, 7, 33, 0]\n    let r11 = jit_compile_block(test11, 0, 7)\n    let res11 = call_jit(r11.code_ptr)\n    println("[JIT] domain+header: " + str(res11) + " (" + str(r11.code_size) + " bytes)")\n    if res11 != 33:\n        println("[JIT] FAIL: domain")\n        return 1\n\n    // Test 12: FENCED_CODE skip\n    let test12: Array<Int> = [6, 100, 200, 0]\n    let r12 = jit_compile_block(test12, 0, 4)\n    let res12 = call_jit(r12.code_ptr)\n    println("[JIT] fenced code: " + str(res12) + " (" + str(r12.code_size) + " bytes)")\n\n    // Test 13: PUSH_MATRIX skip + push 42\n    let test13: Array<Int> = [5, 0, 2, 2, 4, 1, 0, 0, 0, 1, 0, 7, 42, 0]\n    let r13 = jit_compile_block(test13, 0, 14)\n    let res13 = call_jit(r13.code_ptr)\n    println("[JIT] matrix skip+push: " + str(res13) + " (" + str(r13.code_size) + " bytes)")\n    if res13 != 42:\n        println("[JIT] FAIL: PUSH_MATRIX")\n        return 1\n\n    // Test 14: JMP forward skip\n    let test14: Array<Int> = [7, 99, 12, 5, 7, 1, 0]\n    let r14 = jit_compile_block(test14, 0, 7)\n    let res14 = call_jit(r14.code_ptr)\n    println("[JIT] jmp forward: " + str(res14) + " (" + str(r14.code_size) + " bytes)")\n    if res14 != 99:\n        println("[JIT] FAIL: jmp")\n        return 1\n\n    // Test 15: JZ skip on non-zero\n    let test15: Array<Int> = [7, 5, 13, 5, 7, 88, 0]\n    let r15 = jit_compile_block(test15, 0, 7)\n    let res15 = call_jit(r15.code_ptr)\n    println("[JIT] jz non-zero: " + str(res15) + " (" + str(r15.code_size) + " bytes)")\n    if res15 != 88:\n        println("[JIT] FAIL: jz")\n        return 1\n\n    // Test 16: JN skip on positive\n    let test16: Array<Int> = [7, 5, 20, 5, 7, 77, 0]\n    let r16 = jit_compile_block(test16, 0, 7)\n    let res16 = call_jit(r16.code_ptr)\n    println("[JIT] jn positive: " + str(res16) + " (" + str(r16.code_size) + " bytes)")\n    if res16 != 77:\n        println("[JIT] FAIL: jn")\n        return 1\n\n    // Test 17: Store then load same variable\n    let test17: Array<Int> = [7, 123, 19, 0, 18, 0, 8, 0]\n    let r17 = jit_compile_block(test17, 0, 8)\n    let res17 = call_jit(r17.code_ptr)\n    println("[JIT] store+load var: " + str(res17) + " (" + str(r17.code_size) + " bytes)")\n    if res17 != 123:\n        println("[JIT] FAIL: store+load var")\n        return 1\n\n    println("[JIT] All self-tests passed!")\n    return 0'
new9 = b'fn run_jit_test(label: String, bc: Array<Int>, expected: Int) -> Int with Unsafe:\n    let r = jit_compile_block(bc, 0, len(bc))\n    if r.code_size < 1:\n        println("[JIT] FAIL " + label + ": compile failed - " + r.error)\n        return 1\n    if ptr_to_int(r.code_ptr) == 0:\n        println("[JIT] FAIL " + label + ": null code_ptr")\n        return 1\n    println("[JIT] " + label + ": " + str(len(bc)) + " bc ops, " + str(r.code_size) + " bytes")\n    let res = call_jit(r.code_ptr)\n    if res != expected:\n        println("[JIT] FAIL " + label + ": got " + str(res) + ", expected " + str(expected))\n        return 1\n    return 0\n\npub fn jit_selftest() -> Int with Unsafe:\n    println("[JIT] Running self-tests...")\n\n    // Test 0: just halt\n    if run_jit_test("halt", [0], 0) != 0:\n        return 1\n\n    // Test 1: push 42, halt\n    if run_jit_test("push 42", [7, 42, 0], 42) != 0:\n        return 1\n\n    // Test 2: 1+2=3\n    if run_jit_test("1+2", [7, 1, 7, 2, 14, 0], 3) != 0:\n        return 1\n\n    // Test 3: 100-30=70\n    if run_jit_test("100-30", [7, 100, 7, 30, 15, 0], 70) != 0:\n        return 1\n\n    // Test 4: 7*6=42\n    if run_jit_test("7*6", [7, 7, 7, 6, 16, 0], 42) != 0:\n        return 1\n\n    // Test 5: 100/5=20\n    if run_jit_test("100/5", [7, 100, 7, 5, 17, 0], 20) != 0:\n        return 1\n\n    // Test 6: dup + pop\n    if run_jit_test("dup+pop", [7, 42, 9, 8, 0], 42) != 0:\n        return 1\n\n    // Test 7: LOAD_VAR + STORE_VAR\n    if run_jit_test("load/store var", [7, 100, 18, 1, 19, 2, 0], 100) != 0:\n        return 1\n\n    // Test 8: EXECUTE_CALL skip\n    if run_jit_test("exec_call skip", [7, 42, 3, 12345, 4, 0], 42) != 0:\n        return 1\n\n    // Test 9: OP_CALL skip\n    if run_jit_test("op_call skip", [7, 77, 10, 0], 77) != 0:\n        return 1\n\n    // Test 10: OP_RET skip\n    if run_jit_test("ret skip", [7, 55, 11, 0], 55) != 0:\n        return 1\n\n    // Test 11: ENTER_DOMAIN + ROUTINE_HEADER skip\n    if run_jit_test("domain+header", [1, 7777, 2, 8888, 7, 33, 0], 33) != 0:\n        return 1\n\n    // Test 12: FENCED_CODE skip\n    if run_jit_test("fenced code", [6, 100, 200, 0], 0) != 0:\n        return 1\n\n    // Test 13: PUSH_MATRIX skip + push 42\n    if run_jit_test("matrix skip+push", [5, 0, 2, 2, 4, 1, 0, 0, 0, 1, 0, 7, 42, 0], 42) != 0:\n        return 1\n\n    // Test 14: JMP forward skip\n    if run_jit_test("jmp forward", [7, 99, 12, 5, 7, 1, 0], 99) != 0:\n        return 1\n\n    // Test 15: JZ skip on non-zero\n    if run_jit_test("jz non-zero", [7, 5, 13, 5, 7, 88, 0], 88) != 0:\n        return 1\n\n    // Test 16: JN skip on positive\n    if run_jit_test("jn positive", [7, 5, 20, 5, 7, 77, 0], 77) != 0:\n        return 1\n\n    // Test 17: Store then load same variable\n    if run_jit_test("store+load var", [7, 123, 19, 0, 18, 0, 8, 0], 123) != 0:\n        return 1\n\n    println("[JIT] All self-tests passed!")\n    return 0'
if old9 in content:
    content = content.replace(old9, new9, 1)
    changes += 1
    print('Fix 9: jit_selftest rewritten with run_jit_test helper')

if changes == 0:
    print('No fixes applied - patterns not found')
else:
    with open('src/jit.kn', 'wb') as f:
        f.write(content)
    print(f'All {changes} fix(es) applied, {len(content)} bytes written')
