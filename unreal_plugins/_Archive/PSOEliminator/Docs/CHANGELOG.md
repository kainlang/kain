# PSOEliminator - Changelog

All notable changes to this project will be documented in this file.

---

## [1.0.0] - 2026-02-19

### 🎉 Initial Release

#### Features
- ✅ **Automated PSO Caching** - One-click shader compilation
- ✅ **Multi-threaded Compilation** - 1-32 configurable threads
- ✅ **Intelligent Permutation Gathering** - Automatic detection of shader variants
- ✅ **Production Dashboard** - Real-time progress tracking with ETA
- ✅ **Shader Preview Viewport** - Live preview of compiling materials
- ✅ **Details Panel** - Configurable settings (thread count, permutations, filters)
- ✅ **Toolbar Integration** - Quick actions (scan, compile, export, import, clear)
- ✅ **Cache Validation** - Integrity checking before shipping
- ✅ **Manifest Export/Import** - Team workflow integration
- ✅ **Blueprint Functions** - Automation-friendly API

#### Shader Types Supported
- ✅ Material shaders
- ✅ Global shaders
- ✅ Niagara shaders
- ✅ Compute shaders
- ✅ Post-process shaders

#### Editor Integration
- ✅ Menu entry: Tools → PSO Cache Builder
- ✅ Toolbar button for quick access
- ✅ Asset editor with viewport + details + toolbar
- ✅ Slate dashboard with progress visualization

#### Technical
- ✅ Built with KAIN compiler (production-ready C++ generation)
- ✅ UE5.4+ compatibility
- ✅ Modular architecture (runtime + editor modules)
- ✅ Data-driven configuration (KAIN.toml + .ini files)
- ✅ Networked replication for distributed builds

#### Documentation
- ✅ Comprehensive README.md
- ✅ Quick Start Guide (5-minute setup)
- ✅ Market positioning analysis
- ✅ Technical architecture overview
- ✅ Troubleshooting guide

---

## [Unreleased] - Roadmap

### Version 1.1 (Q2 2026)
- 🔄 **Incremental Cache Updates** - Only recompile changed materials
- 🔄 **Cloud Cache Sharing** - Team-wide cache distribution via cloud storage
- 🔄 **CI/CD Integration** - Automated cache builds in build pipelines
- 🔄 **Platform-Specific Permutations** - Separate caches for PC/Console/Mobile
- 🔄 **Material Instance Detection** - Track material instance parameters
- 🔄 **Progress Notifications** - Desktop notifications when builds complete

### Version 1.2 (Q3 2026)
- 🔄 **Hot Reload Support** - Recompile without editor restart
- 🔄 **Blueprint Material Parameters** - Track dynamic material parameters
- 🔄 **Automated Regression Testing** - Detect cache corruption automatically
- 🔄 **Performance Profiling** - Built-in profiler for cache hit/miss rates
- 🔄 **Custom Permutation Rules** - User-defined permutation filters
- 🔄 **Batch Processing** - Process multiple projects simultaneously

### Version 2.0 (Q4 2026)
- 🔄 **Runtime PSO Preloading** - Load caches during level streaming
- 🔄 **Adaptive Compilation** - Prioritize visible materials first
- 🔄 **Telemetry Integration** - Track PSO cache effectiveness in shipped games
- 🔄 **Marketplace Integration** - One-click install from Unreal Marketplace
- 🔄 **Machine Learning** - Predict which PSOs to compile based on gameplay
- 🔄 **Multi-Project Management** - Manage caches across multiple projects

---

## Known Issues

### Version 1.0.0
- ⚠️ **Large Projects (1000+ materials)** - May take 2-4 hours to compile all PSOs
  - **Workaround:** Use permutation filters to reduce compile time
- ⚠️ **Niagara Shaders** - Some complex Niagara systems may not be detected
  - **Workaround:** Manually trigger Niagara shader compilation in editor
- ⚠️ **Material Instances** - Dynamic material instances not tracked yet
  - **Workaround:** Compile parent materials, instances will inherit PSOs

---

## Migration Guide

### From Manual PSO Caching
1. Remove manual PSO caching code from your project
2. Install PSOEliminator plugin
3. Run "Scan Project Materials"
4. Click "Start Cache Build"
5. Export manifest for version control

### From Epic's Built-in Tools
1. Disable Epic's PSO gathering in Project Settings
2. Install PSOEliminator plugin
3. Import existing PSO data (if available)
4. Run "Start Cache Build" to rebuild with PSOEliminator

---

## Support

**Bug Reports:** support@kainresearchlabs.com  
**Feature Requests:** Discord [Link TBD]  
**Documentation:** README.md + QUICKSTART.md

---

**Built with KAIN** - The future of UE5 plugin development.
