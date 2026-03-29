# PSOEliminator - Quick Start Guide

Get up and running in 5 minutes!

---

## Step 1: Build the Plugin (2 minutes)

### Windows:
```bash
# Double-click Build5.4.bat
# OR run from command line:
Build5.4.bat
```

This will:
- Clean old generated files
- Run KAIN compiler
- Generate C++ source code
- Generate shader files

**Expected Output:**
```
Source/KainPSOEliminator/
Source/KainPSOEliminatorEditor/
Shaders/PSOTestShader.usf
Shaders/PermutationGenerator.usf
```

---

## Step 2: Install in UE5 Project (1 minute)

1. Copy entire `PSOEliminator` folder to your project's `Plugins/` directory:
   ```
   YourProject/
   └── Plugins/
       └── PSOEliminator/  <-- Copy here
   ```

2. Right-click your `.uproject` file → **Generate Visual Studio project files**

3. Open solution in Visual Studio

4. Build solution (Ctrl+Shift+B)

---

## Step 3: Enable Plugin (30 seconds)

1. Launch Unreal Editor
2. Go to **Edit → Plugins**
3. Search for "PSOEliminator"
4. Check the box to enable it
5. Restart editor when prompted

---

## Step 4: Open PSO Cache Builder (30 seconds)

1. Go to **Tools → PSO Cache Builder → Open PSO Eliminator**
2. The PSO Cache Editor window will open

You should see:
- **Dashboard** - Status, progress, statistics
- **Viewport** - Shader preview (rotating mesh)
- **Details Panel** - Settings (thread count, permutations)
- **Toolbar** - Quick actions (scan, compile, export)

---

## Step 5: Build Your First Cache (1 minute)

1. In the **Details Panel**, set:
   - Thread Count: `8` (adjust based on your CPU)
   - Max Permutations: `500`

2. Click **"Start Cache Build"** button

3. Watch the progress bar in the **Dashboard**

4. When complete, you'll see:
   - Status: `Complete`
   - Materials Compiled: `150`
   - Total Time: `~30-60 seconds` (depends on project size)

---

## Step 6: Validate & Export (30 seconds)

1. Click **"Validate Cache"** button
   - Ensures all PSOs are valid
   - Reports any corrupted entries

2. Click **"Export Manifest"** button
   - Saves cache manifest to `Saved/PSOCache/manifest.json`
   - Share this file with your team for consistent builds

---

## Step 7: Test In-Game (1 minute)

1. Package your project (File → Package Project → Windows)

2. Run the packaged game

3. Play for 5-10 minutes, moving through different areas

4. **Result:** Zero shader compilation stutters! 🎉

---

## Troubleshooting

### "Plugin failed to load"
- Ensure you built the plugin in Visual Studio
- Check Output Log for specific errors
- Verify UE5.4+ is installed

### "No materials found during scan"
- Ensure your project has materials in Content Browser
- Check that materials are not in excluded folders
- Enable "Include Engine Content" in scan settings

### "Compilation is slow"
- Increase thread count (up to your CPU core count)
- Reduce max permutations per material
- Disable shader types you don't use (Niagara, Compute, etc.)

### "Cache validation failed"
- Clear cache and rebuild: Click "Clear Cache" → "Start Cache Build"
- Check for corrupted material assets
- Verify shader files are not modified

---

## Next Steps

### For Production:
1. **Automate cache builds** - Add to CI/CD pipeline
2. **Share manifests** - Commit to version control
3. **Test on target hardware** - Validate on min-spec machines
4. **Monitor cache size** - Keep under 2GB for fast loading

### For Teams:
1. **Export manifest** after each material update
2. **Import manifest** on build machines
3. **Validate cache** before shipping
4. **Document settings** in project wiki

### For Advanced Users:
1. **Customize permutations** - Edit `PermutationGenerator.usf`
2. **Add custom shaders** - Create new `.usf` files
3. **Integrate with tools** - Use Blueprint functions in automation
4. **Profile performance** - Use Unreal Insights to verify zero stutters

---

## Support

**Questions?** support@kainresearchlabs.com  
**Discord:** [Link TBD]  
**Documentation:** See `README.md` for full details

---

**Congratulations!** You've eliminated shader stutters from your game. 🚀
