// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/FParticleLifeParams.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeFParticleLifeParams() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FParticleLifeParams();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin ScriptStruct FParticleLifeParams
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_ParticleLifeParams;
class UScriptStruct* FParticleLifeParams::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_ParticleLifeParams.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_ParticleLifeParams.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FParticleLifeParams, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("ParticleLifeParams"));
	}
	return Z_Registration_Info_UScriptStruct_ParticleLifeParams.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UScriptStruct* StaticStruct<FParticleLifeParams>()
{
	return FParticleLifeParams::StaticStruct();
}
struct Z_Construct_UScriptStruct_FParticleLifeParams_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/FParticleLifeParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_enabled_MetaData[] = {
		{ "Category", "ParticleLifeParams" },
		{ "ModuleRelativePath", "Public/FParticleLifeParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_decay_rate_MetaData[] = {
		{ "Category", "ParticleLifeParams" },
		{ "ModuleRelativePath", "Public/FParticleLifeParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_respawn_bounds_MetaData[] = {
		{ "Category", "ParticleLifeParams" },
		{ "ModuleRelativePath", "Public/FParticleLifeParams.h" },
	};
#endif // WITH_METADATA
	static void NewProp_enabled_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_enabled;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_decay_rate;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_respawn_bounds;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FParticleLifeParams>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
void Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_enabled_SetBit(void* Obj)
{
	((FParticleLifeParams*)Obj)->enabled = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_enabled = { "enabled", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FParticleLifeParams), &Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_enabled_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_enabled_MetaData), NewProp_enabled_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_decay_rate = { "decay_rate", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FParticleLifeParams, decay_rate), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_decay_rate_MetaData), NewProp_decay_rate_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_respawn_bounds = { "respawn_bounds", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FParticleLifeParams, respawn_bounds), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_respawn_bounds_MetaData), NewProp_respawn_bounds_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FParticleLifeParams_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_enabled,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_decay_rate,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewProp_respawn_bounds,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FParticleLifeParams_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FParticleLifeParams_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	&NewStructOps,
	"ParticleLifeParams",
	Z_Construct_UScriptStruct_FParticleLifeParams_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FParticleLifeParams_Statics::PropPointers),
	sizeof(FParticleLifeParams),
	alignof(FParticleLifeParams),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FParticleLifeParams_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FParticleLifeParams_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FParticleLifeParams()
{
	if (!Z_Registration_Info_UScriptStruct_ParticleLifeParams.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_ParticleLifeParams.InnerSingleton, Z_Construct_UScriptStruct_FParticleLifeParams_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_ParticleLifeParams.InnerSingleton;
}
// End ScriptStruct FParticleLifeParams

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleLifeParams_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FParticleLifeParams::StaticStruct, Z_Construct_UScriptStruct_FParticleLifeParams_Statics::NewStructOps, TEXT("ParticleLifeParams"), &Z_Registration_Info_UScriptStruct_ParticleLifeParams, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FParticleLifeParams), 3454634139U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleLifeParams_h_4117370378(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleLifeParams_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleLifeParams_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
