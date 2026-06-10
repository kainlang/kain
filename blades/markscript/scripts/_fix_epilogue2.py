#!/usr/bin/env python3
with open('src/jit.kn', 'rb') as f:
    content = f.read()

old = b'\nfn emit_epilogue'
# Find where the old function was (after the blank lines following emit_prologue)
# Look for the comment about OPERAND STACK OPS
marker = b'//  OPERAND STACK OPS (RBP-relative, no native push/pop)'
idx = content.find(marker)
if idx < 0:
    print('FAIL: marker not found')
    exit(1)

# Go backwards past all the blank lines (lines with just \r\n)
pos = idx
while pos > 0 and (content[pos-2:pos] == b'\r\n' or content[pos-1:pos] == b'\n'):
    if content[pos-2:pos] == b'\r\n':
        pos -= 2
    else:
        pos -= 1

# Now pos points to the last blank line before the marker
# Replace everything from emit_prologue return to the marker
prologue_end = content.find(b'    return arr\n\n', 0, idx)
if prologue_end < 0:
    prologue_end = content.find(b'    return arr\r\n\r\n', 0, idx)
    
if prologue_end < 0:
    print('FAIL: prologue end not found')
    exit(1)

# Find the end of the blank area
prologue_end += len(b'    return arr\n')  # past the return arr\n

replacement = b'''fn emit_epilogue(arr: Array<Int>) -> Array<Int>:
    // mov rsp, rbp - discard operand stack by restoring RSP to frame base
    // Encoding: REX.W 89 /r where reg=RBP(source), rm=RSP(dest) = 48 89 EC
    push(arr, X64_REX_W)
    push(arr, 0x89)
    push(arr, 0xEC)  // ModRM: mod=11, reg=5(RBP), rm=4(RSP)
    push(arr, X64_POP_RBX)               // restore RBX
    push(arr, X64_POP_RBP)               // restore RBP
    push(arr, X64_RET)                   // return (result in RAX)
    return arr

'''

new_content = content[:prologue_end] + replacement + content[idx:]
with open('src/jit.kn', 'wb') as f:
    f.write(new_content)
print('OK: epilogue restored with fix')
print('Size:', len(new_content), 'bytes')
