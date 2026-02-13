// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/ESimulationMode.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeESimulationMode() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UEnum* Z_Construct_UEnum_QuantumParticles_ESimulationMode();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin Enum ESimulationMode
static FEnumRegistrationInfo Z_Registration_Info_UEnum_ESimulationMode;
static UEnum* ESimulationMode_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_ESimulationMode.OuterSingleton)
	{
		Z_Registration_Info_UEnum_ESimulationMode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_QuantumParticles_ESimulationMode, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("ESimulationMode"));
	}
	return Z_Registration_Info_UEnum_ESimulationMode.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UEnum* StaticEnum<ESimulationMode>()
{
	return ESimulationMode_StaticEnum();
}
struct Z_Construct_UEnum_QuantumParticles_ESimulationMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "AizawaAttractor.DisplayName", "AizawaAttractor" },
		{ "AizawaAttractor.Name", "ESimulationMode::AizawaAttractor" },
		{ "AlcubierreWarp.DisplayName", "AlcubierreWarp" },
		{ "AlcubierreWarp.Name", "ESimulationMode::AlcubierreWarp" },
		{ "BinarySystem.DisplayName", "BinarySystem" },
		{ "BinarySystem.Name", "ESimulationMode::BinarySystem" },
		{ "BlueprintType", "true" },
		{ "CyberpunkCity.DisplayName", "CyberpunkCity" },
		{ "CyberpunkCity.Name", "ESimulationMode::CyberpunkCity" },
		{ "Datamosh.DisplayName", "Datamosh" },
		{ "Datamosh.Name", "ESimulationMode::Datamosh" },
		{ "DNAHelix.DisplayName", "DNAHelix" },
		{ "DNAHelix.Name", "ESimulationMode::DNAHelix" },
		{ "Ferrofluid.DisplayName", "Ferrofluid" },
		{ "Ferrofluid.Name", "ESimulationMode::Ferrofluid" },
		{ "GalacticSpiral.DisplayName", "GalacticSpiral" },
		{ "GalacticSpiral.Name", "ESimulationMode::GalacticSpiral" },
		{ "Hellfire.DisplayName", "Hellfire" },
		{ "Hellfire.Name", "ESimulationMode::Hellfire" },
		{ "IonStorm.DisplayName", "IonStorm" },
		{ "IonStorm.Name", "ESimulationMode::IonStorm" },
		{ "KerrBlackHole.DisplayName", "KerrBlackHole" },
		{ "KerrBlackHole.Name", "ESimulationMode::KerrBlackHole" },
		{ "LorenzAttractor.DisplayName", "LorenzAttractor" },
		{ "LorenzAttractor.Name", "ESimulationMode::LorenzAttractor" },
		{ "ModuleRelativePath", "Public/ESimulationMode.h" },
		{ "NavierStokes.DisplayName", "NavierStokes" },
		{ "NavierStokes.Name", "ESimulationMode::NavierStokes" },
		{ "NeuralLattice.DisplayName", "NeuralLattice" },
		{ "NeuralLattice.Name", "ESimulationMode::NeuralLattice" },
		{ "PhotoKinesis.DisplayName", "PhotoKinesis" },
		{ "PhotoKinesis.Name", "ESimulationMode::PhotoKinesis" },
		{ "PlasmaArc.DisplayName", "PlasmaArc" },
		{ "PlasmaArc.Name", "ESimulationMode::PlasmaArc" },
		{ "QuantumFoam.DisplayName", "QuantumFoam" },
		{ "QuantumFoam.Name", "ESimulationMode::QuantumFoam" },
		{ "QuantumPilot.DisplayName", "QuantumPilot" },
		{ "QuantumPilot.Name", "ESimulationMode::QuantumPilot" },
		{ "QuasarJet.DisplayName", "QuasarJet" },
		{ "QuasarJet.Name", "ESimulationMode::QuasarJet" },
		{ "SchrodingerWave.DisplayName", "SchrodingerWave" },
		{ "SchrodingerWave.Name", "ESimulationMode::SchrodingerWave" },
		{ "SolarProminence.DisplayName", "SolarProminence" },
		{ "SolarProminence.Name", "ESimulationMode::SolarProminence" },
		{ "SupernovaRemnant.DisplayName", "SupernovaRemnant" },
		{ "SupernovaRemnant.Name", "ESimulationMode::SupernovaRemnant" },
		{ "SuperVortex.DisplayName", "SuperVortex" },
		{ "SuperVortex.Name", "ESimulationMode::SuperVortex" },
		{ "Tesseract.DisplayName", "Tesseract" },
		{ "Tesseract.Name", "ESimulationMode::Tesseract" },
		{ "VanAllenBelt.DisplayName", "VanAllenBelt" },
		{ "VanAllenBelt.Name", "ESimulationMode::VanAllenBelt" },
		{ "ZeroPoint.DisplayName", "ZeroPoint" },
		{ "ZeroPoint.Name", "ESimulationMode::ZeroPoint" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "ESimulationMode::ZeroPoint", (int64)ESimulationMode::ZeroPoint },
		{ "ESimulationMode::GalacticSpiral", (int64)ESimulationMode::GalacticSpiral },
		{ "ESimulationMode::PhotoKinesis", (int64)ESimulationMode::PhotoKinesis },
		{ "ESimulationMode::QuantumPilot", (int64)ESimulationMode::QuantumPilot },
		{ "ESimulationMode::SchrodingerWave", (int64)ESimulationMode::SchrodingerWave },
		{ "ESimulationMode::Tesseract", (int64)ESimulationMode::Tesseract },
		{ "ESimulationMode::NeuralLattice", (int64)ESimulationMode::NeuralLattice },
		{ "ESimulationMode::Ferrofluid", (int64)ESimulationMode::Ferrofluid },
		{ "ESimulationMode::Datamosh", (int64)ESimulationMode::Datamosh },
		{ "ESimulationMode::NavierStokes", (int64)ESimulationMode::NavierStokes },
		{ "ESimulationMode::Hellfire", (int64)ESimulationMode::Hellfire },
		{ "ESimulationMode::PlasmaArc", (int64)ESimulationMode::PlasmaArc },
		{ "ESimulationMode::SuperVortex", (int64)ESimulationMode::SuperVortex },
		{ "ESimulationMode::KerrBlackHole", (int64)ESimulationMode::KerrBlackHole },
		{ "ESimulationMode::IonStorm", (int64)ESimulationMode::IonStorm },
		{ "ESimulationMode::LorenzAttractor", (int64)ESimulationMode::LorenzAttractor },
		{ "ESimulationMode::VanAllenBelt", (int64)ESimulationMode::VanAllenBelt },
		{ "ESimulationMode::AizawaAttractor", (int64)ESimulationMode::AizawaAttractor },
		{ "ESimulationMode::BinarySystem", (int64)ESimulationMode::BinarySystem },
		{ "ESimulationMode::QuasarJet", (int64)ESimulationMode::QuasarJet },
		{ "ESimulationMode::SupernovaRemnant", (int64)ESimulationMode::SupernovaRemnant },
		{ "ESimulationMode::AlcubierreWarp", (int64)ESimulationMode::AlcubierreWarp },
		{ "ESimulationMode::SolarProminence", (int64)ESimulationMode::SolarProminence },
		{ "ESimulationMode::QuantumFoam", (int64)ESimulationMode::QuantumFoam },
		{ "ESimulationMode::CyberpunkCity", (int64)ESimulationMode::CyberpunkCity },
		{ "ESimulationMode::DNAHelix", (int64)ESimulationMode::DNAHelix },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_QuantumParticles_ESimulationMode_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	"ESimulationMode",
	"ESimulationMode",
	Z_Construct_UEnum_QuantumParticles_ESimulationMode_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_ESimulationMode_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_QuantumParticles_ESimulationMode_Statics::Enum_MetaDataParams), Z_Construct_UEnum_QuantumParticles_ESimulationMode_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_QuantumParticles_ESimulationMode()
{
	if (!Z_Registration_Info_UEnum_ESimulationMode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_ESimulationMode.InnerSingleton, Z_Construct_UEnum_QuantumParticles_ESimulationMode_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_ESimulationMode.InnerSingleton;
}
// End Enum ESimulationMode

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_ESimulationMode_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ ESimulationMode_StaticEnum, TEXT("ESimulationMode"), &Z_Registration_Info_UEnum_ESimulationMode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3496891889U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_ESimulationMode_h_2566215962(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_ESimulationMode_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_ESimulationMode_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
