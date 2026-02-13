// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/UltimateVFX.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeUltimateVFX() {}

// Begin Cross Module References
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector();
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector2D();
ENGINE_API UClass* Z_Construct_UClass_AActor();
ENGINE_API UClass* Z_Construct_UClass_UBlueprintFunctionLibrary();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AAmbientOcclusionActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AAmbientOcclusionActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AAtmosphericScatteringActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AAtmosphericScatteringActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ABloomLensFlareActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ABloomLensFlareActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AChromaticAberrationActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AChromaticAberrationActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AColorGradingActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AColorGradingActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ADepthOfFieldActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ADepthOfFieldActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AFilmGrainActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AFilmGrainActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AGodRaysActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AGodRaysActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AMotionBlurActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AMotionBlurActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AOceanRenderingActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AOceanRenderingActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AProceduralSkyActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AProceduralSkyActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ARainDropsActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ARainDropsActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AScreenSpaceReflectionsActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AScreenSpaceReflectionsActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ASharpenActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_ASharpenActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AVolumetricCloudsActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AVolumetricCloudsActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AVolumetricFogActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_AVolumetricFogActor_NoRegister();
ULTIMATEVFX_API UClass* Z_Construct_UClass_UUltimateVFXFunctionLibrary();
ULTIMATEVFX_API UClass* Z_Construct_UClass_UUltimateVFXFunctionLibrary_NoRegister();
ULTIMATEVFX_API UEnum* Z_Construct_UEnum_UltimateVFX_EAtmospherePreset();
ULTIMATEVFX_API UEnum* Z_Construct_UEnum_UltimateVFX_EEffectQuality();
ULTIMATEVFX_API UEnum* Z_Construct_UEnum_UltimateVFX_ETimeOfDay();
ULTIMATEVFX_API UEnum* Z_Construct_UEnum_UltimateVFX_EWeatherType();
UPackage* Z_Construct_UPackage__Script_UltimateVFX();
// End Cross Module References

