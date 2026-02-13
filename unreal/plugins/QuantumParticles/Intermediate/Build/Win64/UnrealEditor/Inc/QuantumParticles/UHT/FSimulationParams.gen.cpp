// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/FSimulationParams.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeFSimulationParams() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UEnum* Z_Construct_UEnum_QuantumParticles_ESimulationMode();
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FSimulationParams();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin ScriptStruct FSimulationParams
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_SimulationParams;
class UScriptStruct* FSimulationParams::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_SimulationParams.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_SimulationParams.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FSimulationParams, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("SimulationParams"));
	}
	return Z_Registration_Info_UScriptStruct_SimulationParams.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UScriptStruct* StaticStruct<FSimulationParams>()
{
	return FSimulationParams::StaticStruct();
}
struct Z_Construct_UScriptStruct_FSimulationParams_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_mode_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_speed_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_chaos_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_damping_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_force_multiplier_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_curl_strength_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_center_pull_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_max_velocity_MetaData[] = {
		{ "Category", "SimulationParams" },
		{ "ModuleRelativePath", "Public/FSimulationParams.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_mode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_mode;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_speed;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_chaos;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_damping;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_force_multiplier;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_curl_strength;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_center_pull;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_max_velocity;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FSimulationParams>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_mode = { "mode", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, mode), Z_Construct_UEnum_QuantumParticles_ESimulationMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mode_MetaData), NewProp_mode_MetaData) }; // 3496891889
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_speed = { "speed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_speed_MetaData), NewProp_speed_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_chaos = { "chaos", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, chaos), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_chaos_MetaData), NewProp_chaos_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_damping = { "damping", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, damping), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_damping_MetaData), NewProp_damping_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_force_multiplier = { "force_multiplier", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, force_multiplier), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_force_multiplier_MetaData), NewProp_force_multiplier_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_curl_strength = { "curl_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, curl_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_curl_strength_MetaData), NewProp_curl_strength_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_center_pull = { "center_pull", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, center_pull), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_center_pull_MetaData), NewProp_center_pull_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_max_velocity = { "max_velocity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSimulationParams, max_velocity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_max_velocity_MetaData), NewProp_max_velocity_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FSimulationParams_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_mode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_mode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_chaos,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_damping,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_force_multiplier,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_curl_strength,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_center_pull,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSimulationParams_Statics::NewProp_max_velocity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSimulationParams_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FSimulationParams_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	&NewStructOps,
	"SimulationParams",
	Z_Construct_UScriptStruct_FSimulationParams_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSimulationParams_Statics::PropPointers),
	sizeof(FSimulationParams),
	alignof(FSimulationParams),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSimulationParams_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FSimulationParams_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FSimulationParams()
{
	if (!Z_Registration_Info_UScriptStruct_SimulationParams.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_SimulationParams.InnerSingleton, Z_Construct_UScriptStruct_FSimulationParams_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_SimulationParams.InnerSingleton;
}
// End ScriptStruct FSimulationParams

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FSimulationParams_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FSimulationParams::StaticStruct, Z_Construct_UScriptStruct_FSimulationParams_Statics::NewStructOps, TEXT("SimulationParams"), &Z_Registration_Info_UScriptStruct_SimulationParams, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FSimulationParams), 3295150083U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FSimulationParams_h_823683465(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FSimulationParams_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FSimulationParams_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
