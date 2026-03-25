# Known Missing Modules

This document lists modules that are referenced in UE5 .Build.cs files but don't have their own .Build.cs files in the scanned Engine/Source directory. These are typically platform-specific, third-party, or plugin modules.

## Missing Modules List

### 1. AndroidPermission
- **Referenced by:** Voice (public_include_path_modules)
- **Type:** Platform-specific
- **Reason:** Android-only module, not present in all UE5 installations
- **Impact:** None - only needed for Android builds
- **Action:** No action needed - platform-specific modules are handled by UE5 build system

### 2. CryptoPP
- **Referenced by:** 8 encryption modules
  - AESBlockEncryptor
  - BlockEncryptionHandlerComponent
  - BlowFishBlockEncryptor
  - RSAEncryptionHandlerComponent
  - RSAKeyAESEncryption
  - StreamEncryptionHandlerComponent
  - TwoFishBlockEncryptor
  - XORBlockEncryptor
- **Type:** Third-party library
- **Reason:** Crypto++ library, may be in ThirdParty directory with different structure
- **Impact:** Low - encryption modules are optional
- **Action:** Consider adding ThirdParty module scanning

### 3. GameplayTagsEditor
- **Referenced by:** GameplayTasks (private_deps)
- **Type:** Editor module
- **Reason:** May be in a plugin or separate editor directory
- **Impact:** Low - editor-only dependency
- **Action:** Verify if this module exists in UE5 5.5+

### 4. OnlineSubsystemFacebook
- **Referenced by:** UnrealGame (dynamic_deps)
- **Type:** Plugin module
- **Reason:** Online subsystem plugin, not part of core engine
- **Impact:** None - dynamically loaded, optional
- **Action:** No action needed - plugin modules are optional

### 5. OnlineSubsystemGooglePlay
- **Referenced by:** UnrealGame (dynamic_deps)
- **Type:** Plugin module
- **Reason:** Online subsystem plugin, not part of core engine
- **Impact:** None - dynamically loaded, optional
- **Action:** No action needed - plugin modules are optional

### 6. OnlineSubsystemIOS
- **Referenced by:** UnrealGame (dynamic_deps)
- **Type:** Plugin module
- **Reason:** Online subsystem plugin, not part of core engine
- **Impact:** None - dynamically loaded, optional
- **Action:** No action needed - plugin modules are optional

### 7. OnlineSubsystemNull
- **Referenced by:** UnrealGame (dynamic_deps)
- **Type:** Plugin module
- **Reason:** Online subsystem plugin, not part of core engine
- **Impact:** None - dynamically loaded, optional
- **Action:** No action needed - plugin modules are optional

### 8. Shaders
- **Referenced by:** 7 rendering modules
  - D3D11RHI (private_include_path_modules)
  - Engine (public_include_path_modules)
  - GeometryFramework (public_include_path_modules)
  - Landscape (private_include_path_modules)
  - RenderCore (private_include_path_modules)
  - Renderer (private_include_path_modules)
  - ShaderFormatD3D (private_include_path_modules)
- **Type:** Deprecated/merged module
- **Reason:** Shader functionality merged into RenderCore in recent UE5 versions
- **Impact:** Low - include path only, not a link dependency
- **Action:** Update references to use RenderCore instead

### 9. TcpMessaging
- **Referenced by:** AndroidDeviceDetection (dynamic_deps, private_include_path_modules)
- **Type:** Messaging module
- **Reason:** May be in a plugin or separate messaging directory
- **Impact:** Low - Android device detection only
- **Action:** Verify if this module exists in Plugins directory

### 10. XCurl
- **Referenced by:** HTTP (public_deps)
- **Type:** Third-party library
- **Reason:** libcurl wrapper, may be in ThirdParty directory with different structure
- **Impact:** Medium - HTTP module is commonly used
- **Action:** Add ThirdParty module scanning, or document as known external dependency

### 11. detex
- **Referenced by:** OpenGLDrv (private_deps)
- **Type:** Third-party library
- **Reason:** Texture decompression library, may be in ThirdParty directory
- **Impact:** Low - OpenGL driver only
- **Action:** Add ThirdParty module scanning

## Impact on KAIN Codegen

### No Impact
- **Dynamic dependencies** (OnlineSubsystem*) - Loaded at runtime, not needed for compilation
- **Platform-specific modules** (AndroidPermission) - Only needed for specific platforms
- **Include path modules** (Shaders) - Only for header includes, not linking

### Low Impact
- **Third-party libraries** (CryptoPP, detex) - Optional functionality
- **Editor modules** (GameplayTagsEditor) - Editor-only features

### Medium Impact
- **XCurl** - HTTP module is commonly used, but KAIN can fall back to detecting HTTP module directly

## Handling in KAIN Codegen

KAIN's module dependency resolver should:

1. **Ignore missing dynamic dependencies** - They're loaded at runtime
2. **Warn about missing public dependencies** - These may cause link errors
3. **Fall back to parent module** - If Shaders is missing, use RenderCore
4. **Document external dependencies** - List XCurl, CryptoPP as external

Example handling:
```rust
fn resolve_module_dependency(&self, dep: &str) -> Option<String> {
    if self.module_graph.modules.contains_key(dep) {
        Some(dep.to_string())
    } else if KNOWN_MISSING_MODULES.contains(dep) {
        // Check if it's a known missing module
        match dep {
            "Shaders" => Some("RenderCore".to_string()),
            "XCurl" => Some("HTTP".to_string()),
            _ => {
                warn!("Module '{}' not found, but is a known missing module", dep);
                None
            }
        }
    } else {
        error!("Unknown module dependency: {}", dep);
        None
    }
}
```

## Recommendations

### Short Term
1. ✅ Document these as known missing modules (this file)
2. ✅ Update KAIN codegen to handle gracefully
3. ⚠️ Add fallback mappings (Shaders → RenderCore, XCurl → HTTP)

### Long Term
1. Scan ThirdParty directory for additional modules
2. Scan Plugins directory for plugin modules
3. Add platform-specific module detection
4. Generate separate module graphs per platform

## Version Differences

These missing modules may vary by UE5 version:

| Module | 5.4 | 5.5 | 5.6 | 5.7 |
|--------|-----|-----|-----|-----|
| Shaders | Missing | ? | ? | ? |
| XCurl | Missing | ? | ? | ? |
| CryptoPP | Missing | ? | ? | ? |
| GameplayTagsEditor | Missing | ? | ? | ? |

**Action:** Run module graph extraction for UE5 5.5, 5.6, 5.7 to verify.

## Conclusion

The 11 missing modules are **acceptable** and don't prevent KAIN from generating correct .Build.cs files. They are:
- Optional dependencies (plugins, third-party)
- Platform-specific (Android, iOS)
- Deprecated (Shaders merged into RenderCore)

KAIN's module dependency resolver can handle these gracefully with fallback logic and warnings.
