# Auto-Logging Build System

## What It Does

The enhanced FULLBUILD.bat now **automatically logs errors** to `Factory/COMBINEDLOG.md` when builds fail.

## Features

### 1. Smart Error Detection
- Captures errors from both KAIN compilation and UE5 build phases
- Filters for lines containing: `error`, `Error:`, `ERROR`, `failed`, `FAILED`

### 2. Intelligent Truncation
- **Max 50 errors per build** to prevent log bloat
- Shows truncation notice: `[TRUNCATED: X more errors not shown]`
- Includes total error count

### 3. Automatic Formatting
```markdown
-------------
PLUGINNAME - KAIN COMPILATION
-------------
error line 1
error line 2
...

Total errors found: 15

-------------
PLUGINNAME - UE5 BUILD
-------------
error line 1
error line 2
...

[TRUNCATED: 25 more errors not shown]
Total errors found: 75
```

## Usage

### Run a Single Plugin Build
```bash
cd Factory/BulkMatte
FULLBUILD.bat
```

If it fails, errors are automatically appended to `Factory/COMBINEDLOG.md`

### Update All Plugins
```bash
cd Factory/_scripts
update_all_fullbuild.bat
```

This copies the enhanced FULLBUILD.bat to all plugin folders.

## How It Works

1. **Capture Output**: Redirects build output to temp file
2. **Check Result**: If build fails, parse the temp file
3. **Extract Errors**: Filter lines containing error keywords
4. **Truncate**: Keep only first 50 errors
5. **Append**: Add to COMBINEDLOG.md with plugin name header
6. **Cleanup**: Delete temp file

## Configuration

Edit these variables in FULLBUILD.bat:

```bat
set "MAX_LINES=50"          REM Max errors to log per build
set "COMBINED_LOG=%SCRIPT_DIR%\..\COMBINEDLOG.md"  REM Log file location
```

## Benefits

✅ **No manual copy-paste** - Errors logged automatically  
✅ **Clean logs** - Truncation prevents 1000+ line error dumps  
✅ **Context preserved** - Plugin name and error type clearly labeled  
✅ **Zero overhead** - Only logs on failure  
✅ **Centralized** - All plugin errors in one file  

## Example Output

```markdown
-------------
BulkMatte - UE5 BUILD
-------------
M:\Code\Factory\BulkMatte\Source\BulkMatte\Public\EParameterType.h(15): Error: Enum 'EParameterType' shares engine name 'EParameterType' with enum 'EParameterType' in D:\Unreal\UE_5.4\Engine\Plugins\Runtime\Harmonix\Source\HarmonixDsp\Public\HarmonixDsp\Containers\TypedParameter.h(15)

Total errors found: 1
```

## Troubleshooting

**Q: Logs not appearing?**  
A: Check that `Factory/COMBINEDLOG.md` exists and is writable

**Q: Too many/few errors logged?**  
A: Adjust `MAX_LINES` variable in FULLBUILD.bat

**Q: Want to clear the log?**  
A: Delete or truncate `Factory/COMBINEDLOG.md`

## Future Enhancements

- [ ] Color-coded error severity
- [ ] Timestamp for each build
- [ ] Success builds logged with summary
- [ ] Email/Slack notifications on failure
- [ ] HTML report generation
