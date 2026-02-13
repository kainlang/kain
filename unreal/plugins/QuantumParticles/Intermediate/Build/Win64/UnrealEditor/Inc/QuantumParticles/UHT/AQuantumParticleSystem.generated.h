// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "AQuantumParticleSystem.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
enum class EColorMode : uint8;
enum class ESimulationMode : uint8;
#ifdef QUANTUMPARTICLES_AQuantumParticleSystem_generated_h
#error "AQuantumParticleSystem.generated.h already included, missing '#pragma once' in AQuantumParticleSystem.h"
#endif
#define QUANTUMPARTICLES_AQuantumParticleSystem_generated_h

#define FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_23_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execDisableAudioReactivity); \
	DECLARE_FUNCTION(execEnableAudioReactivity); \
	DECLARE_FUNCTION(execSetAudioLevels); \
	DECLARE_FUNCTION(execDisableSwarm); \
	DECLARE_FUNCTION(execEnableSwarm); \
	DECLARE_FUNCTION(execDisableExplosion); \
	DECLARE_FUNCTION(execEnableExplosion); \
	DECLARE_FUNCTION(execDisableGravity); \
	DECLARE_FUNCTION(execEnableGravity); \
	DECLARE_FUNCTION(execDisableVortex); \
	DECLARE_FUNCTION(execEnableVortex); \
	DECLARE_FUNCTION(execDisableHeartbeat); \
	DECLARE_FUNCTION(execEnableHeartbeat); \
	DECLARE_FUNCTION(execSetOpacity); \
	DECLARE_FUNCTION(execSetPointSize); \
	DECLARE_FUNCTION(execSetSecondaryColor); \
	DECLARE_FUNCTION(execSetPrimaryColor); \
	DECLARE_FUNCTION(execSetColorMode); \
	DECLARE_FUNCTION(execResetSimulation); \
	DECLARE_FUNCTION(execSetDamping); \
	DECLARE_FUNCTION(execSetChaos); \
	DECLARE_FUNCTION(execSetSpeed); \
	DECLARE_FUNCTION(execSetSimulationMode);


#define FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_23_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAQuantumParticleSystem(); \
	friend struct Z_Construct_UClass_AQuantumParticleSystem_Statics; \
public: \
	DECLARE_CLASS(AQuantumParticleSystem, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/QuantumParticles"), NO_API) \
	DECLARE_SERIALIZER(AQuantumParticleSystem)


#define FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_23_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AQuantumParticleSystem(AQuantumParticleSystem&&); \
	AQuantumParticleSystem(const AQuantumParticleSystem&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AQuantumParticleSystem); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AQuantumParticleSystem); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AQuantumParticleSystem) \
	NO_API virtual ~AQuantumParticleSystem();


#define FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_20_PROLOG
#define FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_23_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_23_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_23_INCLASS_NO_PURE_DECLS \
	FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h_23_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> QUANTUMPARTICLES_API UClass* StaticClass<class AQuantumParticleSystem>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_MyProject_Plugins_QuantumParticles_Source_Public_AQuantumParticleSystem_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
