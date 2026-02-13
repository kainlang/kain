// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "QuantumParticlesBlueprintLibrary.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
enum class ESimulationMode : uint8;
struct FParticleConfig;
struct FSimulationParams;
#ifdef QUANTUMPARTICLES_QuantumParticlesBlueprintLibrary_generated_h
#error "QuantumParticlesBlueprintLibrary.generated.h already included, missing '#pragma once' in QuantumParticlesBlueprintLibrary.h"
#endif
#define QUANTUMPARTICLES_QuantumParticlesBlueprintLibrary_generated_h

#define FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_33_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execget_mode_name); \
	DECLARE_FUNCTION(execcalculate_particle_count); \
	DECLARE_FUNCTION(execlerp_color); \
	DECLARE_FUNCTION(execcreate_simulation_params); \
	DECLARE_FUNCTION(execcreate_particle_config);


#define FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_33_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUQuantumParticlesBlueprintLibraryFunctionLibrary(); \
	friend struct Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics; \
public: \
	DECLARE_CLASS(UQuantumParticlesBlueprintLibraryFunctionLibrary, UBlueprintFunctionLibrary, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/QuantumParticles"), NO_API) \
	DECLARE_SERIALIZER(UQuantumParticlesBlueprintLibraryFunctionLibrary)


#define FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_33_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UQuantumParticlesBlueprintLibraryFunctionLibrary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UQuantumParticlesBlueprintLibraryFunctionLibrary(UQuantumParticlesBlueprintLibraryFunctionLibrary&&); \
	UQuantumParticlesBlueprintLibraryFunctionLibrary(const UQuantumParticlesBlueprintLibraryFunctionLibrary&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UQuantumParticlesBlueprintLibraryFunctionLibrary); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UQuantumParticlesBlueprintLibraryFunctionLibrary); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UQuantumParticlesBlueprintLibraryFunctionLibrary) \
	NO_API virtual ~UQuantumParticlesBlueprintLibraryFunctionLibrary();


#define FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_30_PROLOG
#define FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_33_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_33_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_33_INCLASS_NO_PURE_DECLS \
	FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_33_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> QUANTUMPARTICLES_API UClass* StaticClass<class UQuantumParticlesBlueprintLibraryFunctionLibrary>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
