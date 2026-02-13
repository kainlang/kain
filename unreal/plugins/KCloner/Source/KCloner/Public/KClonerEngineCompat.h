// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerEngineCompat.h
// Engine version compatibility layer for K-Cloner
// Handles API differences between UE 5.4, 5.5, 5.6, and 5.7+

#pragma once

#include "Runtime/Launch/Resources/Version.h"

// =============================================================================
// VERSION DETECTION MACROS
// =============================================================================

// Convenience macros for version checking
#define KCLONER_ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

#define KCLONER_UE_54 (KCLONER_ENGINE_VERSION == 504)
#define KCLONER_UE_55 (KCLONER_ENGINE_VERSION == 505)
#define KCLONER_UE_56 (KCLONER_ENGINE_VERSION == 506)
#define KCLONER_UE_57_OR_LATER (KCLONER_ENGINE_VERSION >= 507)

// Range checks
#define KCLONER_UE_55_OR_LATER (KCLONER_ENGINE_VERSION >= 505)
#define KCLONER_UE_56_OR_LATER (KCLONER_ENGINE_VERSION >= 506)

// =============================================================================
// MESH MERGING SETTINGS INCLUDE
// =============================================================================
// The location of FMeshMergingSettings changed between versions:
// - UE 5.4: Engine/MeshMerging.h
// - UE 5.5+: MeshMerge/MeshMergingSettings.h

#if KCLONER_UE_55_OR_LATER
    #define KCLONER_MESH_MERGING_INCLUDE "MeshMerge/MeshMergingSettings.h"
#else
    #define KCLONER_MESH_MERGING_INCLUDE "Engine/MeshMerging.h"
#endif

// =============================================================================
// TEXTURE SOURCE FORMAT
// =============================================================================
// TSF_RGBA8 was deprecated in 5.5, use TSF_BGRA8 instead
// Both formats exist in all versions, but the check logic may differ

#if KCLONER_UE_55_OR_LATER
    // In 5.5+, TSF_RGBA8 is marked deprecated, just use BGRA8
    #define KCLONER_TEXTURE_FORMAT_RGBA TSF_BGRA8
#else
    // In 5.4, TSF_RGBA8 still exists
    #define KCLONER_TEXTURE_FORMAT_RGBA TSF_RGBA8
#endif

// =============================================================================
// SEQUENCER TRACK EDITOR API
// =============================================================================
// BuildTrackSidebarMenu became pure virtual in 5.5+
// ISequencer::CreateBinding signature changed between versions

#if KCLONER_UE_55_OR_LATER
    #define KCLONER_NEED_BUILD_TRACK_SIDEBAR_MENU 1
#else
    #define KCLONER_NEED_BUILD_TRACK_SIDEBAR_MENU 0
#endif

// CreateBinding API:
// - UE 5.4: CreateBinding(UObject& InObject, const FString& InName)
// - UE 5.7: CreateBinding(UMovieSceneSequence*, AActor*) - different signature entirely
#if KCLONER_UE_57_OR_LATER
    #define KCLONER_CREATE_BINDING_NEW_API 1
#else
    #define KCLONER_CREATE_BINDING_NEW_API 0
#endif

// =============================================================================
// STATIC MESH IMPORT VERSION
// =============================================================================
// SetImportVersion was removed in 5.4, use direct property access

#if KCLONER_UE_57_OR_LATER
    #define KCLONER_SET_STATIC_MESH_IMPORT_VERSION(Mesh) \
        (Mesh)->SetImportVersion(EImportStaticMeshVersion::LastVersion)
#else
    #define KCLONER_SET_STATIC_MESH_IMPORT_VERSION(Mesh) \
        (Mesh)->ImportVersion = EImportStaticMeshVersion::LastVersion
#endif

// =============================================================================
// LOGGING HELPER
// =============================================================================

#define KCLONER_LOG_VERSION() \
    UE_LOG(LogTemp, Log, TEXT("K-Cloner: Compiled for UE %d.%d"), \
        ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION)
