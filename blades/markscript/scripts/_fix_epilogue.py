#!/usr/bin/env python3
with open('src/jit.kn', 'rb') as f:
    content = f.read()

old = b'fn emit_epilogue(arr: Array<Int>) -> Array<Int>:\n    emit_rr(arr, X64_REX_W, 0x89, 4, 5)  // mov rsp, rbp  (discard operand stack)\n    push(arr, X64_POP_RBX)               // restore RBX\n    push(arr, X64_POP_RBP)               // restore RBP\n    push(arr, X64_RET)                   // return (result in RAX)\n    return arr'

new = b'fn emit_epilogue(arr: Array<Int>) -> Array<Int>:\n    // mov rsp, rbp - discard operand stack by restoring RSP to frame base\n    // Encoding: REX.W 89 /r where reg=RBP(source), rm=RSP(dest) = 48 89 EC\n    push(arr, X64_REX_W)\n    push(arr, 0x89)\n    push(arr, 0xEC)  // ModRM: mod=11, reg=5(RBP), rm=4(RSP)\n    push(arr, X64_POP_RBX)               // restore RBX\n    push(arr, X64_POP_RBP)               // restore RBP\n    push(arr, X64_RET)                   // return (result in RAX)\n    return arr'

if old in content:
    content = content.replace(old, new, 1)
    with open('src/jit.kn', 'wb') as f:
        f.write(content)
    print('OK: replaced')
else:
    print('FAIL: old text not found')
    print('Looking for bytes:', old[:50])
