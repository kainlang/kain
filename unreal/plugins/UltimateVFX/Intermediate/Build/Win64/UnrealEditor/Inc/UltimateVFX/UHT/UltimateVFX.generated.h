// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "UltimateVFX.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
enum class EAtmospherePreset : uint8;
enum class EEffectQuality : uint8;
enum class ETimeOfDay : uint8;
enum class EWeatherType : uint8;
#ifdef ULTIMATEVFX_UltimateVFX_generated_h
#error "UltimateVFX.generated.h already included, missing '#pragma once' in UltimateVFX.h"
#endif
#define ULTIMATEVFX_UltimateVFX_generated_h

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_RPC_WRAPPERS_NO_PURE_DECLS \
	virtual void Server_SetTimeOfDay_Implementation(const ETimeOfDay time); \
	DECLARE_FUNCTION(execServer_SetTimeOfDay);


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_CALLBACK_WRAPPERS
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAAtmosphericScatteringActor(); \
	friend struct Z_Construct_UClass_AAtmosphericScatteringActor_Statics; \
public: \
	DECLARE_CLASS(AAtmosphericScatteringActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AAtmosphericScatteringActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AAtmosphericScatteringActor(AAtmosphericScatteringActor&&); \
	AAtmosphericScatteringActor(const AAtmosphericScatteringActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AAtmosphericScatteringActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AAtmosphericScatteringActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AAtmosphericScatteringActor) \
	NO_API virtual ~AAtmosphericScatteringActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_59_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_CALLBACK_WRAPPERS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_62_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AAtmosphericScatteringActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_RPC_WRAPPERS_NO_PURE_DECLS \
	virtual void Server_SetWeather_Implementation(const EWeatherType weather); \
	DECLARE_FUNCTION(execServer_SetWeather);


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_CALLBACK_WRAPPERS
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAVolumetricCloudsActor(); \
	friend struct Z_Construct_UClass_AVolumetricCloudsActor_Statics; \
public: \
	DECLARE_CLASS(AVolumetricCloudsActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AVolumetricCloudsActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AVolumetricCloudsActor(AVolumetricCloudsActor&&); \
	AVolumetricCloudsActor(const AVolumetricCloudsActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AVolumetricCloudsActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AVolumetricCloudsActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AVolumetricCloudsActor) \
	NO_API virtual ~AVolumetricCloudsActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_114_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_CALLBACK_WRAPPERS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_117_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AVolumetricCloudsActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_182_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAOceanRenderingActor(); \
	friend struct Z_Construct_UClass_AOceanRenderingActor_Statics; \
public: \
	DECLARE_CLASS(AOceanRenderingActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AOceanRenderingActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_182_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AOceanRenderingActor(AOceanRenderingActor&&); \
	AOceanRenderingActor(const AOceanRenderingActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AOceanRenderingActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AOceanRenderingActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AOceanRenderingActor) \
	NO_API virtual ~AOceanRenderingActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_179_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_182_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_182_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_182_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AOceanRenderingActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_244_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAVolumetricFogActor(); \
	friend struct Z_Construct_UClass_AVolumetricFogActor_Statics; \
public: \
	DECLARE_CLASS(AVolumetricFogActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AVolumetricFogActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_244_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AVolumetricFogActor(AVolumetricFogActor&&); \
	AVolumetricFogActor(const AVolumetricFogActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AVolumetricFogActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AVolumetricFogActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AVolumetricFogActor) \
	NO_API virtual ~AVolumetricFogActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_241_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_244_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_244_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_244_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AVolumetricFogActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_289_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAGodRaysActor(); \
	friend struct Z_Construct_UClass_AGodRaysActor_Statics; \
public: \
	DECLARE_CLASS(AGodRaysActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AGodRaysActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_289_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AGodRaysActor(AGodRaysActor&&); \
	AGodRaysActor(const AGodRaysActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AGodRaysActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AGodRaysActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AGodRaysActor) \
	NO_API virtual ~AGodRaysActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_286_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_289_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_289_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_289_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AGodRaysActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_328_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesABloomLensFlareActor(); \
	friend struct Z_Construct_UClass_ABloomLensFlareActor_Statics; \
public: \
	DECLARE_CLASS(ABloomLensFlareActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(ABloomLensFlareActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_328_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	ABloomLensFlareActor(ABloomLensFlareActor&&); \
	ABloomLensFlareActor(const ABloomLensFlareActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ABloomLensFlareActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ABloomLensFlareActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(ABloomLensFlareActor) \
	NO_API virtual ~ABloomLensFlareActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_325_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_328_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_328_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_328_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class ABloomLensFlareActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_370_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAScreenSpaceReflectionsActor(); \
	friend struct Z_Construct_UClass_AScreenSpaceReflectionsActor_Statics; \
public: \
	DECLARE_CLASS(AScreenSpaceReflectionsActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AScreenSpaceReflectionsActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_370_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AScreenSpaceReflectionsActor(AScreenSpaceReflectionsActor&&); \
	AScreenSpaceReflectionsActor(const AScreenSpaceReflectionsActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AScreenSpaceReflectionsActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AScreenSpaceReflectionsActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AScreenSpaceReflectionsActor) \
	NO_API virtual ~AScreenSpaceReflectionsActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_367_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_370_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_370_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_370_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AScreenSpaceReflectionsActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_412_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAAmbientOcclusionActor(); \
	friend struct Z_Construct_UClass_AAmbientOcclusionActor_Statics; \
public: \
	DECLARE_CLASS(AAmbientOcclusionActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AAmbientOcclusionActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_412_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AAmbientOcclusionActor(AAmbientOcclusionActor&&); \
	AAmbientOcclusionActor(const AAmbientOcclusionActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AAmbientOcclusionActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AAmbientOcclusionActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AAmbientOcclusionActor) \
	NO_API virtual ~AAmbientOcclusionActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_409_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_412_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_412_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_412_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AAmbientOcclusionActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_448_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesADepthOfFieldActor(); \
	friend struct Z_Construct_UClass_ADepthOfFieldActor_Statics; \
public: \
	DECLARE_CLASS(ADepthOfFieldActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(ADepthOfFieldActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_448_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	ADepthOfFieldActor(ADepthOfFieldActor&&); \
	ADepthOfFieldActor(const ADepthOfFieldActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ADepthOfFieldActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ADepthOfFieldActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(ADepthOfFieldActor) \
	NO_API virtual ~ADepthOfFieldActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_445_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_448_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_448_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_448_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class ADepthOfFieldActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_487_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAMotionBlurActor(); \
	friend struct Z_Construct_UClass_AMotionBlurActor_Statics; \
public: \
	DECLARE_CLASS(AMotionBlurActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AMotionBlurActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_487_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AMotionBlurActor(AMotionBlurActor&&); \
	AMotionBlurActor(const AMotionBlurActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AMotionBlurActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AMotionBlurActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AMotionBlurActor) \
	NO_API virtual ~AMotionBlurActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_484_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_487_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_487_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_487_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AMotionBlurActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_514_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAColorGradingActor(); \
	friend struct Z_Construct_UClass_AColorGradingActor_Statics; \
public: \
	DECLARE_CLASS(AColorGradingActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AColorGradingActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_514_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AColorGradingActor(AColorGradingActor&&); \
	AColorGradingActor(const AColorGradingActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AColorGradingActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AColorGradingActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AColorGradingActor) \
	NO_API virtual ~AColorGradingActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_511_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_514_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_514_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_514_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AColorGradingActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_562_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAChromaticAberrationActor(); \
	friend struct Z_Construct_UClass_AChromaticAberrationActor_Statics; \
public: \
	DECLARE_CLASS(AChromaticAberrationActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AChromaticAberrationActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_562_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AChromaticAberrationActor(AChromaticAberrationActor&&); \
	AChromaticAberrationActor(const AChromaticAberrationActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AChromaticAberrationActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AChromaticAberrationActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AChromaticAberrationActor) \
	NO_API virtual ~AChromaticAberrationActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_559_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_562_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_562_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_562_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AChromaticAberrationActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_586_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAFilmGrainActor(); \
	friend struct Z_Construct_UClass_AFilmGrainActor_Statics; \
public: \
	DECLARE_CLASS(AFilmGrainActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AFilmGrainActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_586_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AFilmGrainActor(AFilmGrainActor&&); \
	AFilmGrainActor(const AFilmGrainActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AFilmGrainActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AFilmGrainActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AFilmGrainActor) \
	NO_API virtual ~AFilmGrainActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_583_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_586_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_586_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_586_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AFilmGrainActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_610_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesASharpenActor(); \
	friend struct Z_Construct_UClass_ASharpenActor_Statics; \
public: \
	DECLARE_CLASS(ASharpenActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(ASharpenActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_610_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	ASharpenActor(ASharpenActor&&); \
	ASharpenActor(const ASharpenActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ASharpenActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ASharpenActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(ASharpenActor) \
	NO_API virtual ~ASharpenActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_607_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_610_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_610_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_610_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class ASharpenActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_631_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesARainDropsActor(); \
	friend struct Z_Construct_UClass_ARainDropsActor_Statics; \
public: \
	DECLARE_CLASS(ARainDropsActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(ARainDropsActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_631_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	ARainDropsActor(ARainDropsActor&&); \
	ARainDropsActor(const ARainDropsActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ARainDropsActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ARainDropsActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(ARainDropsActor) \
	NO_API virtual ~ARainDropsActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_628_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_631_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_631_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_631_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class ARainDropsActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_661_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAProceduralSkyActor(); \
	friend struct Z_Construct_UClass_AProceduralSkyActor_Statics; \
public: \
	DECLARE_CLASS(AProceduralSkyActor, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(AProceduralSkyActor)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_661_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AProceduralSkyActor(AProceduralSkyActor&&); \
	AProceduralSkyActor(const AProceduralSkyActor&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AProceduralSkyActor); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AProceduralSkyActor); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AProceduralSkyActor) \
	NO_API virtual ~AProceduralSkyActor();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_658_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_661_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_661_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_661_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class AProceduralSkyActor>();

#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_710_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execget_atmosphere_preset_colors); \
	DECLARE_FUNCTION(execcalculate_sun_color); \
	DECLARE_FUNCTION(execlerp_vec3); \
	DECLARE_FUNCTION(execget_weather_fog_density); \
	DECLARE_FUNCTION(execget_time_of_day_sun_angle); \
	DECLARE_FUNCTION(execget_quality_sample_count);


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_710_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUUltimateVFXFunctionLibrary(); \
	friend struct Z_Construct_UClass_UUltimateVFXFunctionLibrary_Statics; \
public: \
	DECLARE_CLASS(UUltimateVFXFunctionLibrary, UBlueprintFunctionLibrary, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/UltimateVFX"), NO_API) \
	DECLARE_SERIALIZER(UUltimateVFXFunctionLibrary)


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_710_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UUltimateVFXFunctionLibrary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UUltimateVFXFunctionLibrary(UUltimateVFXFunctionLibrary&&); \
	UUltimateVFXFunctionLibrary(const UUltimateVFXFunctionLibrary&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UUltimateVFXFunctionLibrary); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UUltimateVFXFunctionLibrary); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UUltimateVFXFunctionLibrary) \
	NO_API virtual ~UUltimateVFXFunctionLibrary();


#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_707_PROLOG
#define FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_710_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_710_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_710_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h_710_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> ULTIMATEVFX_API UClass* StaticClass<class UUltimateVFXFunctionLibrary>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFX_h


#define FOREACH_ENUM_EEFFECTQUALITY(op) \
	op(EEffectQuality::Potato) \
	op(EEffectQuality::Low) \
	op(EEffectQuality::Medium) \
	op(EEffectQuality::High) \
	op(EEffectQuality::Ultra) \
	op(EEffectQuality::Cinematic) 

enum class EEffectQuality : uint8;
template<> struct TIsUEnumClass<EEffectQuality> { enum { Value = true }; };
template<> ULTIMATEVFX_API UEnum* StaticEnum<EEffectQuality>();

#define FOREACH_ENUM_ETIMEOFDAY(op) \
	op(ETimeOfDay::Dawn) \
	op(ETimeOfDay::Morning) \
	op(ETimeOfDay::Noon) \
	op(ETimeOfDay::Afternoon) \
	op(ETimeOfDay::Dusk) \
	op(ETimeOfDay::Night) \
	op(ETimeOfDay::Midnight) 

enum class ETimeOfDay : uint8;
template<> struct TIsUEnumClass<ETimeOfDay> { enum { Value = true }; };
template<> ULTIMATEVFX_API UEnum* StaticEnum<ETimeOfDay>();

#define FOREACH_ENUM_EWEATHERTYPE(op) \
	op(EWeatherType::Clear) \
	op(EWeatherType::Cloudy) \
	op(EWeatherType::Overcast) \
	op(EWeatherType::LightRain) \
	op(EWeatherType::HeavyRain) \
	op(EWeatherType::Storm) \
	op(EWeatherType::Snow) \
	op(EWeatherType::Fog) 

enum class EWeatherType : uint8;
template<> struct TIsUEnumClass<EWeatherType> { enum { Value = true }; };
template<> ULTIMATEVFX_API UEnum* StaticEnum<EWeatherType>();

#define FOREACH_ENUM_EATMOSPHEREPRESET(op) \
	op(EAtmospherePreset::Earth) \
	op(EAtmospherePreset::Mars) \
	op(EAtmospherePreset::Alien) \
	op(EAtmospherePreset::Toxic) \
	op(EAtmospherePreset::Underwater) \
	op(EAtmospherePreset::Space) 

enum class EAtmospherePreset : uint8;
template<> struct TIsUEnumClass<EAtmospherePreset> { enum { Value = true }; };
template<> ULTIMATEVFX_API UEnum* StaticEnum<EAtmospherePreset>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
