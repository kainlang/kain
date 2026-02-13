// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/QuantumParticlesBlueprintLibrary.h"
#include "Public/FParticleConfig.h"
#include "Public/FSimulationParams.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeQuantumParticlesBlueprintLibrary() {}

// Begin Cross Module References
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector();
ENGINE_API UClass* Z_Construct_UClass_UBlueprintFunctionLibrary();
QUANTUMPARTICLES_API UClass* Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary();
QUANTUMPARTICLES_API UClass* Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_NoRegister();
QUANTUMPARTICLES_API UEnum* Z_Construct_UEnum_QuantumParticles_ESimulationMode();
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FParticleConfig();
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FSimulationParams();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function calculate_particle_count
struct Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics
{
	struct QuantumParticlesBlueprintLibraryFunctionLibrary_eventcalculate_particle_count_Parms
	{
		int64 resolution;
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/QuantumParticlesBlueprintLibrary.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_resolution_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FInt64PropertyParams NewProp_resolution;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::NewProp_resolution = { "resolution", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcalculate_particle_count_Parms, resolution), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_resolution_MetaData), NewProp_resolution_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcalculate_particle_count_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::NewProp_resolution,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary, nullptr, "calculate_particle_count", nullptr, nullptr, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::PropPointers), sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventcalculate_particle_count_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::Function_MetaDataParams), Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventcalculate_particle_count_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UQuantumParticlesBlueprintLibraryFunctionLibrary::execcalculate_particle_count)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_resolution);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=UQuantumParticlesBlueprintLibraryFunctionLibrary::calculate_particle_count(Z_Param_resolution);
	P_NATIVE_END;
}
// End Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function calculate_particle_count

// Begin Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function create_particle_config
struct Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics
{
	struct QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_particle_config_Parms
	{
		int64 resolution;
		float size;
		float opacity;
		FParticleConfig ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/QuantumParticlesBlueprintLibrary.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_resolution_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_size_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_opacity_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FInt64PropertyParams NewProp_resolution;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_size;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_opacity;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_resolution = { "resolution", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_particle_config_Parms, resolution), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_resolution_MetaData), NewProp_resolution_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_size = { "size", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_particle_config_Parms, size), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_size_MetaData), NewProp_size_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_opacity = { "opacity", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_particle_config_Parms, opacity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_opacity_MetaData), NewProp_opacity_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_particle_config_Parms, ReturnValue), Z_Construct_UScriptStruct_FParticleConfig, METADATA_PARAMS(0, nullptr) }; // 1273624142
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_resolution,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_size,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_opacity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary, nullptr, "create_particle_config", nullptr, nullptr, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::PropPointers), sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_particle_config_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::Function_MetaDataParams), Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_particle_config_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UQuantumParticlesBlueprintLibraryFunctionLibrary::execcreate_particle_config)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_resolution);
	P_GET_PROPERTY(FFloatProperty,Z_Param_size);
	P_GET_PROPERTY(FFloatProperty,Z_Param_opacity);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FParticleConfig*)Z_Param__Result=UQuantumParticlesBlueprintLibraryFunctionLibrary::create_particle_config(Z_Param_resolution,Z_Param_size,Z_Param_opacity);
	P_NATIVE_END;
}
// End Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function create_particle_config

// Begin Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function create_simulation_params
struct Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics
{
	struct QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_simulation_params_Parms
	{
		ESimulationMode mode;
		float speed;
		float chaos;
		FSimulationParams ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/QuantumParticlesBlueprintLibrary.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_mode_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_speed_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_chaos_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_mode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_mode;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_speed;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_chaos;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_mode = { "mode", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_simulation_params_Parms, mode), Z_Construct_UEnum_QuantumParticles_ESimulationMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mode_MetaData), NewProp_mode_MetaData) }; // 3496891889
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_speed = { "speed", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_simulation_params_Parms, speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_speed_MetaData), NewProp_speed_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_chaos = { "chaos", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_simulation_params_Parms, chaos), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_chaos_MetaData), NewProp_chaos_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_simulation_params_Parms, ReturnValue), Z_Construct_UScriptStruct_FSimulationParams, METADATA_PARAMS(0, nullptr) }; // 3295150083
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_mode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_mode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_chaos,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary, nullptr, "create_simulation_params", nullptr, nullptr, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::PropPointers), sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_simulation_params_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::Function_MetaDataParams), Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventcreate_simulation_params_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UQuantumParticlesBlueprintLibraryFunctionLibrary::execcreate_simulation_params)
{
	P_GET_ENUM(ESimulationMode,Z_Param_mode);
	P_GET_PROPERTY(FFloatProperty,Z_Param_speed);
	P_GET_PROPERTY(FFloatProperty,Z_Param_chaos);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FSimulationParams*)Z_Param__Result=UQuantumParticlesBlueprintLibraryFunctionLibrary::create_simulation_params(ESimulationMode(Z_Param_mode),Z_Param_speed,Z_Param_chaos);
	P_NATIVE_END;
}
// End Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function create_simulation_params

