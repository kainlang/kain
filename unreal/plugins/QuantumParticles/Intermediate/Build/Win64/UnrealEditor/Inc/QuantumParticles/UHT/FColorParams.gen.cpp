// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/FColorParams.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeFColorParams() {}

// Begin Cross Module References
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector();
QUANTUMPARTICLES_API UEnum* Z_Construct_UEnum_QuantumParticles_EColorMode();
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FColorParams();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin ScriptStruct FColorParams
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_ColorParams;
class UScriptStruct* FColorParams::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_ColorParams.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_ColorParams.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FColorParams, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("ColorParams"));
	}
	return Z_Registration_Info_UScriptStruct_ColorParams.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UScriptStruct* StaticStruct<FColorParams>()
{
	return FColorParams::StaticStruct();
}
struct Z_Construct_UScriptStruct_FColorParams_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/FColorParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_mode_MetaData[] = {
		{ "Category", "ColorParams" },
		{ "ModuleRelativePath", "Public/FColorParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_primary_color_MetaData[] = {
		{ "Category", "ColorParams" },
		{ "ModuleRelativePath", "Public/FColorParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_secondary_color_MetaData[] = {
		{ "Category", "ColorParams" },
		{ "ModuleRelativePath", "Public/FColorParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_gradient_strength_MetaData[] = {
		{ "Category", "ColorParams" },
		{ "ModuleRelativePath", "Public/FColorParams.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_mode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_mode;
	static const UECodeGen_Private::FStructPropertyParams NewProp_primary_color;
	static const UECodeGen_Private::FStructPropertyParams NewProp_secondary_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_gradient_strength;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FColorParams>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_mode = { "mode", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FColorParams, mode), Z_Construct_UEnum_QuantumParticles_EColorMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mode_MetaData), NewProp_mode_MetaData) }; // 1449425345
const UECodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_primary_color = { "primary_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FColorParams, primary_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_primary_color_MetaData), NewProp_primary_color_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_secondary_color = { "secondary_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FColorParams, secondary_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_secondary_color_MetaData), NewProp_secondary_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_gradient_strength = { "gradient_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FColorParams, gradient_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_gradient_strength_MetaData), NewProp_gradient_strength_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FColorParams_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_mode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_mode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_primary_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_secondary_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FColorParams_Statics::NewProp_gradient_strength,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FColorParams_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FColorParams_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	&NewStructOps,
	"ColorParams",
	Z_Construct_UScriptStruct_FColorParams_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FColorParams_Statics::PropPointers),
	sizeof(FColorParams),
	alignof(FColorParams),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FColorParams_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FColorParams_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FColorParams()
{
	if (!Z_Registration_Info_UScriptStruct_ColorParams.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_ColorParams.InnerSingleton, Z_Construct_UScriptStruct_FColorParams_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_ColorParams.InnerSingleton;
}
// End ScriptStruct FColorParams

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FColorParams_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FColorParams::StaticStruct, Z_Construct_UScriptStruct_FColorParams_Statics::NewStructOps, TEXT("ColorParams"), &Z_Registration_Info_UScriptStruct_ColorParams, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FColorParams), 696378393U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FColorParams_h_1259175933(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FColorParams_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FColorParams_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
