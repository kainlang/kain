# KAIN Corpus Library

**Generated:** 2026-02-13 23:04:00  
**Total Files:** 88  
**Total Lines:** 13,663

## Purpose

This library contains deduplicated, high-quality KAIN source files collected from the entire repository. It serves as:

1. **Training Data** - For future LLM fine-tuning on KAIN syntax
2. **Pattern Library** - For semantic analysis and code generation
3. **Example Repository** - For documentation and learning
4. **Corpus Analysis** - For language feature usage statistics

## Structure

```
kn_library/
├── corpus_index.json       # Metadata database (searchable)
├── actors/                 # Actor definitions with networking
├── shaders/                # Shader code (fragment, compute, etc.)
├── editor/                 # Editor tools (Slate, Details, etc.)
├── components/             # Reusable component definitions
├── datatables/             # DataTable structs
├── utilities/              # Blueprint functions and helpers
└── examples/               # Miscellaneous examples
```

## Statistics

| Category | Files | Description |
|----------|-------|-------------|
| utilities | 33 | KAIN utilities code |
| shaders | 29 | KAIN shaders code |
| actors | 10 | KAIN actors code |
| editor | 7 | KAIN editor code |
| components | 5 | KAIN components code |
| datatables | 4 | KAIN datatables code |

## Quality Filters

- **Minimum Lines:** 10 lines of actual code
- **Deduplication:** Content-based (SHA256 hash)
- **Encoding:** UTF-8 with error handling

## Usage

### Search by Feature
```python
import json

with open('corpus_index.json') as f:
    index = json.load(f)

# Find all files with networking
networking_files = [
    f for f in index['files'] 
    if 'networking' in f['features']
]
```

### Search by Category
```python
# Find all shader files
shader_files = [
    f for f in index['files']
    if f['category'] == 'shaders'
]
```

### Get File Content
```python
# Read a specific file
file_info = index['files'][0]
with open(file_info['library_path']) as f:
    content = f.read()
```

## Maintenance

To update the corpus:
```bash
python collect_kain_corpus.py
```

To see statistics only:
```bash
python collect_kain_corpus.py --stats-only
```

## Future Enhancements

- [ ] Semantic search via embeddings
- [ ] AST-based similarity detection
- [ ] Feature extraction for ML training
- [ ] Code complexity metrics
- [ ] Dependency graph analysis
- [ ] Pattern mining for common idioms
