// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/EBoundsMode.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeEBoundsMode() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UEnum* Z_Construct_UEnum_QuantumParticles_EBoundsMode();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin Enum EBoundsMode
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EBoundsMode;
static UEnum* EBoundsMode_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EBoundsMode.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EBoundsMode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_QuantumParticles_EBoundsMode, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("EBoundsMode"));
	}
	return Z_Registration_Info_UEnum_EBoundsMode.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UEnum* StaticEnum<EBoundsMode>()
{
	return EBoundsMode_StaticEnum();
}
struct Z_Construct_UEnum_QuantumParticles_EBoundsMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "Bounce.DisplayName", "Bounce" },
		{ "Bounce.Name", "EBoundsMode::Bounce" },
		{ "ModuleRelativePath", "Public/EBoundsMode.h" },
		{ "Respawn.DisplayName", "Respawn" },
		{ "Respawn.Name", "EBoundsMode::Respawn" },
		{ "Wrap.DisplayName", "Wrap" },
		{ "Wrap.Name", "EBoundsMode::Wrap" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EBoundsMode::Bounce", (int64)EBoundsMode::Bounce },
		{ "EBoundsMode::Wrap", (int64)EBoundsMode::Wrap },
		{ "EBoundsMode::Respawn", (int64)EBoundsMode::Respawn },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_QuantumParticles_EBoundsMode_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	"EBoundsMode",
	"EBoundsMode",
	Z_Construct_UEnum_QuantumParticles_EBoundsMode_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_EBoundsMode_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_EBoundsMode_Statics::Enum_MetaDataParams), Z_Construct_UEnum_QuantumParticles_EBoundsMode_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_QuantumParticles_EBoundsMode()
{
	if (!Z_Registration_Info_UEnum_EBoundsMode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EBoundsMode.InnerSingleton, Z_Construct_UEnum_QuantumParticles_EBoundsMode_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EBoundsMode.InnerSingleton;
}
// End Enum EBoundsMode

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EBoundsMode_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EBoundsMode_StaticEnum, TEXT("EBoundsMode"), &Z_Registration_Info_UEnum_EBoundsMode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2804057704U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EBoundsMode_h_2737561166(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EBoundsMode_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EBoundsMode_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
