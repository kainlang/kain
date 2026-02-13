// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "EColorMode.h"
#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef QUANTUMPARTICLES_EColorMode_generated_h
#error "EColorMode.generated.h already included, missing '#pragma once' in EColorMode.h"
#endif
#define QUANTUMPARTICLES_EColorMode_generated_h

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_MyProject_Plugins_QuantumParticles_Source_Public_EColorMode_h


#define FOREACH_ENUM_ECOLORMODE(op) \
	op(EColorMode::Solid) \
	op(EColorMode::Velocity) \
	op(EColorMode::Image) \
	op(EColorMode::Gradient) 

enum class EColorMode : uint8;
template<> struct TIsUEnumClass<EColorMode> { enum { Value = true }; };
template<> QUANTUMPARTICLES_API UEnum* StaticEnum<EColorMode>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
