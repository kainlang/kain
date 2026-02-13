// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/EColorMode.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeEColorMode() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UEnum* Z_Construct_UEnum_QuantumParticles_EColorMode();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin Enum EColorMode
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EColorMode;
static UEnum* EColorMode_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EColorMode.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EColorMode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_QuantumParticles_EColorMode, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("EColorMode"));
	}
	return Z_Registration_Info_UEnum_EColorMode.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UEnum* StaticEnum<EColorMode>()
{
	return EColorMode_StaticEnum();
}
struct Z_Construct_UEnum_QuantumParticles_EColorMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "Gradient.DisplayName", "Gradient" },
		{ "Gradient.Name", "EColorMode::Gradient" },
		{ "Image.DisplayName", "Image" },
		{ "Image.Name", "EColorMode::Image" },
		{ "ModuleRelativePath", "Public/EColorMode.h" },
		{ "Solid.DisplayName", "Solid" },
		{ "Solid.Name", "EColorMode::Solid" },
		{ "Velocity.DisplayName", "Velocity" },
		{ "Velocity.Name", "EColorMode::Velocity" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EColorMode::Solid", (int64)EColorMode::Solid },
		{ "EColorMode::Velocity", (int64)EColorMode::Velocity },
		{ "EColorMode::Image", (int64)EColorMode::Image },
		{ "EColorMode::Gradient", (int64)EColorMode::Gradient },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_QuantumParticles_EColorMode_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	"EColorMode",
	"EColorMode",
	Z_Construct_UEnum_QuantumParticles_EColorMode_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_EColorMode_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_EColorMode_Statics::Enum_MetaDataParams), Z_Construct_UEnum_QuantumParticles_EColorMode_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_QuantumParticles_EColorMode()
{
	if (!Z_Registration_Info_UEnum_EColorMode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EColorMode.InnerSingleton, Z_Construct_UEnum_QuantumParticles_EColorMode_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EColorMode.InnerSingleton;
}
// End Enum EColorMode

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EColorMode_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EColorMode_StaticEnum, TEXT("EColorMode"), &Z_Registration_Info_UEnum_EColorMode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1449425345U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EColorMode_h_867467768(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EColorMode_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EColorMode_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
