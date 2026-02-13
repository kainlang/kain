// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/FPostProcessParams.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeFPostProcessParams() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FPostProcessParams();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin ScriptStruct FPostProcessParams
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_PostProcessParams;
class UScriptStruct* FPostProcessParams::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_PostProcessParams.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_PostProcessParams.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FPostProcessParams, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("PostProcessParams"));
	}
	return Z_Registration_Info_UScriptStruct_PostProcessParams.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UScriptStruct* StaticStruct<FPostProcessParams>()
{
	return FPostProcessParams::StaticStruct();
}
struct Z_Construct_UScriptStruct_FPostProcessParams_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/FPostProcessParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_decay_MetaData[] = {
		{ "Category", "PostProcessParams" },
		{ "ModuleRelativePath", "Public/FPostProcessParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_aberration_MetaData[] = {
		{ "Category", "PostProcessParams" },
		{ "ModuleRelativePath", "Public/FPostProcessParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_distortion_MetaData[] = {
		{ "Category", "PostProcessParams" },
		{ "ModuleRelativePath", "Public/FPostProcessParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bloom_intensity_MetaData[] = {
		{ "Category", "PostProcessParams" },
		{ "ModuleRelativePath", "Public/FPostProcessParams.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_decay;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_aberration;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_distortion;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bloom_intensity;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FPostProcessParams>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_decay = { "decay", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FPostProcessParams, decay), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_decay_MetaData), NewProp_decay_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_aberration = { "aberration", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FPostProcessParams, aberration), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_aberration_MetaData), NewProp_aberration_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_distortion = { "distortion", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FPostProcessParams, distortion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_distortion_MetaData), NewProp_distortion_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_bloom_intensity = { "bloom_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FPostProcessParams, bloom_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bloom_intensity_MetaData), NewProp_bloom_intensity_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FPostProcessParams_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_decay,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_aberration,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_distortion,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewProp_bloom_intensity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FPostProcessParams_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FPostProcessParams_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	&NewStructOps,
	"PostProcessParams",
	Z_Construct_UScriptStruct_FPostProcessParams_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FPostProcessParams_Statics::PropPointers),
	sizeof(FPostProcessParams),
	alignof(FPostProcessParams),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FPostProcessParams_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FPostProcessParams_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FPostProcessParams()
{
	if (!Z_Registration_Info_UScriptStruct_PostProcessParams.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_PostProcessParams.InnerSingleton, Z_Construct_UScriptStruct_FPostProcessParams_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_PostProcessParams.InnerSingleton;
}
// End ScriptStruct FPostProcessParams

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FPostProcessParams_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FPostProcessParams::StaticStruct, Z_Construct_UScriptStruct_FPostProcessParams_Statics::NewStructOps, TEXT("PostProcessParams"), &Z_Registration_Info_UScriptStruct_PostProcessParams, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FPostProcessParams), 3210171026U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FPostProcessParams_h_1203572525(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FPostProcessParams_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FPostProcessParams_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
