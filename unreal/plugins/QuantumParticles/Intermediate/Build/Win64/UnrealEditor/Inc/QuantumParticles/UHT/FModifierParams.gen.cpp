// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/FModifierParams.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeFModifierParams() {}

// Begin Cross Module References
QUANTUMPARTICLES_API UScriptStruct* Z_Construct_UScriptStruct_FModifierParams();
UPackage* Z_Construct_UPackage__Script_QuantumParticles();
// End Cross Module References

// Begin ScriptStruct FModifierParams
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_ModifierParams;
class UScriptStruct* FModifierParams::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_ModifierParams.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_ModifierParams.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FModifierParams, (UObject*)Z_Construct_UPackage__Script_QuantumParticles(), TEXT("ModifierParams"));
	}
	return Z_Registration_Info_UScriptStruct_ModifierParams.OuterSingleton;
}
template<> QUANTUMPARTICLES_API UScriptStruct* StaticStruct<FModifierParams>()
{
	return FModifierParams::StaticStruct();
}
struct Z_Construct_UScriptStruct_FModifierParams_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_heartbeat_bpm_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_heartbeat_intensity_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_seismic_scale_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_seismic_freq_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_pulse_freq_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_pulse_amplitude_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_breathe_rate_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_breathe_depth_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_helix_speed_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_helix_tightness_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_gravity_force_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_gravity_radius_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_repulsor_force_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_repulsor_falloff_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_orbit_speed_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_orbit_radius_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_vortex_strength_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_vortex_lift_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_magnet_dipole_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_magnet_separation_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_explosion_force_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_explosion_decay_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_swarm_cohesion_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_swarm_separation_MetaData[] = {
		{ "Category", "ModifierParams" },
		{ "ModuleRelativePath", "Public/FModifierParams.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_heartbeat_bpm;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_heartbeat_intensity;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_seismic_scale;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_seismic_freq;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_pulse_freq;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_pulse_amplitude;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_breathe_rate;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_breathe_depth;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_helix_speed;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_helix_tightness;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_gravity_force;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_gravity_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_repulsor_force;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_repulsor_falloff;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_orbit_speed;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_orbit_radius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_vortex_strength;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_vortex_lift;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_magnet_dipole;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_magnet_separation;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_explosion_force;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_explosion_decay;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_swarm_cohesion;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_swarm_separation;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FModifierParams>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_heartbeat_bpm = { "heartbeat_bpm", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, heartbeat_bpm), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_heartbeat_bpm_MetaData), NewProp_heartbeat_bpm_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_heartbeat_intensity = { "heartbeat_intensity", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, heartbeat_intensity), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_heartbeat_intensity_MetaData), NewProp_heartbeat_intensity_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_seismic_scale = { "seismic_scale", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, seismic_scale), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_seismic_scale_MetaData), NewProp_seismic_scale_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_seismic_freq = { "seismic_freq", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, seismic_freq), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_seismic_freq_MetaData), NewProp_seismic_freq_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_pulse_freq = { "pulse_freq", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, pulse_freq), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_pulse_freq_MetaData), NewProp_pulse_freq_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_pulse_amplitude = { "pulse_amplitude", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, pulse_amplitude), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_pulse_amplitude_MetaData), NewProp_pulse_amplitude_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_breathe_rate = { "breathe_rate", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, breathe_rate), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_breathe_rate_MetaData), NewProp_breathe_rate_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_breathe_depth = { "breathe_depth", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, breathe_depth), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_breathe_depth_MetaData), NewProp_breathe_depth_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_helix_speed = { "helix_speed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, helix_speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_helix_speed_MetaData), NewProp_helix_speed_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_helix_tightness = { "helix_tightness", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, helix_tightness), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_helix_tightness_MetaData), NewProp_helix_tightness_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_gravity_force = { "gravity_force", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, gravity_force), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_gravity_force_MetaData), NewProp_gravity_force_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_gravity_radius = { "gravity_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, gravity_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_gravity_radius_MetaData), NewProp_gravity_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_repulsor_force = { "repulsor_force", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, repulsor_force), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_repulsor_force_MetaData), NewProp_repulsor_force_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_repulsor_falloff = { "repulsor_falloff", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, repulsor_falloff), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_repulsor_falloff_MetaData), NewProp_repulsor_falloff_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_orbit_speed = { "orbit_speed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, orbit_speed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_orbit_speed_MetaData), NewProp_orbit_speed_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_orbit_radius = { "orbit_radius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, orbit_radius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_orbit_radius_MetaData), NewProp_orbit_radius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_vortex_strength = { "vortex_strength", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, vortex_strength), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_vortex_strength_MetaData), NewProp_vortex_strength_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_vortex_lift = { "vortex_lift", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, vortex_lift), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_vortex_lift_MetaData), NewProp_vortex_lift_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_magnet_dipole = { "magnet_dipole", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, magnet_dipole), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_magnet_dipole_MetaData), NewProp_magnet_dipole_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_magnet_separation = { "magnet_separation", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, magnet_separation), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_magnet_separation_MetaData), NewProp_magnet_separation_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_explosion_force = { "explosion_force", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, explosion_force), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_explosion_force_MetaData), NewProp_explosion_force_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_explosion_decay = { "explosion_decay", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, explosion_decay), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_explosion_decay_MetaData), NewProp_explosion_decay_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_swarm_cohesion = { "swarm_cohesion", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, swarm_cohesion), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_swarm_cohesion_MetaData), NewProp_swarm_cohesion_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_swarm_separation = { "swarm_separation", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FModifierParams, swarm_separation), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_swarm_separation_MetaData), NewProp_swarm_separation_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FModifierParams_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_heartbeat_bpm,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_heartbeat_intensity,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_seismic_scale,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_seismic_freq,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_pulse_freq,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_pulse_amplitude,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_breathe_rate,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_breathe_depth,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_helix_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_helix_tightness,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_gravity_force,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_gravity_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_repulsor_force,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_repulsor_falloff,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_orbit_speed,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_orbit_radius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_vortex_strength,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_vortex_lift,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_magnet_dipole,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_magnet_separation,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_explosion_force,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_explosion_decay,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_swarm_cohesion,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FModifierParams_Statics::NewProp_swarm_separation,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FModifierParams_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FModifierParams_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_QuantumParticles,
	nullptr,
	&NewStructOps,
	"ModifierParams",
	Z_Construct_UScriptStruct_FModifierParams_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FModifierParams_Statics::PropPointers),
	sizeof(FModifierParams),
	alignof(FModifierParams),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FModifierParams_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FModifierParams_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FModifierParams()
{
	if (!Z_Registration_Info_UScriptStruct_ModifierParams.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_ModifierParams.InnerSingleton, Z_Construct_UScriptStruct_FModifierParams_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_ModifierParams.InnerSingleton;
}
// End ScriptStruct FModifierParams

// Begin Registration
struct Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FModifierParams_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FModifierParams::StaticStruct, Z_Construct_UScriptStruct_FModifierParams_Statics::NewStructOps, TEXT("ModifierParams"), &Z_Registration_Info_UScriptStruct_ModifierParams, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FModifierParams), 1325771244U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FModifierParams_h_963940167(TEXT("/Script/QuantumParticles"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FModifierParams_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MyProject_Plugins_QuantumParticles_Source_Public_FModifierParams_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
