# KAIN Intelligence Scanner - Quick Start

## What This Does

Scans your $1M+ worth of UE5 plugins and extracts:
- Every class implementation
- Common patterns
- Best practices
- Include dependencies

Output: `kain_intelligence.lance` - embedded in KAIN compiler

## Installation

```bash
pip install sentence-transformers lancedb pyarrow
```

## Usage

1. **Edit plugin directories** in `scripts/python/intelligence_scanner.py`:
```python
plugin_dirs = [
    "D:/UE5Plugins/Marketplace",
    "D:/UE5Plugins/Premium",
    "D:/UE5Plugins/Custom",
]
```

2. **Run scanner**:
```bash
python scripts/python/intelligence_scanner.py
```

3. **Wait** (could take 30min - 2 hours for thousands of plugins)

4. **Output**: `kain_intelligence.lance` (500MB-2GB)

## What Gets Extracted

For each class:
- Full header/source code
- Properties (UPROPERTY)
- Functions (UFUNCTION)
- Includes
- Module dependencies
- Pattern detection (replication, networking, timers, etc.)
- Semantic embedding for similarity search

## Next Steps

Once you have `kain_intelligence.lance`:

1. **Test it**:
```python
import lancedb
db = lancedb.connect('kain_intelligence.lance')
classes = db.open_table('class_implementations')
print(f"Total classes: {len(classes)}")
```

2. **Embed in KAIN**:
- Copy to `kain/crates/ue5/`
- Build script will embed it
- Compiler loads from memory

3. **Use it**:
- Compiler queries during codegen
- Finds similar patterns
- Generates better code

## Performance

- **Scanning**: ~100 files/second
- **Database size**: ~1KB per class
- **Query time**: <1ms (indexed)
- **Embedding**: Included in binary

## Troubleshooting

**"No classes found"**
- Check plugin directories exist
- Make sure they have `Source/` folders
- Look for `.h` and `.cpp` files

**"Out of memory"**
- Process in batches
- Reduce `header_code` size limit
- Use more RAM (16GB+ recommended)

**"Encoding errors"**
- Normal - some files have weird encoding
- Scanner handles gracefully with `errors='ignore'`

## Example Output

```
📂 Scanning D:/UE5Plugins/Marketplace...
  🔍 AdvancedLocomotionSystem... 47 classes
  🔍 InventorySystem... 23 classes
  🔍 MultiplayerSessions... 12 classes
  ...

🔍 Extracting common patterns...

💾 Saving to database...
✅ Saved 10,247 classes
✅ Saved 234 patterns

📊 SCANNING COMPLETE
============================================================
Plugins scanned:    847
Files processed:    23,456
Classes extracted:  10,247
Patterns found:     234
Errors encountered: 89
============================================================

✅ Database saved to: kain_intelligence.lance
📊 Size: 1,234.5 MB

🎉 Ready to embed in KAIN compiler!
```

## Advanced Usage

### Scan specific plugin:
```python
scanner = PluginScanner()
classes = scanner.scan_plugin(Path("D:/MyPlugin"))
```

### Query database:
```python
import lancedb
db = lancedb.connect('kain_intelligence.lance')

# Find all character classes
classes = db.open_table('class_implementations')
results = classes.search("character movement inventory").limit(10).to_list()

for cls in results:
    print(f"{cls['class_name']} from {cls['source_plugin']}")
```

### Extract specific patterns:
```python
patterns = db.open_table('common_patterns')
character_patterns = patterns.search("ACharacter").limit(5).to_list()
```

## Tips

- **Start small**: Test on 1-2 plugins first
- **Check output**: Verify database has data before embedding
- **Monitor RAM**: Large scans need 8-16GB
- **Be patient**: Thousands of plugins = hours of scanning
- **Incremental**: Can scan in batches and merge databases

## What's Next

See `docs/CODE_INTELLIGENCE_DATABASE.md` for:
- Full architecture
- Integration with KAIN
- Query examples
- Embedding process
