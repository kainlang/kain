// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "EModifierType.h"
#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef QUANTUMPARTICLES_EModifierType_generated_h
#error "EModifierType.generated.h already included, missing '#pragma once' in EModifierType.h"
#endif
#define QUANTUMPARTICLES_EModifierType_generated_h

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_MyProject_Plugins_QuantumParticles_Source_Public_EModifierType_h


#define FOREACH_ENUM_EMODIFIERTYPE(op) \
	op(EModifierType::Heartbeat) \
	op(EModifierType::Seismic) \
	op(EModifierType::Pulse) \
	op(EModifierType::Breathe) \
	op(EModifierType::Helix) \
	op(EModifierType::Gravity) \
	op(EModifierType::Repulsor) \
	op(EModifierType::Orbit) \
	op(EModifierType::Vortex) \
	op(EModifierType::Magnet) \
	op(EModifierType::Explosion) \
	op(EModifierType::Swarm) 

enum class EModifierType : uint8;
template<> struct TIsUEnumClass<EModifierType> { enum { Value = true }; };
template<> QUANTUMPARTICLES_API UEnum* StaticEnum<EModifierType>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
