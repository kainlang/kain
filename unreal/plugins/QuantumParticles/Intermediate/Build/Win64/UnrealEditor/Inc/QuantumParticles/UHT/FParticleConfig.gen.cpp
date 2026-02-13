// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/FParticleConfig.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeFParticleConfig() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FParticleConfig();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin ScriptStruct FParticleConfig
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_ParticleConfig;
class UScriptStruct* FParticleConfig::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_ParticleConfig.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_ParticleConfig.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FParticleConfig, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("ParticleConfig"));
	}
	return Z_Registration_Info_UScriptStruct_ParticleConfig.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UScriptStruct* StaticStruct<FParticleConfig>()
{
	return FParticleConfig::StaticStruct();
}
struct Z_Construct_UScriptStruct_FParticleConfig_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/FParticleConfig.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_simulation_resolution_MetaData[] = {
		{ "Category", "ParticleConfig" },
		{ "ModuleRelativePath", "Public/FParticleConfig.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_particle_count_MetaData[] = {
		{ "Category", "ParticleConfig" },
		{ "ModuleRelativePath", "Public/FParticleConfig.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_point_size_MetaData[] = {
		{ "Category", "ParticleConfig" },
		{ "ModuleRelativePath", "Public/FParticleConfig.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_opacity_MetaData[] = {
		{ "Category", "ParticleConfig" },
		{ "ModuleRelativePath", "Public/FParticleConfig.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FInt64PropertyParams NewProp_simulation_resolution;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_particle_count;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_point_size;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_opacity;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FParticleConfig>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_simulation_resolution = { "simulation_resolution", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FParticleConfig, simulation_resolution), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_simulation_resolution_MetaData), NewProp_simulation_resolution_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_particle_count = { "particle_count", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FParticleConfig, particle_count), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_particle_count_MetaData), NewProp_particle_count_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_point_size = { "point_size", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FParticleConfig, point_size), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_point_size_MetaData), NewProp_point_size_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_opacity = { "opacity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FParticleConfig, opacity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_opacity_MetaData), NewProp_opacity_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FParticleConfig_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_simulation_resolution,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_particle_count,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_point_size,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FParticleConfig_Statics::NewProp_opacity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FParticleConfig_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FParticleConfig_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	&NewStructOps,
	"ParticleConfig",
	Z_Construct_UScriptStruct_FParticleConfig_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FParticleConfig_Statics::PropPointers),
	sizeof(FParticleConfig),
	alignof(FParticleConfig),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FParticleConfig_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FParticleConfig_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FParticleConfig()
{
	if (!Z_Registration_Info_UScriptStruct_ParticleConfig.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_ParticleConfig.InnerSingleton, Z_Construct_UScriptStruct_FParticleConfig_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_ParticleConfig.InnerSingleton;
}
// End ScriptStruct FParticleConfig

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleConfig_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FParticleConfig::StaticStruct, Z_Construct_UScriptStruct_FParticleConfig_Statics::NewStructOps, TEXT("ParticleConfig"), &Z_Registration_Info_UScriptStruct_ParticleConfig, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FParticleConfig), 1273624142U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleConfig_h_3652777573(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleConfig_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FParticleConfig_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