// Begin Enum EEffectQuality
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EEffectQuality;
static UEnum* EEffectQuality_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EEffectQuality.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EEffectQuality.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UltimateVFX_EEffectQuality, (UObject*)Z_Construct_UPackage__Script_UltimateVFX(), TEXT("EEffectQuality"));
	}
	return Z_Registration_Info_UEnum_EEffectQuality.OuterSingleton;
}
template<> ULTIMATEVFX_API UEnum* StaticEnum<EEffectQuality>()
{
	return EEffectQuality_StaticEnum();
}
struct Z_Construct_UEnum_UltimateVFX_EEffectQuality_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "Cinematic.DisplayName", "Cinematic" },
		{ "Cinematic.Name", "EEffectQuality::Cinematic" },
		{ "High.DisplayName", "High" },
		{ "High.Name", "EEffectQuality::High" },
		{ "Low.DisplayName", "Low" },
		{ "Low.Name", "EEffectQuality::Low" },
		{ "Medium.DisplayName", "Medium" },
		{ "Medium.Name", "EEffectQuality::Medium" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
		{ "Potato.DisplayName", "Potato" },
		{ "Potato.Name", "EEffectQuality::Potato" },
		{ "Ultra.DisplayName", "Ultra" },
		{ "Ultra.Name", "EEffectQuality::Ultra" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EEffectQuality::Potato", (int64)EEffectQuality::Potato },
		{ "EEffectQuality::Low", (int64)EEffectQuality::Low },
		{ "EEffectQuality::Medium", (int64)EEffectQuality::Medium },
		{ "EEffectQuality::High", (int64)EEffectQuality::High },
		{ "EEffectQuality::Ultra", (int64)EEffectQuality::Ultra },
		{ "EEffectQuality::Cinematic", (int64)EEffectQuality::Cinematic },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UltimateVFX_EEffectQuality_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UltimateVFX,
	nullptr,
	"EEffectQuality",
	"EEffectQuality",
	Z_Construct_UEnum_UltimateVFX_EEffectQuality_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_EEffectQuality_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_EEffectQuality_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UltimateVFX_EEffectQuality_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UltimateVFX_EEffectQuality()
{
	if (!Z_Registration_Info_UEnum_EEffectQuality.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EEffectQuality.InnerSingleton, Z_Construct_UEnum_UltimateVFX_EEffectQuality_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EEffectQuality.InnerSingleton;
}
// End Enum EEffectQuality

// Begin Enum ETimeOfDay
static FEnumRegistrationInfo Z_Registration_Info_UEnum_ETimeOfDay;
static UEnum* ETimeOfDay_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_ETimeOfDay.OuterSingleton)
	{
		Z_Registration_Info_UEnum_ETimeOfDay.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UltimateVFX_ETimeOfDay, (UObject*)Z_Construct_UPackage__Script_UltimateVFX(), TEXT("ETimeOfDay"));
	}
	return Z_Registration_Info_UEnum_ETimeOfDay.OuterSingleton;
}
template<> ULTIMATEVFX_API UEnum* StaticEnum<ETimeOfDay>()
{
	return ETimeOfDay_StaticEnum();
}
struct Z_Construct_UEnum_UltimateVFX_ETimeOfDay_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "Afternoon.DisplayName", "Afternoon" },
		{ "Afternoon.Name", "ETimeOfDay::Afternoon" },
		{ "BlueprintType", "true" },
		{ "Dawn.DisplayName", "Dawn" },
		{ "Dawn.Name", "ETimeOfDay::Dawn" },
		{ "Dusk.DisplayName", "Dusk" },
		{ "Dusk.Name", "ETimeOfDay::Dusk" },
		{ "Midnight.DisplayName", "Midnight" },
		{ "Midnight.Name", "ETimeOfDay::Midnight" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
		{ "Morning.DisplayName", "Morning" },
		{ "Morning.Name", "ETimeOfDay::Morning" },
		{ "Night.DisplayName", "Night" },
		{ "Night.Name", "ETimeOfDay::Night" },
		{ "Noon.DisplayName", "Noon" },
		{ "Noon.Name", "ETimeOfDay::Noon" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "ETimeOfDay::Dawn", (int64)ETimeOfDay::Dawn },
		{ "ETimeOfDay::Morning", (int64)ETimeOfDay::Morning },
		{ "ETimeOfDay::Noon", (int64)ETimeOfDay::Noon },
		{ "ETimeOfDay::Afternoon", (int64)ETimeOfDay::Afternoon },
		{ "ETimeOfDay::Dusk", (int64)ETimeOfDay::Dusk },
		{ "ETimeOfDay::Night", (int64)ETimeOfDay::Night },
		{ "ETimeOfDay::Midnight", (int64)ETimeOfDay::Midnight },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UltimateVFX_ETimeOfDay_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UltimateVFX,
	nullptr,
	"ETimeOfDay",
	"ETimeOfDay",
	Z_Construct_UEnum_UltimateVFX_ETimeOfDay_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_ETimeOfDay_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_ETimeOfDay_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UltimateVFX_ETimeOfDay_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UltimateVFX_ETimeOfDay()
{
	if (!Z_Registration_Info_UEnum_ETimeOfDay.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_ETimeOfDay.InnerSingleton, Z_Construct_UEnum_UltimateVFX_ETimeOfDay_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_ETimeOfDay.InnerSingleton;
}
// End Enum ETimeOfDay

// Begin Enum EWeatherType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EWeatherType;
static UEnum* EWeatherType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EWeatherType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EWeatherType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UltimateVFX_EWeatherType, (UObject*)Z_Construct_UPackage__Script_UltimateVFX(), TEXT("EWeatherType"));
	}
	return Z_Registration_Info_UEnum_EWeatherType.OuterSingleton;
}
template<> ULTIMATEVFX_API UEnum* StaticEnum<EWeatherType>()
{
	return EWeatherType_StaticEnum();
}
struct Z_Construct_UEnum_UltimateVFX_EWeatherType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "Clear.DisplayName", "Clear" },
		{ "Clear.Name", "EWeatherType::Clear" },
		{ "Cloudy.DisplayName", "Cloudy" },
		{ "Cloudy.Name", "EWeatherType::Cloudy" },
		{ "Fog.DisplayName", "Fog" },
		{ "Fog.Name", "EWeatherType::Fog" },
		{ "HeavyRain.DisplayName", "HeavyRain" },
		{ "HeavyRain.Name", "EWeatherType::HeavyRain" },
		{ "LightRain.DisplayName", "LightRain" },
		{ "LightRain.Name", "EWeatherType::LightRain" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
		{ "Overcast.DisplayName", "Overcast" },
		{ "Overcast.Name", "EWeatherType::Overcast" },
		{ "Snow.DisplayName", "Snow" },
		{ "Snow.Name", "EWeatherType::Snow" },
		{ "Storm.DisplayName", "Storm" },
		{ "Storm.Name", "EWeatherType::Storm" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EWeatherType::Clear", (int64)EWeatherType::Clear },
		{ "EWeatherType::Cloudy", (int64)EWeatherType::Cloudy },
		{ "EWeatherType::Overcast", (int64)EWeatherType::Overcast },
		{ "EWeatherType::LightRain", (int64)EWeatherType::LightRain },
		{ "EWeatherType::HeavyRain", (int64)EWeatherType::HeavyRain },
		{ "EWeatherType::Storm", (int64)EWeatherType::Storm },
		{ "EWeatherType::Snow", (int64)EWeatherType::Snow },
		{ "EWeatherType::Fog", (int64)EWeatherType::Fog },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UltimateVFX_EWeatherType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UltimateVFX,
	nullptr,
	"EWeatherType",
	"EWeatherType",
	Z_Construct_UEnum_UltimateVFX_EWeatherType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_EWeatherType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_EWeatherType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UltimateVFX_EWeatherType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UltimateVFX_EWeatherType()
{
	if (!Z_Registration_Info_UEnum_EWeatherType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EWeatherType.InnerSingleton, Z_Construct_UEnum_UltimateVFX_EWeatherType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EWeatherType.InnerSingleton;
}
// End Enum EWeatherType

// Begin Enum EAtmospherePreset
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EAtmospherePreset;
static UEnum* EAtmospherePreset_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EAtmospherePreset.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EAtmospherePreset.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_UltimateVFX_EAtmospherePreset, (UObject*)Z_Construct_UPackage__Script_UltimateVFX(), TEXT("EAtmospherePreset"));
	}
	return Z_Registration_Info_UEnum_EAtmospherePreset.OuterSingleton;
}
template<> ULTIMATEVFX_API UEnum* StaticEnum<EAtmospherePreset>()
{
	return EAtmospherePreset_StaticEnum();
}
struct Z_Construct_UEnum_UltimateVFX_EAtmospherePreset_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "Alien.DisplayName", "Alien" },
		{ "Alien.Name", "EAtmospherePreset::Alien" },
		{ "BlueprintType", "true" },
		{ "Earth.DisplayName", "Earth" },
		{ "Earth.Name", "EAtmospherePreset::Earth" },
		{ "Mars.DisplayName", "Mars" },
		{ "Mars.Name", "EAtmospherePreset::Mars" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
		{ "Space.DisplayName", "Space" },
		{ "Space.Name", "EAtmospherePreset::Space" },
		{ "Toxic.DisplayName", "Toxic" },
		{ "Toxic.Name", "EAtmospherePreset::Toxic" },
		{ "Underwater.DisplayName", "Underwater" },
		{ "Underwater.Name", "EAtmospherePreset::Underwater" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EAtmospherePreset::Earth", (int64)EAtmospherePreset::Earth },
		{ "EAtmospherePreset::Mars", (int64)EAtmospherePreset::Mars },
		{ "EAtmospherePreset::Alien", (int64)EAtmospherePreset::Alien },
		{ "EAtmospherePreset::Toxic", (int64)EAtmospherePreset::Toxic },
		{ "EAtmospherePreset::Underwater", (int64)EAtmospherePreset::Underwater },
		{ "EAtmospherePreset::Space", (int64)EAtmospherePreset::Space },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_UltimateVFX_EAtmospherePreset_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_UltimateVFX,
	nullptr,
	"EAtmospherePreset",
	"EAtmospherePreset",
	Z_Construct_UEnum_UltimateVFX_EAtmospherePreset_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_EAtmospherePreset_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_UltimateVFX_EAtmospherePreset_Statics::Enum_MetaDataParams), Z_Construct_UEnum_UltimateVFX_EAtmospherePreset_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_UltimateVFX_EAtmospherePreset()
{
	if (!Z_Registration_Info_UEnum_EAtmospherePreset.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EAtmospherePreset.InnerSingleton, Z_Construct_UEnum_UltimateVFX_EAtmospherePreset_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EAtmospherePreset.InnerSingleton;
}
// End Enum EAtmospherePreset

// Begin Class AAtmosphericScatteringActor Function Server_SetTimeOfDay
struct AtmosphericScatteringActor_eventServer_SetTimeOfDay_Parms
{
	ETimeOfDay time;
};
static FName NAME_AAtmosphericScatteringActor_Server_SetTimeOfDay = FName(TEXT("Server_SetTimeOfDay"));
void AAtmosphericScatteringActor::Server_SetTimeOfDay(const ETimeOfDay time)
{
	AtmosphericScatteringActor_eventServer_SetTimeOfDay_Parms Parms;
	Parms.time=time;
	ProcessEvent(FindFunctionChecked(NAME_AAtmosphericScatteringActor_Server_SetTimeOfDay),&Parms);
}
struct Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Server_SetTimeOfDay" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_time_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_time_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_time;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::NewProp_time_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::NewProp_time = { "time", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AtmosphericScatteringActor_eventServer_SetTimeOfDay_Parms, time), Z_Construct_UEnum_UltimateVFX_ETimeOfDay, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_time_MetaData), NewProp_time_MetaData) }; // 3512541502
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::NewProp_time_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::NewProp_time,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_AAtmosphericScatteringActor, nullptr, "Server_SetTimeOfDay", nullptr, nullptr, Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::PropPointers), sizeof(AtmosphericScatteringActor_eventServer_SetTimeOfDay_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04220CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::Function_MetaDataParams), Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::Function_MetaDataParams) };
static_assert(sizeof(AtmosphericScatteringActor_eventServer_SetTimeOfDay_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(AAtmosphericScatteringActor::execServer_SetTimeOfDay)
{
	P_GET_ENUM(ETimeOfDay,Z_Param_time);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Server_SetTimeOfDay_Implementation(ETimeOfDay(Z_Param_time));
	P_NATIVE_END;
}
// End Class AAtmosphericScatteringActor Function Server_SetTimeOfDay

// Begin Class AAtmosphericScatteringActor
void AAtmosphericScatteringActor::StaticRegisterNativesAAtmosphericScatteringActor()
{
	UClass* Class = AAtmosphericScatteringActor::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "Server_SetTimeOfDay", &AAtmosphericScatteringActor::execServer_SetTimeOfDay },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AAtmosphericScatteringActor);
UClass* Z_Construct_UClass_AAtmosphericScatteringActor_NoRegister()
{
	return AAtmosphericScatteringActor::StaticClass();
}
struct Z_Construct_UClass_AAtmosphericScatteringActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_rayleigh_coefficient_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_rayleigh_scale_height_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_mie_coefficient_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_mie_scale_height_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_mie_direction_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sun_intensity_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sun_direction_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sun_color_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_planet_radius_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_atmosphere_radius_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sample_count_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_quality_MetaData[] = {
		{ "Category", "AtmosphericScatteringActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_rayleigh_coefficient;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_rayleigh_scale_height;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_mie_coefficient;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_mie_scale_height;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_mie_direction;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_sun_intensity;
	static const UECodeGen_Private::FStructPropertyParams NewProp_sun_direction;
	static const UECodeGen_Private::FStructPropertyParams NewProp_sun_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_planet_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_atmosphere_radius;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sample_count;
	static const UECodeGen_Private::FBytePropertyParams NewProp_quality_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_quality;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_AAtmosphericScatteringActor_Server_SetTimeOfDay, "Server_SetTimeOfDay" }, // 1121195055
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AAtmosphericScatteringActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_rayleigh_coefficient = { "rayleigh_coefficient", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, rayleigh_coefficient), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_rayleigh_coefficient_MetaData), NewProp_rayleigh_coefficient_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_rayleigh_scale_height = { "rayleigh_scale_height", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, rayleigh_scale_height), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_rayleigh_scale_height_MetaData), NewProp_rayleigh_scale_height_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_mie_coefficient = { "mie_coefficient", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, mie_coefficient), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mie_coefficient_MetaData), NewProp_mie_coefficient_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_mie_scale_height = { "mie_scale_height", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, mie_scale_height), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mie_scale_height_MetaData), NewProp_mie_scale_height_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_mie_direction = { "mie_direction", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, mie_direction), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mie_direction_MetaData), NewProp_mie_direction_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sun_intensity = { "sun_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, sun_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sun_intensity_MetaData), NewProp_sun_intensity_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sun_direction = { "sun_direction", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, sun_direction), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sun_direction_MetaData), NewProp_sun_direction_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sun_color = { "sun_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, sun_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sun_color_MetaData), NewProp_sun_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_planet_radius = { "planet_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, planet_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_planet_radius_MetaData), NewProp_planet_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_atmosphere_radius = { "atmosphere_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, atmosphere_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_atmosphere_radius_MetaData), NewProp_atmosphere_radius_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sample_count = { "sample_count", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, sample_count), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sample_count_MetaData), NewProp_sample_count_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_quality_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_quality = { "quality", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAtmosphericScatteringActor, quality), Z_Construct_UEnum_UltimateVFX_EEffectQuality, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_quality_MetaData), NewProp_quality_MetaData) }; // 4088020921
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AAtmosphericScatteringActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_rayleigh_coefficient,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_rayleigh_scale_height,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_mie_coefficient,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_mie_scale_height,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_mie_direction,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sun_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sun_direction,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sun_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_planet_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_atmosphere_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_sample_count,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_quality_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAtmosphericScatteringActor_Statics::NewProp_quality,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AAtmosphericScatteringActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AAtmosphericScatteringActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AAtmosphericScatteringActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AAtmosphericScatteringActor_Statics::ClassParams = {
	&AAtmosphericScatteringActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_AAtmosphericScatteringActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_AAtmosphericScatteringActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AAtmosphericScatteringActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AAtmosphericScatteringActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AAtmosphericScatteringActor()
{
	if (!Z_Registration_Info_UClass_AAtmosphericScatteringActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AAtmosphericScatteringActor.OuterSingleton, Z_Construct_UClass_AAtmosphericScatteringActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AAtmosphericScatteringActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AAtmosphericScatteringActor>()
{
	return AAtmosphericScatteringActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AAtmosphericScatteringActor);
AAtmosphericScatteringActor::~AAtmosphericScatteringActor() {}
// End Class AAtmosphericScatteringActor

// Begin Class AVolumetricCloudsActor Function Server_SetWeather
struct VolumetricCloudsActor_eventServer_SetWeather_Parms
{
	EWeatherType weather;
};
static FName NAME_AVolumetricCloudsActor_Server_SetWeather = FName(TEXT("Server_SetWeather"));
void AVolumetricCloudsActor::Server_SetWeather(const EWeatherType weather)
{
	VolumetricCloudsActor_eventServer_SetWeather_Parms Parms;
	Parms.weather=weather;
	ProcessEvent(FindFunctionChecked(NAME_AVolumetricCloudsActor_Server_SetWeather),&Parms);
}
struct Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Server_SetWeather" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_weather_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_weather_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_weather;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::NewProp_weather_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::NewProp_weather = { "weather", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(VolumetricCloudsActor_eventServer_SetWeather_Parms, weather), Z_Construct_UEnum_UltimateVFX_EWeatherType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_weather_MetaData), NewProp_weather_MetaData) }; // 1740431995
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::NewProp_weather_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::NewProp_weather,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_AVolumetricCloudsActor, nullptr, "Server_SetWeather", nullptr, nullptr, Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::PropPointers), sizeof(VolumetricCloudsActor_eventServer_SetWeather_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04220CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::Function_MetaDataParams), Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::Function_MetaDataParams) };
static_assert(sizeof(VolumetricCloudsActor_eventServer_SetWeather_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(AVolumetricCloudsActor::execServer_SetWeather)
{
	P_GET_ENUM(EWeatherType,Z_Param_weather);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Server_SetWeather_Implementation(EWeatherType(Z_Param_weather));
	P_NATIVE_END;
}
// End Class AVolumetricCloudsActor Function Server_SetWeather

// Begin Class AVolumetricCloudsActor
void AVolumetricCloudsActor::StaticRegisterNativesAVolumetricCloudsActor()
{
	UClass* Class = AVolumetricCloudsActor::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "Server_SetWeather", &AVolumetricCloudsActor::execServer_SetWeather },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AVolumetricCloudsActor);
UClass* Z_Construct_UClass_AVolumetricCloudsActor_NoRegister()
{
	return AVolumetricCloudsActor::StaticClass();
}
struct Z_Construct_UClass_AVolumetricCloudsActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cloud_coverage_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cloud_density_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cloud_height_min_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cloud_height_max_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_detail_scale_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_detail_strength_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_erosion_scale_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_erosion_strength_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wind_direction_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wind_speed_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_turbulence_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ambient_color_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sun_scatter_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_powder_effect_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_march_steps_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_light_steps_MetaData[] = {
		{ "Category", "VolumetricCloudsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_cloud_coverage;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_cloud_density;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_cloud_height_min;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_cloud_height_max;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_detail_scale;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_detail_strength;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_erosion_scale;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_erosion_strength;
	static const UECodeGen_Private::FStructPropertyParams NewProp_wind_direction;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wind_speed;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_turbulence;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ambient_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_sun_scatter;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_powder_effect;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_march_steps;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_light_steps;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_AVolumetricCloudsActor_Server_SetWeather, "Server_SetWeather" }, // 1050010900
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AVolumetricCloudsActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_coverage = { "cloud_coverage", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, cloud_coverage), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cloud_coverage_MetaData), NewProp_cloud_coverage_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_density = { "cloud_density", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, cloud_density), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cloud_density_MetaData), NewProp_cloud_density_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_height_min = { "cloud_height_min", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, cloud_height_min), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cloud_height_min_MetaData), NewProp_cloud_height_min_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_height_max = { "cloud_height_max", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, cloud_height_max), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cloud_height_max_MetaData), NewProp_cloud_height_max_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_detail_scale = { "detail_scale", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, detail_scale), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_detail_scale_MetaData), NewProp_detail_scale_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_detail_strength = { "detail_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, detail_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_detail_strength_MetaData), NewProp_detail_strength_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_erosion_scale = { "erosion_scale", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, erosion_scale), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_erosion_scale_MetaData), NewProp_erosion_scale_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_erosion_strength = { "erosion_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, erosion_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_erosion_strength_MetaData), NewProp_erosion_strength_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_wind_direction = { "wind_direction", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, wind_direction), Z_Construct_UScriptStruct_FVector2D, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wind_direction_MetaData), NewProp_wind_direction_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_wind_speed = { "wind_speed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, wind_speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wind_speed_MetaData), NewProp_wind_speed_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_turbulence = { "turbulence", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, turbulence), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_turbulence_MetaData), NewProp_turbulence_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_ambient_color = { "ambient_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, ambient_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ambient_color_MetaData), NewProp_ambient_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_sun_scatter = { "sun_scatter", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, sun_scatter), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sun_scatter_MetaData), NewProp_sun_scatter_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_powder_effect = { "powder_effect", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, powder_effect), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_powder_effect_MetaData), NewProp_powder_effect_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_march_steps = { "march_steps", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, march_steps), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_march_steps_MetaData), NewProp_march_steps_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_light_steps = { "light_steps", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricCloudsActor, light_steps), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_light_steps_MetaData), NewProp_light_steps_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AVolumetricCloudsActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_coverage,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_density,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_height_min,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_cloud_height_max,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_detail_scale,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_detail_strength,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_erosion_scale,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_erosion_strength,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_wind_direction,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_wind_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_turbulence,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_ambient_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_sun_scatter,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_powder_effect,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_march_steps,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricCloudsActor_Statics::NewProp_light_steps,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricCloudsActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AVolumetricCloudsActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricCloudsActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AVolumetricCloudsActor_Statics::ClassParams = {
	&AVolumetricCloudsActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_AVolumetricCloudsActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricCloudsActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricCloudsActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AVolumetricCloudsActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AVolumetricCloudsActor()
{
	if (!Z_Registration_Info_UClass_AVolumetricCloudsActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AVolumetricCloudsActor.OuterSingleton, Z_Construct_UClass_AVolumetricCloudsActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AVolumetricCloudsActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AVolumetricCloudsActor>()
{
	return AVolumetricCloudsActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AVolumetricCloudsActor);
AVolumetricCloudsActor::~AVolumetricCloudsActor() {}
// End Class AVolumetricCloudsActor

// Begin Class AOceanRenderingActor
void AOceanRenderingActor::StaticRegisterNativesAOceanRenderingActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AOceanRenderingActor);
UClass* Z_Construct_UClass_AOceanRenderingActor_NoRegister()
{
	return AOceanRenderingActor::StaticClass();
}
struct Z_Construct_UClass_AOceanRenderingActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wave_amplitude_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wave_frequency_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wave_speed_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wave_choppiness_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wave_scale_1_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wave_scale_2_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_wave_scale_3_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_foam_threshold_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_foam_color_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_foam_decay_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_water_color_shallow_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_water_color_deep_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_scatter_strength_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_roughness_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_metallic_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_refraction_strength_MetaData[] = {
		{ "Category", "OceanRenderingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wave_amplitude;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wave_frequency;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wave_speed;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wave_choppiness;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wave_scale_1;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wave_scale_2;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_wave_scale_3;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_foam_threshold;
	static const UECodeGen_Private::FStructPropertyParams NewProp_foam_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_foam_decay;
	static const UECodeGen_Private::FStructPropertyParams NewProp_water_color_shallow;
	static const UECodeGen_Private::FStructPropertyParams NewProp_water_color_deep;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_scatter_strength;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_roughness;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_metallic;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_refraction_strength;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AOceanRenderingActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_amplitude = { "wave_amplitude", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, wave_amplitude), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wave_amplitude_MetaData), NewProp_wave_amplitude_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_frequency = { "wave_frequency", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, wave_frequency), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wave_frequency_MetaData), NewProp_wave_frequency_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_speed = { "wave_speed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, wave_speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wave_speed_MetaData), NewProp_wave_speed_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_choppiness = { "wave_choppiness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, wave_choppiness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wave_choppiness_MetaData), NewProp_wave_choppiness_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_scale_1 = { "wave_scale_1", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, wave_scale_1), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wave_scale_1_MetaData), NewProp_wave_scale_1_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_scale_2 = { "wave_scale_2", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, wave_scale_2), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wave_scale_2_MetaData), NewProp_wave_scale_2_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_scale_3 = { "wave_scale_3", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, wave_scale_3), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_wave_scale_3_MetaData), NewProp_wave_scale_3_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_foam_threshold = { "foam_threshold", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, foam_threshold), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_foam_threshold_MetaData), NewProp_foam_threshold_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_foam_color = { "foam_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, foam_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_foam_color_MetaData), NewProp_foam_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_foam_decay = { "foam_decay", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, foam_decay), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_foam_decay_MetaData), NewProp_foam_decay_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_water_color_shallow = { "water_color_shallow", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, water_color_shallow), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_water_color_shallow_MetaData), NewProp_water_color_shallow_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_water_color_deep = { "water_color_deep", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, water_color_deep), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_water_color_deep_MetaData), NewProp_water_color_deep_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_scatter_strength = { "scatter_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, scatter_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_scatter_strength_MetaData), NewProp_scatter_strength_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_roughness = { "roughness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, roughness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_roughness_MetaData), NewProp_roughness_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_metallic = { "metallic", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, metallic), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_metallic_MetaData), NewProp_metallic_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_refraction_strength = { "refraction_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AOceanRenderingActor, refraction_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_refraction_strength_MetaData), NewProp_refraction_strength_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AOceanRenderingActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_amplitude,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_frequency,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_choppiness,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_scale_1,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_scale_2,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_wave_scale_3,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_foam_threshold,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_foam_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_foam_decay,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_water_color_shallow,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_water_color_deep,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_scatter_strength,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_roughness,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_metallic,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AOceanRenderingActor_Statics::NewProp_refraction_strength,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AOceanRenderingActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AOceanRenderingActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AOceanRenderingActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AOceanRenderingActor_Statics::ClassParams = {
	&AOceanRenderingActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AOceanRenderingActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AOceanRenderingActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AOceanRenderingActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AOceanRenderingActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AOceanRenderingActor()
{
	if (!Z_Registration_Info_UClass_AOceanRenderingActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AOceanRenderingActor.OuterSingleton, Z_Construct_UClass_AOceanRenderingActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AOceanRenderingActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AOceanRenderingActor>()
{
	return AOceanRenderingActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AOceanRenderingActor);
AOceanRenderingActor::~AOceanRenderingActor() {}
// End Class AOceanRenderingActor

// Begin Class AVolumetricFogActor
void AVolumetricFogActor::StaticRegisterNativesAVolumetricFogActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AVolumetricFogActor);
UClass* Z_Construct_UClass_AVolumetricFogActor_NoRegister()
{
	return AVolumetricFogActor::StaticClass();
}
struct Z_Construct_UClass_AVolumetricFogActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_fog_density_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_fog_height_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_fog_falloff_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_fog_color_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_absorption_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_scattering_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_anisotropy_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_noise_scale_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_animation_speed_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ray_march_steps_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_light_march_steps_MetaData[] = {
		{ "Category", "VolumetricFogActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_fog_density;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_fog_height;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_fog_falloff;
	static const UECodeGen_Private::FStructPropertyParams NewProp_fog_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_absorption;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_scattering;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_anisotropy;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_noise_scale;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_animation_speed;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ray_march_steps;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_light_march_steps;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AVolumetricFogActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_density = { "fog_density", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, fog_density), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_fog_density_MetaData), NewProp_fog_density_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_height = { "fog_height", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, fog_height), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_fog_height_MetaData), NewProp_fog_height_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_falloff = { "fog_falloff", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, fog_falloff), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_fog_falloff_MetaData), NewProp_fog_falloff_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_color = { "fog_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, fog_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_fog_color_MetaData), NewProp_fog_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_absorption = { "absorption", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, absorption), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_absorption_MetaData), NewProp_absorption_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_scattering = { "scattering", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, scattering), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_scattering_MetaData), NewProp_scattering_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_anisotropy = { "anisotropy", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, anisotropy), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_anisotropy_MetaData), NewProp_anisotropy_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_noise_scale = { "noise_scale", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, noise_scale), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_noise_scale_MetaData), NewProp_noise_scale_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_animation_speed = { "animation_speed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, animation_speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_animation_speed_MetaData), NewProp_animation_speed_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_ray_march_steps = { "ray_march_steps", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, ray_march_steps), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ray_march_steps_MetaData), NewProp_ray_march_steps_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_light_march_steps = { "light_march_steps", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVolumetricFogActor, light_march_steps), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_light_march_steps_MetaData), NewProp_light_march_steps_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AVolumetricFogActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_density,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_height,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_falloff,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_fog_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_absorption,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_scattering,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_anisotropy,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_noise_scale,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_animation_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_ray_march_steps,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVolumetricFogActor_Statics::NewProp_light_march_steps,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricFogActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AVolumetricFogActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricFogActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AVolumetricFogActor_Statics::ClassParams = {
	&AVolumetricFogActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AVolumetricFogActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricFogActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AVolumetricFogActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AVolumetricFogActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AVolumetricFogActor()
{
	if (!Z_Registration_Info_UClass_AVolumetricFogActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AVolumetricFogActor.OuterSingleton, Z_Construct_UClass_AVolumetricFogActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AVolumetricFogActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AVolumetricFogActor>()
{
	return AVolumetricFogActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AVolumetricFogActor);
AVolumetricFogActor::~AVolumetricFogActor() {}
// End Class AVolumetricFogActor

// Begin Class AGodRaysActor
void AGodRaysActor::StaticRegisterNativesAGodRaysActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AGodRaysActor);
UClass* Z_Construct_UClass_AGodRaysActor_NoRegister()
{
	return AGodRaysActor::StaticClass();
}
struct Z_Construct_UClass_AGodRaysActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_light_position_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_light_color_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_light_intensity_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ray_density_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ray_weight_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ray_decay_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ray_exposure_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_num_samples_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_dither_strength_MetaData[] = {
		{ "Category", "GodRaysActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_light_position;
	static const UECodeGen_Private::FStructPropertyParams NewProp_light_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_light_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ray_density;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ray_weight;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ray_decay;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ray_exposure;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_num_samples;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_dither_strength;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AGodRaysActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_light_position = { "light_position", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, light_position), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_light_position_MetaData), NewProp_light_position_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_light_color = { "light_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, light_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_light_color_MetaData), NewProp_light_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_light_intensity = { "light_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, light_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_light_intensity_MetaData), NewProp_light_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_density = { "ray_density", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, ray_density), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ray_density_MetaData), NewProp_ray_density_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_weight = { "ray_weight", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, ray_weight), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ray_weight_MetaData), NewProp_ray_weight_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_decay = { "ray_decay", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, ray_decay), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ray_decay_MetaData), NewProp_ray_decay_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_exposure = { "ray_exposure", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, ray_exposure), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ray_exposure_MetaData), NewProp_ray_exposure_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_num_samples = { "num_samples", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, num_samples), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_num_samples_MetaData), NewProp_num_samples_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AGodRaysActor_Statics::NewProp_dither_strength = { "dither_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AGodRaysActor, dither_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_dither_strength_MetaData), NewProp_dither_strength_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AGodRaysActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_light_position,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_light_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_light_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_density,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_weight,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_decay,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_ray_exposure,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_num_samples,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AGodRaysActor_Statics::NewProp_dither_strength,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AGodRaysActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AGodRaysActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AGodRaysActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AGodRaysActor_Statics::ClassParams = {
	&AGodRaysActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AGodRaysActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AGodRaysActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AGodRaysActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AGodRaysActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AGodRaysActor()
{
	if (!Z_Registration_Info_UClass_AGodRaysActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AGodRaysActor.OuterSingleton, Z_Construct_UClass_AGodRaysActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AGodRaysActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AGodRaysActor>()
{
	return AGodRaysActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AGodRaysActor);
AGodRaysActor::~AGodRaysActor() {}
// End Class AGodRaysActor

// Begin Class ABloomLensFlareActor
void ABloomLensFlareActor::StaticRegisterNativesABloomLensFlareActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ABloomLensFlareActor);
UClass* Z_Construct_UClass_ABloomLensFlareActor_NoRegister()
{
	return ABloomLensFlareActor::StaticClass();
}
struct Z_Construct_UClass_ABloomLensFlareActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bloom_threshold_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bloom_intensity_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bloom_radius_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bloom_softness_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_lens_flare_intensity_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_lens_flare_ghosts_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_lens_flare_halo_radius_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_lens_flare_halo_thickness_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_chromatic_aberration_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_lens_dirt_intensity_MetaData[] = {
		{ "Category", "BloomLensFlareActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bloom_threshold;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bloom_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bloom_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bloom_softness;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_lens_flare_intensity;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_lens_flare_ghosts;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_lens_flare_halo_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_lens_flare_halo_thickness;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_chromatic_aberration;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_lens_dirt_intensity;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ABloomLensFlareActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_threshold = { "bloom_threshold", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, bloom_threshold), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bloom_threshold_MetaData), NewProp_bloom_threshold_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_intensity = { "bloom_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, bloom_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bloom_intensity_MetaData), NewProp_bloom_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_radius = { "bloom_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, bloom_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bloom_radius_MetaData), NewProp_bloom_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_softness = { "bloom_softness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, bloom_softness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bloom_softness_MetaData), NewProp_bloom_softness_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_intensity = { "lens_flare_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, lens_flare_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_lens_flare_intensity_MetaData), NewProp_lens_flare_intensity_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_ghosts = { "lens_flare_ghosts", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, lens_flare_ghosts), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_lens_flare_ghosts_MetaData), NewProp_lens_flare_ghosts_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_halo_radius = { "lens_flare_halo_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, lens_flare_halo_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_lens_flare_halo_radius_MetaData), NewProp_lens_flare_halo_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_halo_thickness = { "lens_flare_halo_thickness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, lens_flare_halo_thickness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_lens_flare_halo_thickness_MetaData), NewProp_lens_flare_halo_thickness_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_chromatic_aberration = { "chromatic_aberration", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, chromatic_aberration), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_chromatic_aberration_MetaData), NewProp_chromatic_aberration_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_dirt_intensity = { "lens_dirt_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABloomLensFlareActor, lens_dirt_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_lens_dirt_intensity_MetaData), NewProp_lens_dirt_intensity_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ABloomLensFlareActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_threshold,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_bloom_softness,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_ghosts,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_halo_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_flare_halo_thickness,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_chromatic_aberration,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABloomLensFlareActor_Statics::NewProp_lens_dirt_intensity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ABloomLensFlareActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_ABloomLensFlareActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ABloomLensFlareActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_ABloomLensFlareActor_Statics::ClassParams = {
	&ABloomLensFlareActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_ABloomLensFlareActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_ABloomLensFlareActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_ABloomLensFlareActor_Statics::Class_MetaDataParams), Z_Construct_UClass_ABloomLensFlareActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_ABloomLensFlareActor()
{
	if (!Z_Registration_Info_UClass_ABloomLensFlareActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_ABloomLensFlareActor.OuterSingleton, Z_Construct_UClass_ABloomLensFlareActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_ABloomLensFlareActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<ABloomLensFlareActor>()
{
	return ABloomLensFlareActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(ABloomLensFlareActor);
ABloomLensFlareActor::~ABloomLensFlareActor() {}
// End Class ABloomLensFlareActor

// Begin Class AScreenSpaceReflectionsActor
void AScreenSpaceReflectionsActor::StaticRegisterNativesAScreenSpaceReflectionsActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AScreenSpaceReflectionsActor);
UClass* Z_Construct_UClass_AScreenSpaceReflectionsActor_NoRegister()
{
	return AScreenSpaceReflectionsActor::StaticClass();
}
struct Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ssr_intensity_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ssr_quality_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_max_ray_distance_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_stride_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_thickness_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_jitter_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_max_steps_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_binary_search_steps_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_fade_start_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_fade_end_MetaData[] = {
		{ "Category", "ScreenSpaceReflectionsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ssr_intensity;
	static const UECodeGen_Private::FBytePropertyParams NewProp_ssr_quality_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ssr_quality;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_max_ray_distance;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_stride;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_thickness;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_jitter;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_max_steps;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_binary_search_steps;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_fade_start;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_fade_end;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AScreenSpaceReflectionsActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_ssr_intensity = { "ssr_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, ssr_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ssr_intensity_MetaData), NewProp_ssr_intensity_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_ssr_quality_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_ssr_quality = { "ssr_quality", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, ssr_quality), Z_Construct_UEnum_UltimateVFX_EEffectQuality, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ssr_quality_MetaData), NewProp_ssr_quality_MetaData) }; // 4088020921
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_max_ray_distance = { "max_ray_distance", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, max_ray_distance), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_max_ray_distance_MetaData), NewProp_max_ray_distance_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_stride = { "stride", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, stride), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_stride_MetaData), NewProp_stride_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_thickness = { "thickness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, thickness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_thickness_MetaData), NewProp_thickness_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_jitter = { "jitter", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, jitter), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_jitter_MetaData), NewProp_jitter_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_max_steps = { "max_steps", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, max_steps), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_max_steps_MetaData), NewProp_max_steps_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_binary_search_steps = { "binary_search_steps", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, binary_search_steps), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_binary_search_steps_MetaData), NewProp_binary_search_steps_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_fade_start = { "fade_start", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, fade_start), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_fade_start_MetaData), NewProp_fade_start_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_fade_end = { "fade_end", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AScreenSpaceReflectionsActor, fade_end), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_fade_end_MetaData), NewProp_fade_end_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_ssr_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_ssr_quality_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_ssr_quality,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_max_ray_distance,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_stride,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_thickness,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_jitter,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_max_steps,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_binary_search_steps,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_fade_start,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::NewProp_fade_end,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::ClassParams = {
	&AScreenSpaceReflectionsActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AScreenSpaceReflectionsActor()
{
	if (!Z_Registration_Info_UClass_AScreenSpaceReflectionsActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AScreenSpaceReflectionsActor.OuterSingleton, Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AScreenSpaceReflectionsActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AScreenSpaceReflectionsActor>()
{
	return AScreenSpaceReflectionsActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AScreenSpaceReflectionsActor);
AScreenSpaceReflectionsActor::~AScreenSpaceReflectionsActor() {}
// End Class AScreenSpaceReflectionsActor

// Begin Class AAmbientOcclusionActor
void AAmbientOcclusionActor::StaticRegisterNativesAAmbientOcclusionActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AAmbientOcclusionActor);
UClass* Z_Construct_UClass_AAmbientOcclusionActor_NoRegister()
{
	return AAmbientOcclusionActor::StaticClass();
}
struct Z_Construct_UClass_AAmbientOcclusionActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_intensity_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_radius_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_bias_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_samples_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_spiral_turns_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_blur_radius_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_sharpness_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ao_power_MetaData[] = {
		{ "Category", "AmbientOcclusionActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ao_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ao_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ao_bias;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ao_samples;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ao_spiral_turns;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ao_blur_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ao_sharpness;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ao_power;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AAmbientOcclusionActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_intensity = { "ao_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_intensity_MetaData), NewProp_ao_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_radius = { "ao_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_radius_MetaData), NewProp_ao_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_bias = { "ao_bias", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_bias), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_bias_MetaData), NewProp_ao_bias_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_samples = { "ao_samples", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_samples), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_samples_MetaData), NewProp_ao_samples_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_spiral_turns = { "ao_spiral_turns", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_spiral_turns), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_spiral_turns_MetaData), NewProp_ao_spiral_turns_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_blur_radius = { "ao_blur_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_blur_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_blur_radius_MetaData), NewProp_ao_blur_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_sharpness = { "ao_sharpness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_sharpness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_sharpness_MetaData), NewProp_ao_sharpness_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_power = { "ao_power", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AAmbientOcclusionActor, ao_power), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ao_power_MetaData), NewProp_ao_power_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AAmbientOcclusionActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_bias,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_samples,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_spiral_turns,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_blur_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_sharpness,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AAmbientOcclusionActor_Statics::NewProp_ao_power,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AAmbientOcclusionActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AAmbientOcclusionActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AAmbientOcclusionActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AAmbientOcclusionActor_Statics::ClassParams = {
	&AAmbientOcclusionActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AAmbientOcclusionActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AAmbientOcclusionActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AAmbientOcclusionActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AAmbientOcclusionActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AAmbientOcclusionActor()
{
	if (!Z_Registration_Info_UClass_AAmbientOcclusionActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AAmbientOcclusionActor.OuterSingleton, Z_Construct_UClass_AAmbientOcclusionActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AAmbientOcclusionActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AAmbientOcclusionActor>()
{
	return AAmbientOcclusionActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AAmbientOcclusionActor);
AAmbientOcclusionActor::~AAmbientOcclusionActor() {}
// End Class AAmbientOcclusionActor

// Begin Class ADepthOfFieldActor
void ADepthOfFieldActor::StaticRegisterNativesADepthOfFieldActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ADepthOfFieldActor);
UClass* Z_Construct_UClass_ADepthOfFieldActor_NoRegister()
{
	return ADepthOfFieldActor::StaticClass();
}
struct Z_Construct_UClass_ADepthOfFieldActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_focus_distance_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_focus_range_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_aperture_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_focal_length_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bokeh_shape_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bokeh_rotation_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bokeh_scale_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_coc_scale_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_max_blur_size_MetaData[] = {
		{ "Category", "DepthOfFieldActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_focus_distance;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_focus_range;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_aperture;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_focal_length;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_bokeh_shape;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bokeh_rotation;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_bokeh_scale;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_coc_scale;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_max_blur_size;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ADepthOfFieldActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_focus_distance = { "focus_distance", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, focus_distance), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_focus_distance_MetaData), NewProp_focus_distance_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_focus_range = { "focus_range", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, focus_range), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_focus_range_MetaData), NewProp_focus_range_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_aperture = { "aperture", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, aperture), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_aperture_MetaData), NewProp_aperture_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_focal_length = { "focal_length", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, focal_length), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_focal_length_MetaData), NewProp_focal_length_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_bokeh_shape = { "bokeh_shape", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, bokeh_shape), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bokeh_shape_MetaData), NewProp_bokeh_shape_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_bokeh_rotation = { "bokeh_rotation", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, bokeh_rotation), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bokeh_rotation_MetaData), NewProp_bokeh_rotation_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_bokeh_scale = { "bokeh_scale", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, bokeh_scale), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bokeh_scale_MetaData), NewProp_bokeh_scale_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_coc_scale = { "coc_scale", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, coc_scale), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_coc_scale_MetaData), NewProp_coc_scale_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_max_blur_size = { "max_blur_size", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ADepthOfFieldActor, max_blur_size), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_max_blur_size_MetaData), NewProp_max_blur_size_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ADepthOfFieldActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_focus_distance,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_focus_range,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_aperture,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_focal_length,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_bokeh_shape,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_bokeh_rotation,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_bokeh_scale,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_coc_scale,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ADepthOfFieldActor_Statics::NewProp_max_blur_size,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ADepthOfFieldActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_ADepthOfFieldActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ADepthOfFieldActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_ADepthOfFieldActor_Statics::ClassParams = {
	&ADepthOfFieldActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_ADepthOfFieldActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_ADepthOfFieldActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_ADepthOfFieldActor_Statics::Class_MetaDataParams), Z_Construct_UClass_ADepthOfFieldActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_ADepthOfFieldActor()
{
	if (!Z_Registration_Info_UClass_ADepthOfFieldActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_ADepthOfFieldActor.OuterSingleton, Z_Construct_UClass_ADepthOfFieldActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_ADepthOfFieldActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<ADepthOfFieldActor>()
{
	return ADepthOfFieldActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(ADepthOfFieldActor);
ADepthOfFieldActor::~ADepthOfFieldActor() {}
// End Class ADepthOfFieldActor

// Begin Class AMotionBlurActor
void AMotionBlurActor::StaticRegisterNativesAMotionBlurActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AMotionBlurActor);
UClass* Z_Construct_UClass_AMotionBlurActor_NoRegister()
{
	return AMotionBlurActor::StaticClass();
}
struct Z_Construct_UClass_AMotionBlurActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_blur_intensity_MetaData[] = {
		{ "Category", "MotionBlurActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_blur_samples_MetaData[] = {
		{ "Category", "MotionBlurActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_velocity_scale_MetaData[] = {
		{ "Category", "MotionBlurActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_max_blur_radius_MetaData[] = {
		{ "Category", "MotionBlurActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_center_fade_MetaData[] = {
		{ "Category", "MotionBlurActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_blur_intensity;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_blur_samples;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_velocity_scale;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_max_blur_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_center_fade;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AMotionBlurActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_blur_intensity = { "blur_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AMotionBlurActor, blur_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_blur_intensity_MetaData), NewProp_blur_intensity_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_blur_samples = { "blur_samples", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AMotionBlurActor, blur_samples), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_blur_samples_MetaData), NewProp_blur_samples_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_velocity_scale = { "velocity_scale", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AMotionBlurActor, velocity_scale), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_velocity_scale_MetaData), NewProp_velocity_scale_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_max_blur_radius = { "max_blur_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AMotionBlurActor, max_blur_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_max_blur_radius_MetaData), NewProp_max_blur_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_center_fade = { "center_fade", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AMotionBlurActor, center_fade), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_center_fade_MetaData), NewProp_center_fade_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AMotionBlurActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_blur_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_blur_samples,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_velocity_scale,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_max_blur_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AMotionBlurActor_Statics::NewProp_center_fade,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AMotionBlurActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AMotionBlurActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AMotionBlurActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AMotionBlurActor_Statics::ClassParams = {
	&AMotionBlurActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AMotionBlurActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AMotionBlurActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AMotionBlurActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AMotionBlurActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AMotionBlurActor()
{
	if (!Z_Registration_Info_UClass_AMotionBlurActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AMotionBlurActor.OuterSingleton, Z_Construct_UClass_AMotionBlurActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AMotionBlurActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AMotionBlurActor>()
{
	return AMotionBlurActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AMotionBlurActor);
AMotionBlurActor::~AMotionBlurActor() {}
// End Class AMotionBlurActor

// Begin Class AColorGradingActor
void AColorGradingActor::StaticRegisterNativesAColorGradingActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AColorGradingActor);
UClass* Z_Construct_UClass_AColorGradingActor_NoRegister()
{
	return AColorGradingActor::StaticClass();
}
struct Z_Construct_UClass_AColorGradingActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_temperature_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_tint_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_contrast_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_saturation_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_gamma_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_gain_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_offset_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_shadows_color_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_midtones_color_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_highlights_color_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_vignette_intensity_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_vignette_smoothness_MetaData[] = {
		{ "Category", "ColorGradingActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_temperature;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_tint;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_contrast;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_saturation;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_gamma;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_gain;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_offset;
	static const UECodeGen_Private::FStructPropertyParams NewProp_shadows_color;
	static const UECodeGen_Private::FStructPropertyParams NewProp_midtones_color;
	static const UECodeGen_Private::FStructPropertyParams NewProp_highlights_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_vignette_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_vignette_smoothness;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AColorGradingActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_temperature = { "temperature", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, temperature), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_temperature_MetaData), NewProp_temperature_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_tint = { "tint", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, tint), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_tint_MetaData), NewProp_tint_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_contrast = { "contrast", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, contrast), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_contrast_MetaData), NewProp_contrast_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_saturation = { "saturation", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, saturation), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_saturation_MetaData), NewProp_saturation_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_gamma = { "gamma", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, gamma), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_gamma_MetaData), NewProp_gamma_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_gain = { "gain", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, gain), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_gain_MetaData), NewProp_gain_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_offset = { "offset", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, offset), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_offset_MetaData), NewProp_offset_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_shadows_color = { "shadows_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, shadows_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_shadows_color_MetaData), NewProp_shadows_color_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_midtones_color = { "midtones_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, midtones_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_midtones_color_MetaData), NewProp_midtones_color_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_highlights_color = { "highlights_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, highlights_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_highlights_color_MetaData), NewProp_highlights_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_vignette_intensity = { "vignette_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, vignette_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_vignette_intensity_MetaData), NewProp_vignette_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AColorGradingActor_Statics::NewProp_vignette_smoothness = { "vignette_smoothness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AColorGradingActor, vignette_smoothness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_vignette_smoothness_MetaData), NewProp_vignette_smoothness_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AColorGradingActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_temperature,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_tint,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_contrast,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_saturation,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_gamma,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_gain,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_offset,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_shadows_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_midtones_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_highlights_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_vignette_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AColorGradingActor_Statics::NewProp_vignette_smoothness,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AColorGradingActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AColorGradingActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AColorGradingActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AColorGradingActor_Statics::ClassParams = {
	&AColorGradingActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AColorGradingActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AColorGradingActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AColorGradingActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AColorGradingActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AColorGradingActor()
{
	if (!Z_Registration_Info_UClass_AColorGradingActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AColorGradingActor.OuterSingleton, Z_Construct_UClass_AColorGradingActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AColorGradingActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AColorGradingActor>()
{
	return AColorGradingActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AColorGradingActor);
AColorGradingActor::~AColorGradingActor() {}
// End Class AColorGradingActor

// Begin Class AChromaticAberrationActor
void AChromaticAberrationActor::StaticRegisterNativesAChromaticAberrationActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AChromaticAberrationActor);
UClass* Z_Construct_UClass_AChromaticAberrationActor_NoRegister()
{
	return AChromaticAberrationActor::StaticClass();
}
struct Z_Construct_UClass_AChromaticAberrationActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_aberration_intensity_MetaData[] = {
		{ "Category", "ChromaticAberrationActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_aberration_offset_MetaData[] = {
		{ "Category", "ChromaticAberrationActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_radial_distortion_MetaData[] = {
		{ "Category", "ChromaticAberrationActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_barrel_distortion_MetaData[] = {
		{ "Category", "ChromaticAberrationActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_aberration_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_aberration_offset;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_radial_distortion;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_barrel_distortion;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AChromaticAberrationActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_aberration_intensity = { "aberration_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChromaticAberrationActor, aberration_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_aberration_intensity_MetaData), NewProp_aberration_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_aberration_offset = { "aberration_offset", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChromaticAberrationActor, aberration_offset), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_aberration_offset_MetaData), NewProp_aberration_offset_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_radial_distortion = { "radial_distortion", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChromaticAberrationActor, radial_distortion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_radial_distortion_MetaData), NewProp_radial_distortion_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_barrel_distortion = { "barrel_distortion", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChromaticAberrationActor, barrel_distortion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_barrel_distortion_MetaData), NewProp_barrel_distortion_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AChromaticAberrationActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_aberration_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_aberration_offset,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_radial_distortion,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChromaticAberrationActor_Statics::NewProp_barrel_distortion,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AChromaticAberrationActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AChromaticAberrationActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AChromaticAberrationActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AChromaticAberrationActor_Statics::ClassParams = {
	&AChromaticAberrationActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AChromaticAberrationActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AChromaticAberrationActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AChromaticAberrationActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AChromaticAberrationActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AChromaticAberrationActor()
{
	if (!Z_Registration_Info_UClass_AChromaticAberrationActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AChromaticAberrationActor.OuterSingleton, Z_Construct_UClass_AChromaticAberrationActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AChromaticAberrationActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AChromaticAberrationActor>()
{
	return AChromaticAberrationActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AChromaticAberrationActor);
AChromaticAberrationActor::~AChromaticAberrationActor() {}
// End Class AChromaticAberrationActor

// Begin Class AFilmGrainActor
void AFilmGrainActor::StaticRegisterNativesAFilmGrainActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AFilmGrainActor);
UClass* Z_Construct_UClass_AFilmGrainActor_NoRegister()
{
	return AFilmGrainActor::StaticClass();
}
struct Z_Construct_UClass_AFilmGrainActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_grain_intensity_MetaData[] = {
		{ "Category", "FilmGrainActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_grain_size_MetaData[] = {
		{ "Category", "FilmGrainActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_grain_luminance_contribution_MetaData[] = {
		{ "Category", "FilmGrainActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_grain_color_contribution_MetaData[] = {
		{ "Category", "FilmGrainActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_grain_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_grain_size;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_grain_luminance_contribution;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_grain_color_contribution;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AFilmGrainActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_intensity = { "grain_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AFilmGrainActor, grain_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_grain_intensity_MetaData), NewProp_grain_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_size = { "grain_size", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AFilmGrainActor, grain_size), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_grain_size_MetaData), NewProp_grain_size_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_luminance_contribution = { "grain_luminance_contribution", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AFilmGrainActor, grain_luminance_contribution), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_grain_luminance_contribution_MetaData), NewProp_grain_luminance_contribution_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_color_contribution = { "grain_color_contribution", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AFilmGrainActor, grain_color_contribution), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_grain_color_contribution_MetaData), NewProp_grain_color_contribution_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AFilmGrainActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_size,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_luminance_contribution,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AFilmGrainActor_Statics::NewProp_grain_color_contribution,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AFilmGrainActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AFilmGrainActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AFilmGrainActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AFilmGrainActor_Statics::ClassParams = {
	&AFilmGrainActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AFilmGrainActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AFilmGrainActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AFilmGrainActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AFilmGrainActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AFilmGrainActor()
{
	if (!Z_Registration_Info_UClass_AFilmGrainActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AFilmGrainActor.OuterSingleton, Z_Construct_UClass_AFilmGrainActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AFilmGrainActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AFilmGrainActor>()
{
	return AFilmGrainActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AFilmGrainActor);
AFilmGrainActor::~AFilmGrainActor() {}
// End Class AFilmGrainActor

// Begin Class ASharpenActor
void ASharpenActor::StaticRegisterNativesASharpenActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ASharpenActor);
UClass* Z_Construct_UClass_ASharpenActor_NoRegister()
{
	return ASharpenActor::StaticClass();
}
struct Z_Construct_UClass_ASharpenActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sharpen_intensity_MetaData[] = {
		{ "Category", "SharpenActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sharpen_radius_MetaData[] = {
		{ "Category", "SharpenActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sharpen_threshold_MetaData[] = {
		{ "Category", "SharpenActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_sharpen_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_sharpen_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_sharpen_threshold;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ASharpenActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ASharpenActor_Statics::NewProp_sharpen_intensity = { "sharpen_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ASharpenActor, sharpen_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sharpen_intensity_MetaData), NewProp_sharpen_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ASharpenActor_Statics::NewProp_sharpen_radius = { "sharpen_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ASharpenActor, sharpen_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sharpen_radius_MetaData), NewProp_sharpen_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ASharpenActor_Statics::NewProp_sharpen_threshold = { "sharpen_threshold", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ASharpenActor, sharpen_threshold), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sharpen_threshold_MetaData), NewProp_sharpen_threshold_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ASharpenActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASharpenActor_Statics::NewProp_sharpen_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASharpenActor_Statics::NewProp_sharpen_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASharpenActor_Statics::NewProp_sharpen_threshold,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ASharpenActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_ASharpenActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ASharpenActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_ASharpenActor_Statics::ClassParams = {
	&ASharpenActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_ASharpenActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_ASharpenActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_ASharpenActor_Statics::Class_MetaDataParams), Z_Construct_UClass_ASharpenActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_ASharpenActor()
{
	if (!Z_Registration_Info_UClass_ASharpenActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_ASharpenActor.OuterSingleton, Z_Construct_UClass_ASharpenActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_ASharpenActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<ASharpenActor>()
{
	return ASharpenActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(ASharpenActor);
ASharpenActor::~ASharpenActor() {}
// End Class ASharpenActor

// Begin Class ARainDropsActor
void ARainDropsActor::StaticRegisterNativesARainDropsActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ARainDropsActor);
UClass* Z_Construct_UClass_ARainDropsActor_NoRegister()
{
	return ARainDropsActor::StaticClass();
}
struct Z_Construct_UClass_ARainDropsActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_rain_intensity_MetaData[] = {
		{ "Category", "RainDropsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_drop_size_MetaData[] = {
		{ "Category", "RainDropsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_drop_speed_MetaData[] = {
		{ "Category", "RainDropsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_drop_density_MetaData[] = {
		{ "Category", "RainDropsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ripple_strength_MetaData[] = {
		{ "Category", "RainDropsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_blur_strength_MetaData[] = {
		{ "Category", "RainDropsActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_rain_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_drop_size;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_drop_speed;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_drop_density;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ripple_strength;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_blur_strength;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ARainDropsActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ARainDropsActor_Statics::NewProp_rain_intensity = { "rain_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ARainDropsActor, rain_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_rain_intensity_MetaData), NewProp_rain_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ARainDropsActor_Statics::NewProp_drop_size = { "drop_size", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ARainDropsActor, drop_size), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_drop_size_MetaData), NewProp_drop_size_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ARainDropsActor_Statics::NewProp_drop_speed = { "drop_speed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ARainDropsActor, drop_speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_drop_speed_MetaData), NewProp_drop_speed_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ARainDropsActor_Statics::NewProp_drop_density = { "drop_density", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ARainDropsActor, drop_density), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_drop_density_MetaData), NewProp_drop_density_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ARainDropsActor_Statics::NewProp_ripple_strength = { "ripple_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ARainDropsActor, ripple_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ripple_strength_MetaData), NewProp_ripple_strength_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_ARainDropsActor_Statics::NewProp_blur_strength = { "blur_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ARainDropsActor, blur_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_blur_strength_MetaData), NewProp_blur_strength_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ARainDropsActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ARainDropsActor_Statics::NewProp_rain_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ARainDropsActor_Statics::NewProp_drop_size,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ARainDropsActor_Statics::NewProp_drop_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ARainDropsActor_Statics::NewProp_drop_density,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ARainDropsActor_Statics::NewProp_ripple_strength,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ARainDropsActor_Statics::NewProp_blur_strength,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ARainDropsActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_ARainDropsActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ARainDropsActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_ARainDropsActor_Statics::ClassParams = {
	&ARainDropsActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_ARainDropsActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_ARainDropsActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_ARainDropsActor_Statics::Class_MetaDataParams), Z_Construct_UClass_ARainDropsActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_ARainDropsActor()
{
	if (!Z_Registration_Info_UClass_ARainDropsActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_ARainDropsActor.OuterSingleton, Z_Construct_UClass_ARainDropsActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_ARainDropsActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<ARainDropsActor>()
{
	return ARainDropsActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(ARainDropsActor);
ARainDropsActor::~ARainDropsActor() {}
// End Class ARainDropsActor

// Begin Class AProceduralSkyActor
void AProceduralSkyActor::StaticRegisterNativesAProceduralSkyActor()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AProceduralSkyActor);
UClass* Z_Construct_UClass_AProceduralSkyActor_NoRegister()
{
	return AProceduralSkyActor::StaticClass();
}
struct Z_Construct_UClass_AProceduralSkyActor_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFX.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sky_color_zenith_MetaData[] = {
		{ "Category", "ProceduralSkyActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sky_color_horizon_MetaData[] = {
		{ "Category", "ProceduralSkyActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ground_color_MetaData[] = {
		{ "Category", "ProceduralSkyActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sun_disk_size_MetaData[] = {
		{ "Category", "ProceduralSkyActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sun_disk_intensity_MetaData[] = {
		{ "Category", "ProceduralSkyActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_stars_intensity_MetaData[] = {
		{ "Category", "ProceduralSkyActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_milky_way_intensity_MetaData[] = {
		{ "Category", "ProceduralSkyActor" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_sky_color_zenith;
	static const UECodeGen_Private::FStructPropertyParams NewProp_sky_color_horizon;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ground_color;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_sun_disk_size;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_sun_disk_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_stars_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_milky_way_intensity;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AProceduralSkyActor>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sky_color_zenith = { "sky_color_zenith", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AProceduralSkyActor, sky_color_zenith), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sky_color_zenith_MetaData), NewProp_sky_color_zenith_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sky_color_horizon = { "sky_color_horizon", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AProceduralSkyActor, sky_color_horizon), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sky_color_horizon_MetaData), NewProp_sky_color_horizon_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_ground_color = { "ground_color", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AProceduralSkyActor, ground_color), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ground_color_MetaData), NewProp_ground_color_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sun_disk_size = { "sun_disk_size", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AProceduralSkyActor, sun_disk_size), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sun_disk_size_MetaData), NewProp_sun_disk_size_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sun_disk_intensity = { "sun_disk_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AProceduralSkyActor, sun_disk_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sun_disk_intensity_MetaData), NewProp_sun_disk_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_stars_intensity = { "stars_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AProceduralSkyActor, stars_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_stars_intensity_MetaData), NewProp_stars_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_milky_way_intensity = { "milky_way_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AProceduralSkyActor, milky_way_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_milky_way_intensity_MetaData), NewProp_milky_way_intensity_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AProceduralSkyActor_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sky_color_zenith,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sky_color_horizon,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_ground_color,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sun_disk_size,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_sun_disk_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_stars_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AProceduralSkyActor_Statics::NewProp_milky_way_intensity,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AProceduralSkyActor_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AProceduralSkyActor_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AProceduralSkyActor_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AProceduralSkyActor_Statics::ClassParams = {
	&AProceduralSkyActor::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AProceduralSkyActor_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AProceduralSkyActor_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AProceduralSkyActor_Statics::Class_MetaDataParams), Z_Construct_UClass_AProceduralSkyActor_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AProceduralSkyActor()
{
	if (!Z_Registration_Info_UClass_AProceduralSkyActor.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AProceduralSkyActor.OuterSingleton, Z_Construct_UClass_AProceduralSkyActor_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AProceduralSkyActor.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<AProceduralSkyActor>()
{
	return AProceduralSkyActor::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AProceduralSkyActor);
AProceduralSkyActor::~AProceduralSkyActor() {}
// End Class AProceduralSkyActor

// Begin Class UUltimateVFXFunctionLibrary Function calculate_sun_color
struct Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics
{
	struct UltimateVFXFunctionLibrary_eventcalculate_sun_color_Parms
	{
		ETimeOfDay time;
		FVector ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_time_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_time_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_time;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::NewProp_time_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::NewProp_time = { "time", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventcalculate_sun_color_Parms, time), Z_Construct_UEnum_UltimateVFX_ETimeOfDay, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_time_MetaData), NewProp_time_MetaData) }; // 3512541502
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventcalculate_sun_color_Parms, ReturnValue), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::NewProp_time_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::NewProp_time,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UUltimateVFXFunctionLibrary, nullptr, "calculate_sun_color", nullptr, nullptr, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::PropPointers), sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::UltimateVFXFunctionLibrary_eventcalculate_sun_color_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04822401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::Function_MetaDataParams), Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::UltimateVFXFunctionLibrary_eventcalculate_sun_color_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UUltimateVFXFunctionLibrary::execcalculate_sun_color)
{
	P_GET_ENUM(ETimeOfDay,Z_Param_time);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FVector*)Z_Param__Result=UUltimateVFXFunctionLibrary::calculate_sun_color(ETimeOfDay(Z_Param_time));
	P_NATIVE_END;
}
// End Class UUltimateVFXFunctionLibrary Function calculate_sun_color

// Begin Class UUltimateVFXFunctionLibrary Function get_atmosphere_preset_colors
struct Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics
{
	struct UltimateVFXFunctionLibrary_eventget_atmosphere_preset_colors_Parms
	{
		EAtmospherePreset preset;
		FVector ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_preset_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_preset_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_preset;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::NewProp_preset_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::NewProp_preset = { "preset", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_atmosphere_preset_colors_Parms, preset), Z_Construct_UEnum_UltimateVFX_EAtmospherePreset, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_preset_MetaData), NewProp_preset_MetaData) }; // 1213383253
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_atmosphere_preset_colors_Parms, ReturnValue), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::NewProp_preset_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::NewProp_preset,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UUltimateVFXFunctionLibrary, nullptr, "get_atmosphere_preset_colors", nullptr, nullptr, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::PropPointers), sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::UltimateVFXFunctionLibrary_eventget_atmosphere_preset_colors_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04822401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::Function_MetaDataParams), Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::UltimateVFXFunctionLibrary_eventget_atmosphere_preset_colors_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UUltimateVFXFunctionLibrary::execget_atmosphere_preset_colors)
{
	P_GET_ENUM(EAtmospherePreset,Z_Param_preset);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FVector*)Z_Param__Result=UUltimateVFXFunctionLibrary::get_atmosphere_preset_colors(EAtmospherePreset(Z_Param_preset));
	P_NATIVE_END;
}
// End Class UUltimateVFXFunctionLibrary Function get_atmosphere_preset_colors

// Begin Class UUltimateVFXFunctionLibrary Function get_quality_sample_count
struct Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics
{
	struct UltimateVFXFunctionLibrary_eventget_quality_sample_count_Parms
	{
		EEffectQuality quality;
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_quality_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_quality_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_quality;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::NewProp_quality_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::NewProp_quality = { "quality", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_quality_sample_count_Parms, quality), Z_Construct_UEnum_UltimateVFX_EEffectQuality, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_quality_MetaData), NewProp_quality_MetaData) }; // 4088020921
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_quality_sample_count_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::NewProp_quality_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::NewProp_quality,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UUltimateVFXFunctionLibrary, nullptr, "get_quality_sample_count", nullptr, nullptr, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::PropPointers), sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::UltimateVFXFunctionLibrary_eventget_quality_sample_count_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::Function_MetaDataParams), Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::UltimateVFXFunctionLibrary_eventget_quality_sample_count_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UUltimateVFXFunctionLibrary::execget_quality_sample_count)
{
	P_GET_ENUM(EEffectQuality,Z_Param_quality);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=UUltimateVFXFunctionLibrary::get_quality_sample_count(EEffectQuality(Z_Param_quality));
	P_NATIVE_END;
}
// End Class UUltimateVFXFunctionLibrary Function get_quality_sample_count

// Begin Class UUltimateVFXFunctionLibrary Function get_time_of_day_sun_angle
struct Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics
{
	struct UltimateVFXFunctionLibrary_eventget_time_of_day_sun_angle_Parms
	{
		ETimeOfDay time;
		float ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_time_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_time_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_time;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::NewProp_time_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::NewProp_time = { "time", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_time_of_day_sun_angle_Parms, time), Z_Construct_UEnum_UltimateVFX_ETimeOfDay, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_time_MetaData), NewProp_time_MetaData) }; // 3512541502
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_time_of_day_sun_angle_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::NewProp_time_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::NewProp_time,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UUltimateVFXFunctionLibrary, nullptr, "get_time_of_day_sun_angle", nullptr, nullptr, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::PropPointers), sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::UltimateVFXFunctionLibrary_eventget_time_of_day_sun_angle_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::Function_MetaDataParams), Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::UltimateVFXFunctionLibrary_eventget_time_of_day_sun_angle_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UUltimateVFXFunctionLibrary::execget_time_of_day_sun_angle)
{
	P_GET_ENUM(ETimeOfDay,Z_Param_time);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(float*)Z_Param__Result=UUltimateVFXFunctionLibrary::get_time_of_day_sun_angle(ETimeOfDay(Z_Param_time));
	P_NATIVE_END;
}
// End Class UUltimateVFXFunctionLibrary Function get_time_of_day_sun_angle

// Begin Class UUltimateVFXFunctionLibrary Function get_weather_fog_density
struct Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics
{
	struct UltimateVFXFunctionLibrary_eventget_weather_fog_density_Parms
	{
		EWeatherType weather;
		float ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_weather_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_weather_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_weather;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::NewProp_weather_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::NewProp_weather = { "weather", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_weather_fog_density_Parms, weather), Z_Construct_UEnum_UltimateVFX_EWeatherType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_weather_MetaData), NewProp_weather_MetaData) }; // 1740431995
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventget_weather_fog_density_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::NewProp_weather_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::NewProp_weather,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UUltimateVFXFunctionLibrary, nullptr, "get_weather_fog_density", nullptr, nullptr, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::PropPointers), sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::UltimateVFXFunctionLibrary_eventget_weather_fog_density_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::Function_MetaDataParams), Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::UltimateVFXFunctionLibrary_eventget_weather_fog_density_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UUltimateVFXFunctionLibrary::execget_weather_fog_density)
{
	P_GET_ENUM(EWeatherType,Z_Param_weather);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(float*)Z_Param__Result=UUltimateVFXFunctionLibrary::get_weather_fog_density(EWeatherType(Z_Param_weather));
	P_NATIVE_END;
}
// End Class UUltimateVFXFunctionLibrary Function get_weather_fog_density

// Begin Class UUltimateVFXFunctionLibrary Function lerp_vec3
struct Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics
{
	struct UltimateVFXFunctionLibrary_eventlerp_vec3_Parms
	{
		FVector a;
		FVector b;
		float t;
		FVector ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Kain" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
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
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_a = { "a", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventlerp_vec3_Parms, a), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_a_MetaData), NewProp_a_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_b = { "b", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventlerp_vec3_Parms, b), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_b_MetaData), NewProp_b_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_t = { "t", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventlerp_vec3_Parms, t), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_t_MetaData), NewProp_t_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UltimateVFXFunctionLibrary_eventlerp_vec3_Parms, ReturnValue), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_a,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_b,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_t,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UUltimateVFXFunctionLibrary, nullptr, "lerp_vec3", nullptr, nullptr, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::PropPointers), sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::UltimateVFXFunctionLibrary_eventlerp_vec3_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04822401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::Function_MetaDataParams), Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::UltimateVFXFunctionLibrary_eventlerp_vec3_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UUltimateVFXFunctionLibrary::execlerp_vec3)
{
	P_GET_STRUCT(FVector,Z_Param_a);
	P_GET_STRUCT(FVector,Z_Param_b);
	P_GET_PROPERTY(FFloatProperty,Z_Param_t);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FVector*)Z_Param__Result=UUltimateVFXFunctionLibrary::lerp_vec3(Z_Param_a,Z_Param_b,Z_Param_t);
	P_NATIVE_END;
}
// End Class UUltimateVFXFunctionLibrary Function lerp_vec3

// Begin Class UUltimateVFXFunctionLibrary
void UUltimateVFXFunctionLibrary::StaticRegisterNativesUUltimateVFXFunctionLibrary()
{
	UClass* Class = UUltimateVFXFunctionLibrary::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "calculate_sun_color", &UUltimateVFXFunctionLibrary::execcalculate_sun_color },
		{ "get_atmosphere_preset_colors", &UUltimateVFXFunctionLibrary::execget_atmosphere_preset_colors },
		{ "get_quality_sample_count", &UUltimateVFXFunctionLibrary::execget_quality_sample_count },
		{ "get_time_of_day_sun_angle", &UUltimateVFXFunctionLibrary::execget_time_of_day_sun_angle },
		{ "get_weather_fog_density", &UUltimateVFXFunctionLibrary::execget_weather_fog_density },
		{ "lerp_vec3", &UUltimateVFXFunctionLibrary::execlerp_vec3 },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UUltimateVFXFunctionLibrary);
UClass* Z_Construct_UClass_UUltimateVFXFunctionLibrary_NoRegister()
{
	return UUltimateVFXFunctionLibrary::StaticClass();
}
struct Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "UltimateVFX.h" },
		{ "ModuleRelativePath", "Public/UltimateVFX.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UUltimateVFXFunctionLibrary_calculate_sun_color, "calculate_sun_color" }, // 251182453
		{ &Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_atmosphere_preset_colors, "get_atmosphere_preset_colors" }, // 409888662
		{ &Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_quality_sample_count, "get_quality_sample_count" }, // 1119976340
		{ &Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_time_of_day_sun_angle, "get_time_of_day_sun_angle" }, // 51480377
		{ &Z_Construct_UFunction_UUltimateVFXFunctionLibrary_get_weather_fog_density, "get_weather_fog_density" }, // 1284325110
		{ &Z_Construct_UFunction_UUltimateVFXFunctionLibrary_lerp_vec3, "lerp_vec3" }, // 3037423122
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UUltimateVFXFunctionLibrary>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UBlueprintFunctionLibrary,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics::ClassParams = {
	&UUltimateVFXFunctionLibrary::StaticClass,
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
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics::Class_MetaDataParams), Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UUltimateVFXFunctionLibrary()
{
	if (!Z_Registration_Info_UClass_UUltimateVFXFunctionLibrary.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UUltimateVFXFunctionLibrary.OuterSingleton, Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UUltimateVFXFunctionLibrary.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<UUltimateVFXFunctionLibrary>()
{
	return UUltimateVFXFunctionLibrary::StaticClass();
}
UUltimateVFXFunctionLibrary::UUltimateVFXFunctionLibrary(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(UUltimateVFXFunctionLibrary);
UUltimateVFXFunctionLibrary::~UUltimateVFXFunctionLibrary() {}
// End Class UUltimateVFXFunctionLibrary

// Begin Registration
struct Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EEffectQuality_StaticEnum, TEXT("EEffectQuality"), &Z_Registration_Info_UEnum_EEffectQuality, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 4088020921U) },
		{ ETimeOfDay_StaticEnum, TEXT("ETimeOfDay"), &Z_Registration_Info_UEnum_ETimeOfDay, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3512541502U) },
		{ EWeatherType_StaticEnum, TEXT("EWeatherType"), &Z_Registration_Info_UEnum_EWeatherType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1740431995U) },
		{ EAtmospherePreset_StaticEnum, TEXT("EAtmospherePreset"), &Z_Registration_Info_UEnum_EAtmospherePreset, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1213383253U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AAtmosphericScatteringActor, AAtmosphericScatteringActor::StaticClass, TEXT("AAtmosphericScatteringActor"), &Z_Registration_Info_UClass_AAtmosphericScatteringActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AAtmosphericScatteringActor), 3742540791U) },
		{ Z_Construct_UClass_AVolumetricCloudsActor, AVolumetricCloudsActor::StaticClass, TEXT("AVolumetricCloudsActor"), &Z_Registration_Info_UClass_AVolumetricCloudsActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AVolumetricCloudsActor), 3084037871U) },
		{ Z_Construct_UClass_AOceanRenderingActor, AOceanRenderingActor::StaticClass, TEXT("AOceanRenderingActor"), &Z_Registration_Info_UClass_AOceanRenderingActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AOceanRenderingActor), 626991884U) },
		{ Z_Construct_UClass_AVolumetricFogActor, AVolumetricFogActor::StaticClass, TEXT("AVolumetricFogActor"), &Z_Registration_Info_UClass_AVolumetricFogActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AVolumetricFogActor), 3818580487U) },
		{ Z_Construct_UClass_AGodRaysActor, AGodRaysActor::StaticClass, TEXT("AGodRaysActor"), &Z_Registration_Info_UClass_AGodRaysActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AGodRaysActor), 1045021050U) },
		{ Z_Construct_UClass_ABloomLensFlareActor, ABloomLensFlareActor::StaticClass, TEXT("ABloomLensFlareActor"), &Z_Registration_Info_UClass_ABloomLensFlareActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ABloomLensFlareActor), 1022255029U) },
		{ Z_Construct_UClass_AScreenSpaceReflectionsActor, AScreenSpaceReflectionsActor::StaticClass, TEXT("AScreenSpaceReflectionsActor"), &Z_Registration_Info_UClass_AScreenSpaceReflectionsActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AScreenSpaceReflectionsActor), 2638038317U) },
		{ Z_Construct_UClass_AAmbientOcclusionActor, AAmbientOcclusionActor::StaticClass, TEXT("AAmbientOcclusionActor"), &Z_Registration_Info_UClass_AAmbientOcclusionActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AAmbientOcclusionActor), 554053327U) },
		{ Z_Construct_UClass_ADepthOfFieldActor, ADepthOfFieldActor::StaticClass, TEXT("ADepthOfFieldActor"), &Z_Registration_Info_UClass_ADepthOfFieldActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ADepthOfFieldActor), 1226873439U) },
		{ Z_Construct_UClass_AMotionBlurActor, AMotionBlurActor::StaticClass, TEXT("AMotionBlurActor"), &Z_Registration_Info_UClass_AMotionBlurActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AMotionBlurActor), 4268663278U) },
		{ Z_Construct_UClass_AColorGradingActor, AColorGradingActor::StaticClass, TEXT("AColorGradingActor"), &Z_Registration_Info_UClass_AColorGradingActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AColorGradingActor), 1732604783U) },
		{ Z_Construct_UClass_AChromaticAberrationActor, AChromaticAberrationActor::StaticClass, TEXT("AChromaticAberrationActor"), &Z_Registration_Info_UClass_AChromaticAberrationActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AChromaticAberrationActor), 878569648U) },
		{ Z_Construct_UClass_AFilmGrainActor, AFilmGrainActor::StaticClass, TEXT("AFilmGrainActor"), &Z_Registration_Info_UClass_AFilmGrainActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AFilmGrainActor), 2197984606U) },
		{ Z_Construct_UClass_ASharpenActor, ASharpenActor::StaticClass, TEXT("ASharpenActor"), &Z_Registration_Info_UClass_ASharpenActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ASharpenActor), 2172924748U) },
		{ Z_Construct_UClass_ARainDropsActor, ARainDropsActor::StaticClass, TEXT("ARainDropsActor"), &Z_Registration_Info_UClass_ARainDropsActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ARainDropsActor), 2276941710U) },
		{ Z_Construct_UClass_AProceduralSkyActor, AProceduralSkyActor::StaticClass, TEXT("AProceduralSkyActor"), &Z_Registration_Info_UClass_AProceduralSkyActor, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AProceduralSkyActor), 700102223U) },
		{ Z_Construct_UClass_UUltimateVFXFunctionLibrary, UUltimateVFXFunctionLibrary::StaticClass, TEXT("UUltimateVFXFunctionLibrary"), &Z_Registration_Info_UClass_UUltimateVFXFunctionLibrary, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UUltimateVFXFunctionLibrary), 1454243111U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_1632782749(TEXT("/Script/UltimateVFX"),
	Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_Statics::ClassInfo),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
