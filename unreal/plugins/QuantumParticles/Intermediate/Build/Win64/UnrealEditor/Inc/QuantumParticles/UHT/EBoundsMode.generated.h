// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "EBoundsMode.h"
#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef QUANTUMPARTICLES_EBoundsMode_generated_h
#error "EBoundsMode.generated.h already included, missing '#pragma once' in EBoundsMode.h"
#endif
#define QUANTUMPARTICLES_EBoundsMode_generated_h

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_MyProject_Plugins_QuantumParticles_Source_Public_EBoundsMode_h


#define FOREACH_ENUM_EBOUNDSMODE(op) \
	op(EBoundsMode::Bounce) \
	op(EBoundsMode::Wrap) \
	op(EBoundsMode::Respawn) 

enum class EBoundsMode : uint8;
template<> struct TIsUEnumClass<EBoundsMode> { enum { Value = true }; };
template<> QUANTUMPARTICLES_API UEnum* StaticEnum<EBoundsMode>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
