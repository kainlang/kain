// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/FAudioReactivity.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeFAudioReactivity() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FAudioReactivity();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin ScriptStruct FAudioReactivity
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_AudioReactivity;
class UScriptStruct* FAudioReactivity::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_AudioReactivity.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_AudioReactivity.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FAudioReactivity, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("AudioReactivity"));
	}
	return Z_Registration_Info_UScriptStruct_AudioReactivity.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UScriptStruct* StaticStruct<FAudioReactivity>()
{
	return FAudioReactivity::StaticStruct();
}
struct Z_Construct_UScriptStruct_FAudioReactivity_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/FAudioReactivity.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_enabled_MetaData[] = {
		{ "Category", "AudioReactivity" },
		{ "ModuleRelativePath", "Public/FAudioReactivity.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bass_level_MetaData[] = {
		{ "Category", "AudioReactivity" },
		{ "ModuleRelativePath", "Public/FAudioReactivity.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_high_level_MetaData[] = {
		{ "Category", "AudioReactivity" },
		{ "ModuleRelativePath", "Public/FAudioReactivity.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_overall_level_MetaData[] = {
		{ "Category", "AudioReactivity" },
		{ "ModuleRelativePath", "Public/FAudioReactivity.h" },
	};
#endif // WITH_METADATA
	static void NewProp_enabled_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_enabled;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bass_level;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_high_level;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_overall_level;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FAudioReactivity>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
void Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_enabled_SetBit(void* Obj)
{
	((FAudioReactivity*)Obj)->enabled = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_enabled = { "enabled", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FAudioReactivity), &Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_enabled_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_enabled_MetaData), NewProp_enabled_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_bass_level = { "bass_level", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FAudioReactivity, bass_level), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bass_level_MetaData), NewProp_bass_level_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_high_level = { "high_level", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FAudioReactivity, high_level), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_high_level_MetaData), NewProp_high_level_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_overall_level = { "overall_level", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FAudioReactivity, overall_level), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_overall_level_MetaData), NewProp_overall_level_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FAudioReactivity_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_enabled,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_bass_level,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_high_level,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewProp_overall_level,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FAudioReactivity_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FAudioReactivity_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	&NewStructOps,
	"AudioReactivity",
	Z_Construct_UScriptStruct_FAudioReactivity_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FAudioReactivity_Statics::PropPointers),
	sizeof(FAudioReactivity),
	alignof(FAudioReactivity),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FAudioReactivity_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FAudioReactivity_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FAudioReactivity()
{
	if (!Z_Registration_Info_UScriptStruct_AudioReactivity.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_AudioReactivity.InnerSingleton, Z_Construct_UScriptStruct_FAudioReactivity_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_AudioReactivity.InnerSingleton;
}
// End ScriptStruct FAudioReactivity

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FAudioReactivity_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FAudioReactivity::StaticStruct, Z_Construct_UScriptStruct_FAudioReactivity_Statics::NewStructOps, TEXT("AudioReactivity"), &Z_Registration_Info_UScriptStruct_AudioReactivity, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FAudioReactivity), 2033084769U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FAudioReactivity_h_3776019995(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FAudioReactivity_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FAudioReactivity_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