// Begin Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function get_mode_name
struct Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics
{
	struct QuantumParticlesBlueprintLibraryFunctionLibrary_eventget_mode_name_Parms
	{
		ESimulationMode mode;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/QuantumParticlesBlueprintLibrary.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_mode_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_mode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_mode;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::NewProp_mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::NewProp_mode = { "mode", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventget_mode_name_Parms, mode), Z_Construct_UEnum_QuantumParticles_ESimulationMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mode_MetaData), NewProp_mode_MetaData) }; // 3496891889
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventget_mode_name_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::NewProp_mode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::NewProp_mode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary, nullptr, "get_mode_name", nullptr, nullptr, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::PropPointers), sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventget_mode_name_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::Function_MetaDataParams), Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventget_mode_name_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UQuantumParticlesBlueprintLibraryFunctionLibrary::execget_mode_name)
{
	P_GET_ENUM(ESimulationMode,Z_Param_mode);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UQuantumParticlesBlueprintLibraryFunctionLibrary::get_mode_name(ESimulationMode(Z_Param_mode));
	P_NATIVE_END;
}
// End Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function get_mode_name

// Begin Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function lerp_color
struct Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics
{
	struct QuantumParticlesBlueprintLibraryFunctionLibrary_eventlerp_color_Parms
	{
		FVector a;
		FVector b;
		float t;
		FVector ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/QuantumParticlesBlueprintLibrary.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_a_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_b_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_t_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_a;
	static const UECodeGen_Private::FStructPropertyParams NewProp_b;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_t;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_a = { "a", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventlerp_color_Parms, a), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_a_MetaData), NewProp_a_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_b = { "b", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventlerp_color_Parms, b), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_b_MetaData), NewProp_b_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_t = { "t", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventlerp_color_Parms, t), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_t_MetaData), NewProp_t_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(QuantumParticlesBlueprintLibraryFunctionLibrary_eventlerp_color_Parms, ReturnValue), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_a,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_b,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_t,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary, nullptr, "lerp_color", nullptr, nullptr, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::PropPointers), sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventlerp_color_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04822401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::Function_MetaDataParams), Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::QuantumParticlesBlueprintLibraryFunctionLibrary_eventlerp_color_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UQuantumParticlesBlueprintLibraryFunctionLibrary::execlerp_color)
{
	P_GET_STRUCT(FVector,Z_Param_a);
	P_GET_STRUCT(FVector,Z_Param_b);
	P_GET_PROPERTY(FFloatProperty,Z_Param_t);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FVector*)Z_Param__Result=UQuantumParticlesBlueprintLibraryFunctionLibrary::lerp_color(Z_Param_a,Z_Param_b,Z_Param_t);
	P_NATIVE_END;
}
// End Class UQuantumParticlesBlueprintLibraryFunctionLibrary Function lerp_color

// Begin Class UQuantumParticlesBlueprintLibraryFunctionLibrary
void UQuantumParticlesBlueprintLibraryFunctionLibrary::StaticRegisterNativesUQuantumParticlesBlueprintLibraryFunctionLibrary()
{
	UClass* Class = UQuantumParticlesBlueprintLibraryFunctionLibrary::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "calculate_particle_count", &UQuantumParticlesBlueprintLibraryFunctionLibrary::execcalculate_particle_count },
		{ "create_particle_config", &UQuantumParticlesBlueprintLibraryFunctionLibrary::execcreate_particle_config },
		{ "create_simulation_params", &UQuantumParticlesBlueprintLibraryFunctionLibrary::execcreate_simulation_params },
		{ "get_mode_name", &UQuantumParticlesBlueprintLibraryFunctionLibrary::execget_mode_name },
		{ "lerp_color", &UQuantumParticlesBlueprintLibraryFunctionLibrary::execlerp_color },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UQuantumParticlesBlueprintLibraryFunctionLibrary);
UClass* Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_NoRegister()
{
	return UQuantumParticlesBlueprintLibraryFunctionLibrary::StaticClass();
}
struct Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "QuantumParticlesBlueprintLibrary.h" },
		{ "ModuleRelativePath", "Public/QuantumParticlesBlueprintLibrary.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_calculate_particle_count, "calculate_particle_count" }, // 225271620
		{ &Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_particle_config, "create_particle_config" }, // 2942258761
		{ &Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_create_simulation_params, "create_simulation_params" }, // 2147840212
		{ &Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_get_mode_name, "get_mode_name" }, // 209707932
		{ &Z_Construct_UFunction_UQuantumParticlesBlueprintLibraryFunctionLibrary_lerp_color, "lerp_color" }, // 1421199043
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UQuantumParticlesBlueprintLibraryFunctionLibrary>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UBlueprintFunctionLibrary,
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics::ClassParams = {
	&UQuantumParticlesBlueprintLibraryFunctionLibrary::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	0,
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics::Class_MetaDataParams), Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary()
{
	if (!Z_Registration_Info_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary.OuterSingleton, Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UClass* StaticClass<UQuantumParticlesBlueprintLibraryFunctionLibrary>()
{
	return UQuantumParticlesBlueprintLibraryFunctionLibrary::StaticClass();
}
UQuantumParticlesBlueprintLibraryFunctionLibrary::UQuantumParticlesBlueprintLibraryFunctionLibrary(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(UQuantumParticlesBlueprintLibraryFunctionLibrary);
UQuantumParticlesBlueprintLibraryFunctionLibrary::~UQuantumParticlesBlueprintLibraryFunctionLibrary() {}
// End Class UQuantumParticlesBlueprintLibraryFunctionLibrary

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary, UQuantumParticlesBlueprintLibraryFunctionLibrary::StaticClass, TEXT("UQuantumParticlesBlueprintLibraryFunctionLibrary"), &Z_Registration_Info_UClass_UQuantumParticlesBlueprintLibraryFunctionLibrary, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UQuantumParticlesBlueprintLibraryFunctionLibrary), 2170930069U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_1692034704(TEXT("/Script/QuantumParticles"),
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_QuantumParticlesBlueprintLibrary_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
