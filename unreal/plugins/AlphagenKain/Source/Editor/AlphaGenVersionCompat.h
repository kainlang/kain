// Copyright 2026 K-Studio. All Rights Reserved.
// Version compatibility layer for Unreal Engine 5.4 - 5.7+

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

// Engine version detection
#ifndef ENGINE_MAJOR_VERSION
	#define ENGINE_MAJOR_VERSION 5
#endif

#ifndef ENGINE_MINOR_VERSION
	#define ENGINE_MINOR_VERSION 4
#endif

// ============================================================================
// RHI Texture Type Compatibility
// In UE 5.7+, FTexture2DRHIRef and FRHITexture2D were unified to FRHITexture*
// ============================================================================

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	// UE 5.7+ uses FRHITexture* (unified texture type)
	#define ALPHAGEN_TEXTURE2D_RHI_TYPE FRHITexture*
	#define ALPHAGEN_GET_TEXTURE2D_RHI(Resource) ((Resource)->GetTexture2DRHI())
	#define ALPHAGEN_IS_VALID_RHI(RHI) ((RHI) != nullptr)
#else
	// UE 5.4 - 5.6 uses FTexture2DRHIRef (TRefCountPtr)
	#define ALPHAGEN_TEXTURE2D_RHI_TYPE FTexture2DRHIRef
	#define ALPHAGEN_GET_TEXTURE2D_RHI(Resource) ((Resource)->GetTexture2DRHI())
	#define ALPHAGEN_IS_VALID_RHI(RHI) ((RHI).IsValid())
#endif
