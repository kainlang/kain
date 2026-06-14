#!/usr/bin/env python3
"""Fix remaining escape sequences in pi-squared source files."""
import os

BS_BACKSLASH = bytes([0x5C])  # single backslash
BS_N = bytes([0x5C, 0x6E])    # \n
BS_T = bytes([0x5C, 0x74])    # \t
BS_Q = bytes([0x5C, 0x22])    # \"

def fix_file(path, patterns):
    """Apply byte-level replacements to a file."""
    with open(path, 'rb') as f:
        data = f.read()
    changed = False
    for old, new in patterns:
        if old in data:
            data = data.replace(old, new)
            changed = True
            print(f"  Fixed: {old[:60]}")
    if changed:
        with open(path, 'wb') as f:
            f.write(data)
        return True
    return False

# 1. providers/sse.kn
fix_file('src/providers/sse.kn', [
    (b'        result = result + "' + BS_N + b'" + lines[i]',
     b'        let NL = text_chr(10)\n        result = result + NL + lines[i]'),
])

# 2. session/tree.kn  
fix_file('src/session/tree.kn', [
    (b'    push(msg.content, ContentBlock::TextBlock("[Compaction Summary]"' + BS_N + b' + summary))',
     b'    let NL = text_chr(10)\n    push(msg.content, ContentBlock::TextBlock("[Compaction Summary]" + NL + summary))'),
])

# 3. tui/components/diff.kn
fix_file('src/tui/components/diff.kn', [
    (b'        result = result + "' + BS_N + b'"',
     b'        let NL = text_chr(10)\n        result = result + NL'),
])

# 4. tui/components/editor.kn
fix_file('src/tui/components/editor.kn', [
    (b'        return fmt_join_strings(lines_arr, "' + BS_N + b'")',
     b'        return fmt_join_strings(lines_arr, text_chr(10))'),
    (b'            result = result + "' + BS_N + b'" + display_line',
     b'            let NL = text_chr(10)\n            result = result + NL + display_line'),
    (b'            result = result + "' + BS_N + b'"\n            let status2',
     b'            let NL2 = text_chr(10)\n            result = result + NL2\n            let status2'),
    (b'        let status = "' + BS_N + b'row ',
     b'        let NL = text_chr(10)\n        let status = NL + "row '),
])

# 5. tui/components/markdown.kn
fix_file('src/tui/components/markdown.kn', [
    # \n patterns in string concatenation
    (b'            result = result + f_str + "' + BS_N + b'"',
     b'            let NL = text_chr(10)\n            result = result + f_str + NL'),
    (b'            result = result + f_end + "' + BS_N + b'"',
     b'            let NL2 = text_chr(10)\n            result = result + f_end + NL2'),
    (b'                result = result + fmt_pad_right(code_str, effective_w, " ") + "' + BS_N + b'"',
     b'                let NL3 = text_chr(10)\n                result = result + fmt_pad_right(code_str, effective_w, " ") + NL3'),
    (b'                result = result + fmt_repeat(" ", effective_w) + "' + BS_N + b'"',
     b'                let NL4 = text_chr(10)\n                result = result + fmt_repeat(" ", effective_w) + NL4'),
    (b'                result = result + fmt_pad_right(hdr, effective_w, " ") + "' + BS_N + b'"',
     b'                let NL5 = text_chr(10)\n                result = result + fmt_pad_right(hdr, effective_w, " ") + NL5'),
    (b'                result = result + table_line + "' + BS_N + b'"',
     b'                let NL6 = text_chr(10)\n                result = result + table_line + NL6'),
    (b'            result = result + fmt_pad_right(trimmed, effective_w, " ") + "' + BS_N + b'"',
     b'            let NL7 = text_chr(10)\n            result = result + fmt_pad_right(trimmed, effective_w, " ") + NL7'),
    # \t patterns
    (b'            if ch != " " and ch != "' + BS_T + b'":',
     b'            if ch != " " and ch != text_chr(9):'),
])

# 6. tui/components/select_list.kn
fix_file('src/tui/components/select_list.kn', [
    (b'            result = result + "filter: " + _self.filter + "' + BS_N + b'"',
     b'            let NL = text_chr(10)\n            result = result + "filter: " + _self.filter + NL'),
    (b'                result = result + "' + BS_N + b'"\n                filtered_count',
     b'                let NL2 = text_chr(10)\n                result = result + NL2\n                filtered_count'),
    (b'            result = result + "' + BS_N + b'"\n            let line_count',
     b'            let NL3 = text_chr(10)\n            result = result + NL3\n            let line_count'),
    (b'        result = result + "' + BS_N + b'" + str(len(filtered))',
     b'        let NL4 = text_chr(10)\n        result = result + NL4 + str(len(filtered))'),
])

# 7. tui/components/spacer.kn
fix_file('src/tui/components/spacer.kn', [
    (b'                result = result + "' + BS_N + b'"',
     b'                let NL = text_chr(10)\n                result = result + NL'),
])

# 8. tui/components/text.kn
fix_file('src/tui/components/text.kn', [
    (b'if ch == " " or ch == "' + BS_T + b'" or ch == "' + BS_N + b'" or ch == "\\r":',
     b'if ch == " " or ch == text_chr(9) or ch == text_chr(10) or ch == "\\r":'),
    (b'                result = result + "' + BS_N + b'"',
     b'                let NL = text_chr(10)\n                result = result + NL'),
])

# 9. tui/interactive/assistant.kn
fix_file('src/tui/interactive/assistant.kn', [
    (b'            out_val = out_val + "' + BS_N + b'"',
     b'            let NL = text_chr(10)\n            out_val = out_val + NL'),
])

print("\nAll fixes applied!")
