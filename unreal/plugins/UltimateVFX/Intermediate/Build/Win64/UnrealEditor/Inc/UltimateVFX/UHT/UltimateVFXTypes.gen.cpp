// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Public/UltimateVFXTypes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeUltimateVFXTypes() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_AActor();
ULTIMATEVFX_API UClass* Z_Construct_UClass_APlaceholder();
ULTIMATEVFX_API UClass* Z_Construct_UClass_APlaceholder_NoRegister();
UPackage* Z_Construct_UPackage__Script_UltimateVFX();
// End Cross Module References

// Begin Class APlaceholder
void APlaceholder::StaticRegisterNativesAPlaceholder()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(APlaceholder);
UClass* Z_Construct_UClass_APlaceholder_NoRegister()
{
	return APlaceholder::StaticClass();
}
struct Z_Construct_UClass_APlaceholder_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "UltimateVFXTypes.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/UltimateVFXTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_dummy_MetaData[] = {
		{ "Category", "Placeholder" },
		{ "ModuleRelativePath", "Public/UltimateVFXTypes.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFloatPropertyParams NewProp_dummy;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<APlaceholder>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_APlaceholder_Statics::NewProp_dummy = { "dummy", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(APlaceholder, dummy), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_dummy_MetaData), NewProp_dummy_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_APlaceholder_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_APlaceholder_Statics::NewProp_dummy,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_APlaceholder_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_APlaceholder_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_UltimateVFX,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_APlaceholder_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_APlaceholder_Statics::ClassParams = {
	&APlaceholder::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_APlaceholder_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_APlaceholder_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_APlaceholder_Statics::Class_MetaDataParams), Z_Construct_UClass_APlaceholder_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_APlaceholder()
{
	if (!Z_Registration_Info_UClass_APlaceholder.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_APlaceholder.OuterSingleton, Z_Construct_UClass_APlaceholder_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_APlaceholder.OuterSingleton;
}
template<> ULTIMATEVFX_API UClass* StaticClass<APlaceholder>()
{
	return APlaceholder::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(APlaceholder);
APlaceholder::~APlaceholder() {}
// End Class APlaceholder

// Begin Registration
struct Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFXTypes_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_APlaceholder, APlaceholder::StaticClass, TEXT("APlaceholder"), &Z_Registration_Info_UClass_APlaceholder, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(APlaceholder), 2769956483U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFXTypes_h_2914191274(TEXT("/Script/UltimateVFX"),
	Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFXTypes_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_UltimateVFX_Source_Public_UltimateVFXTypes_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
