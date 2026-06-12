import os

content = []
content.append("# Document header")
content.append("")
content.append("test content")
with open('blades/kain/research/01-lexer-parser-ast.md', 'w', encoding='utf-8') as f:
    f.write(chr(10).join(content))
print('Script test ok')
