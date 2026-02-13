// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/EModifierType.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeEModifierType() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UEnum* Z_Construct_UEnum_QuantumParticles_EModifierType();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin Enum EModifierType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EModifierType;
static UEnum* EModifierType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EModifierType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EModifierType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_QuantumParticles_EModifierType, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("EModifierType"));
	}
	return Z_Registration_Info_UEnum_EModifierType.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UEnum* StaticEnum<EModifierType>()
{
	return EModifierType_StaticEnum();
}
struct Z_Construct_UEnum_QuantumParticles_EModifierType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "Breathe.DisplayName", "Breathe" },
		{ "Breathe.Name", "EModifierType::Breathe" },
		{ "Explosion.DisplayName", "Explosion" },
		{ "Explosion.Name", "EModifierType::Explosion" },
		{ "Gravity.DisplayName", "Gravity" },
		{ "Gravity.Name", "EModifierType::Gravity" },
		{ "Heartbeat.DisplayName", "Heartbeat" },
		{ "Heartbeat.Name", "EModifierType::Heartbeat" },
		{ "Helix.DisplayName", "Helix" },
		{ "Helix.Name", "EModifierType::Helix" },
		{ "Magnet.DisplayName", "Magnet" },
		{ "Magnet.Name", "EModifierType::Magnet" },
		{ "ModuleRelativePath", "Public/EModifierType.h" },
		{ "Orbit.DisplayName", "Orbit" },
		{ "Orbit.Name", "EModifierType::Orbit" },
		{ "Pulse.DisplayName", "Pulse" },
		{ "Pulse.Name", "EModifierType::Pulse" },
		{ "Repulsor.DisplayName", "Repulsor" },
		{ "Repulsor.Name", "EModifierType::Repulsor" },
		{ "Seismic.DisplayName", "Seismic" },
		{ "Seismic.Name", "EModifierType::Seismic" },
		{ "Swarm.DisplayName", "Swarm" },
		{ "Swarm.Name", "EModifierType::Swarm" },
		{ "Vortex.DisplayName", "Vortex" },
		{ "Vortex.Name", "EModifierType::Vortex" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EModifierType::Heartbeat", (int64)EModifierType::Heartbeat },
		{ "EModifierType::Seismic", (int64)EModifierType::Seismic },
		{ "EModifierType::Pulse", (int64)EModifierType::Pulse },
		{ "EModifierType::Breathe", (int64)EModifierType::Breathe },
		{ "EModifierType::Helix", (int64)EModifierType::Helix },
		{ "EModifierType::Gravity", (int64)EModifierType::Gravity },
		{ "EModifierType::Repulsor", (int64)EModifierType::Repulsor },
		{ "EModifierType::Orbit", (int64)EModifierType::Orbit },
		{ "EModifierType::Vortex", (int64)EModifierType::Vortex },
		{ "EModifierType::Magnet", (int64)EModifierType::Magnet },
		{ "EModifierType::Explosion", (int64)EModifierType::Explosion },
		{ "EModifierType::Swarm", (int64)EModifierType::Swarm },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_QuantumParticles_EModifierType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	"EModifierType",
	"EModifierType",
	Z_Construct_UEnum_QuantumParticles_EModifierType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_EModifierType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_EModifierType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_QuantumParticles_EModifierType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_QuantumParticles_EModifierType()
{
	if (!Z_Registration_Info_UEnum_EModifierType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EModifierType.InnerSingleton, Z_Construct_UEnum_QuantumParticles_EModifierType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EModifierType.InnerSingleton;
}
// End Enum EModifierType

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EModifierType_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EModifierType_StaticEnum, TEXT("EModifierType"), &Z_Registration_Info_UEnum_EModifierType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3574065018U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EModifierType_h_2333844699(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EModifierType_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_EModifierType_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
