// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "SketchfabImporter/Public/SketchfabImporterTypes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeSketchfabImporterTypes() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_AActor();
ENGINE_API UClass* Z_Construct_UClass_UActorComponent();
ENGINE_API UClass* Z_Construct_UClass_UBlueprintFunctionLibrary();
ENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FTableRowBase();
SKETCHFABIMPORTER_API UClass* Z_Construct_UClass_ASketchfabImportManager();
SKETCHFABIMPORTER_API UClass* Z_Construct_UClass_ASketchfabImportManager_NoRegister();
SKETCHFABIMPORTER_API UClass* Z_Construct_UClass_UKainFunctionLibrary();
SKETCHFABIMPORTER_API UClass* Z_Construct_UClass_UKainFunctionLibrary_NoRegister();
SKETCHFABIMPORTER_API UClass* Z_Construct_UClass_USketchfabImporterComponent();
SKETCHFABIMPORTER_API UClass* Z_Construct_UClass_USketchfabImporterComponent_NoRegister();
SKETCHFABIMPORTER_API UEnum* Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode();
SKETCHFABIMPORTER_API UScriptStruct* Z_Construct_UScriptStruct_FSketchfabCache();
SKETCHFABIMPORTER_API UScriptStruct* Z_Construct_UScriptStruct_FSketchfabDownloadResult();
SKETCHFABIMPORTER_API UScriptStruct* Z_Construct_UScriptStruct_FSketchfabModel();
SKETCHFABIMPORTER_API UScriptStruct* Z_Construct_UScriptStruct_FSketchfabSearchOptions();
UPackage* Z_Construct_UPackage__Script_SketchfabImporter();
// End Cross Module References

// Begin Enum ESketchfabSortMode
static FEnumRegistrationInfo Z_Registration_Info_UEnum_ESketchfabSortMode;
static UEnum* ESketchfabSortMode_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_ESketchfabSortMode.OuterSingleton)
	{
		Z_Registration_Info_UEnum_ESketchfabSortMode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode, (UObject*)Z_Construct_UPackage__Script_SketchfabImporter(), TEXT("ESketchfabSortMode"));
	}
	return Z_Registration_Info_UEnum_ESketchfabSortMode.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UEnum* StaticEnum<ESketchfabSortMode>()
{
	return ESketchfabSortMode_StaticEnum();
}
struct Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "CreatedDate.DisplayName", "CreatedDate" },
		{ "CreatedDate.Name", "ESketchfabSortMode::CreatedDate" },
		{ "LikeCount.DisplayName", "LikeCount" },
		{ "LikeCount.Name", "ESketchfabSortMode::LikeCount" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
		{ "PublishedDate.DisplayName", "PublishedDate" },
		{ "PublishedDate.Name", "ESketchfabSortMode::PublishedDate" },
		{ "Relevance.DisplayName", "Relevance" },
		{ "Relevance.Name", "ESketchfabSortMode::Relevance" },
		{ "ViewCount.DisplayName", "ViewCount" },
		{ "ViewCount.Name", "ESketchfabSortMode::ViewCount" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "ESketchfabSortMode::Relevance", (int64)ESketchfabSortMode::Relevance },
		{ "ESketchfabSortMode::LikeCount", (int64)ESketchfabSortMode::LikeCount },
		{ "ESketchfabSortMode::ViewCount", (int64)ESketchfabSortMode::ViewCount },
		{ "ESketchfabSortMode::PublishedDate", (int64)ESketchfabSortMode::PublishedDate },
		{ "ESketchfabSortMode::CreatedDate", (int64)ESketchfabSortMode::CreatedDate },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_SketchfabImporter,
	nullptr,
	"ESketchfabSortMode",
	"ESketchfabSortMode",
	Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode_Statics::Enum_MetaDataParams), Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode()
{
	if (!Z_Registration_Info_UEnum_ESketchfabSortMode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_ESketchfabSortMode.InnerSingleton, Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_ESketchfabSortMode.InnerSingleton;
}
// End Enum ESketchfabSortMode

// Begin ScriptStruct FSketchfabModel
static_assert(std::is_polymorphic<FSketchfabModel>() == std::is_polymorphic<FTableRowBase>(), "USTRUCT FSketchfabModel cannot be polymorphic unless super FTableRowBase is polymorphic");
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_SketchfabModel;
class UScriptStruct* FSketchfabModel::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabModel.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_SketchfabModel.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FSketchfabModel, (UObject*)Z_Construct_UPackage__Script_SketchfabImporter(), TEXT("SketchfabModel"));
	}
	return Z_Registration_Info_UScriptStruct_SketchfabModel.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<FSketchfabModel>()
{
	return FSketchfabModel::StaticStruct();
}
struct Z_Construct_UScriptStruct_FSketchfabModel_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_uid_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_name_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_description_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_thumbnail_url_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_author_name_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_author_username_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_viewer_url_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_like_count_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_view_count_MetaData[] = {
		{ "Category", "SketchfabModel" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_uid;
	static const UECodeGen_Private::FStrPropertyParams NewProp_name;
	static const UECodeGen_Private::FStrPropertyParams NewProp_description;
	static const UECodeGen_Private::FStrPropertyParams NewProp_thumbnail_url;
	static const UECodeGen_Private::FStrPropertyParams NewProp_author_name;
	static const UECodeGen_Private::FStrPropertyParams NewProp_author_username;
	static const UECodeGen_Private::FStrPropertyParams NewProp_viewer_url;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_like_count;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_view_count;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FSketchfabModel>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_uid = { "uid", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_uid_MetaData), NewProp_uid_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_name = { "name", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, name), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_name_MetaData), NewProp_name_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_description = { "description", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, description), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_description_MetaData), NewProp_description_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_thumbnail_url = { "thumbnail_url", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, thumbnail_url), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_thumbnail_url_MetaData), NewProp_thumbnail_url_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_author_name = { "author_name", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, author_name), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_author_name_MetaData), NewProp_author_name_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_author_username = { "author_username", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, author_username), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_author_username_MetaData), NewProp_author_username_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_viewer_url = { "viewer_url", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, viewer_url), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_viewer_url_MetaData), NewProp_viewer_url_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_like_count = { "like_count", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, like_count), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_like_count_MetaData), NewProp_like_count_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_view_count = { "view_count", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabModel, view_count), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_view_count_MetaData), NewProp_view_count_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FSketchfabModel_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_name,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_description,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_thumbnail_url,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_author_name,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_author_username,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_viewer_url,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_like_count,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewProp_view_count,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabModel_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FSketchfabModel_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_SketchfabImporter,
	Z_Construct_UScriptStruct_FTableRowBase,
	&NewStructOps,
	"SketchfabModel",
	Z_Construct_UScriptStruct_FSketchfabModel_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabModel_Statics::PropPointers),
	sizeof(FSketchfabModel),
	alignof(FSketchfabModel),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabModel_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FSketchfabModel_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FSketchfabModel()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabModel.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_SketchfabModel.InnerSingleton, Z_Construct_UScriptStruct_FSketchfabModel_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_SketchfabModel.InnerSingleton;
}
// End ScriptStruct FSketchfabModel

// Begin ScriptStruct FSketchfabSearchOptions
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_SketchfabSearchOptions;
class UScriptStruct* FSketchfabSearchOptions::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabSearchOptions.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_SketchfabSearchOptions.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FSketchfabSearchOptions, (UObject*)Z_Construct_UPackage__Script_SketchfabImporter(), TEXT("SketchfabSearchOptions"));
	}
	return Z_Registration_Info_UScriptStruct_SketchfabSearchOptions.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<FSketchfabSearchOptions>()
{
	return FSketchfabSearchOptions::StaticStruct();
}
struct Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_query_MetaData[] = {
		{ "Category", "SketchfabSearchOptions" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_count_MetaData[] = {
		{ "Category", "SketchfabSearchOptions" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_downloadable_only_MetaData[] = {
		{ "Category", "SketchfabSearchOptions" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sort_by_MetaData[] = {
		{ "Category", "SketchfabSearchOptions" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_api_token_MetaData[] = {
		{ "Category", "SketchfabSearchOptions" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_query;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_count;
	static void NewProp_downloadable_only_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_downloadable_only;
	static const UECodeGen_Private::FBytePropertyParams NewProp_sort_by_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_sort_by;
	static const UECodeGen_Private::FStrPropertyParams NewProp_api_token;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FSketchfabSearchOptions>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_query = { "query", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabSearchOptions, query), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_query_MetaData), NewProp_query_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_count = { "count", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabSearchOptions, count), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_count_MetaData), NewProp_count_MetaData) };
void Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_downloadable_only_SetBit(void* Obj)
{
	((FSketchfabSearchOptions*)Obj)->downloadable_only = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_downloadable_only = { "downloadable_only", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FSketchfabSearchOptions), &Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_downloadable_only_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_downloadable_only_MetaData), NewProp_downloadable_only_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_sort_by_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_sort_by = { "sort_by", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabSearchOptions, sort_by), Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sort_by_MetaData), NewProp_sort_by_MetaData) }; // 1524689981
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_api_token = { "api_token", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabSearchOptions, api_token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_api_token_MetaData), NewProp_api_token_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_query,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_count,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_downloadable_only,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_sort_by_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_sort_by,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewProp_api_token,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_SketchfabImporter,
	nullptr,
	&NewStructOps,
	"SketchfabSearchOptions",
	Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::PropPointers),
	sizeof(FSketchfabSearchOptions),
	alignof(FSketchfabSearchOptions),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FSketchfabSearchOptions()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabSearchOptions.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_SketchfabSearchOptions.InnerSingleton, Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_SketchfabSearchOptions.InnerSingleton;
}
// End ScriptStruct FSketchfabSearchOptions

// Begin ScriptStruct FSketchfabDownloadResult
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_SketchfabDownloadResult;
class UScriptStruct* FSketchfabDownloadResult::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabDownloadResult.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_SketchfabDownloadResult.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FSketchfabDownloadResult, (UObject*)Z_Construct_UPackage__Script_SketchfabImporter(), TEXT("SketchfabDownloadResult"));
	}
	return Z_Registration_Info_UScriptStruct_SketchfabDownloadResult.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<FSketchfabDownloadResult>()
{
	return FSketchfabDownloadResult::StaticStruct();
}
struct Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_success_MetaData[] = {
		{ "Category", "SketchfabDownloadResult" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_download_url_MetaData[] = {
		{ "Category", "SketchfabDownloadResult" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_format_MetaData[] = {
		{ "Category", "SketchfabDownloadResult" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_error_message_MetaData[] = {
		{ "Category", "SketchfabDownloadResult" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static void NewProp_success_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_success;
	static const UECodeGen_Private::FStrPropertyParams NewProp_download_url;
	static const UECodeGen_Private::FStrPropertyParams NewProp_format;
	static const UECodeGen_Private::FStrPropertyParams NewProp_error_message;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FSketchfabDownloadResult>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
void Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_success_SetBit(void* Obj)
{
	((FSketchfabDownloadResult*)Obj)->success = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_success = { "success", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FSketchfabDownloadResult), &Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_success_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_success_MetaData), NewProp_success_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_download_url = { "download_url", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabDownloadResult, download_url), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_download_url_MetaData), NewProp_download_url_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_format = { "format", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabDownloadResult, format), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_format_MetaData), NewProp_format_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_error_message = { "error_message", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabDownloadResult, error_message), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_error_message_MetaData), NewProp_error_message_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_success,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_download_url,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_format,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewProp_error_message,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_SketchfabImporter,
	nullptr,
	&NewStructOps,
	"SketchfabDownloadResult",
	Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::PropPointers),
	sizeof(FSketchfabDownloadResult),
	alignof(FSketchfabDownloadResult),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FSketchfabDownloadResult()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabDownloadResult.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_SketchfabDownloadResult.InnerSingleton, Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_SketchfabDownloadResult.InnerSingleton;
}
// End ScriptStruct FSketchfabDownloadResult

// Begin ScriptStruct FSketchfabCache
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_SketchfabCache;
class UScriptStruct* FSketchfabCache::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabCache.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_SketchfabCache.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FSketchfabCache, (UObject*)Z_Construct_UPackage__Script_SketchfabImporter(), TEXT("SketchfabCache"));
	}
	return Z_Registration_Info_UScriptStruct_SketchfabCache.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<FSketchfabCache>()
{
	return FSketchfabCache::StaticStruct();
}
struct Z_Construct_UScriptStruct_FSketchfabCache_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cached_searches_MetaData[] = {
		{ "Category", "SketchfabCache" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cached_models_MetaData[] = {
		{ "Category", "SketchfabCache" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cache_timestamp_MetaData[] = {
		{ "Category", "SketchfabCache" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_max_cache_age_MetaData[] = {
		{ "Category", "SketchfabCache" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_cached_searches_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_cached_searches;
	static const UECodeGen_Private::FStructPropertyParams NewProp_cached_models_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_cached_models;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_cache_timestamp;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_max_cache_age;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FSketchfabCache>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_searches_Inner = { "cached_searches", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_searches = { "cached_searches", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabCache, cached_searches), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cached_searches_MetaData), NewProp_cached_searches_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_models_Inner = { "cached_models", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FSketchfabModel, METADATA_PARAMS(0, nullptr) }; // 2784328974
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_models = { "cached_models", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabCache, cached_models), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cached_models_MetaData), NewProp_cached_models_MetaData) }; // 2784328974
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cache_timestamp = { "cache_timestamp", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabCache, cache_timestamp), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cache_timestamp_MetaData), NewProp_cache_timestamp_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_max_cache_age = { "max_cache_age", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FSketchfabCache, max_cache_age), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_max_cache_age_MetaData), NewProp_max_cache_age_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FSketchfabCache_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_searches_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_searches,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_models_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cached_models,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_cache_timestamp,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewProp_max_cache_age,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabCache_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FSketchfabCache_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_SketchfabImporter,
	nullptr,
	&NewStructOps,
	"SketchfabCache",
	Z_Construct_UScriptStruct_FSketchfabCache_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabCache_Statics::PropPointers),
	sizeof(FSketchfabCache),
	alignof(FSketchfabCache),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FSketchfabCache_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FSketchfabCache_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FSketchfabCache()
{
	if (!Z_Registration_Info_UScriptStruct_SketchfabCache.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_SketchfabCache.InnerSingleton, Z_Construct_UScriptStruct_FSketchfabCache_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_SketchfabCache.InnerSingleton;
}
// End ScriptStruct FSketchfabCache

// Begin Class USketchfabImporterComponent
void USketchfabImporterComponent::StaticRegisterNativesUSketchfabImporterComponent()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(USketchfabImporterComponent);
UClass* Z_Construct_UClass_USketchfabImporterComponent_NoRegister()
{
	return USketchfabImporterComponent::StaticClass();
}
struct Z_Construct_UClass_USketchfabImporterComponent_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintSpawnableComponent", "" },
		{ "ClassGroupNames", "Custom" },
		{ "IncludePath", "SketchfabImporterTypes.h" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_api_token_MetaData[] = {
		{ "Category", "SketchfabImporterComponent" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_last_search_query_MetaData[] = {
		{ "Category", "SketchfabImporterComponent" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_search_results_MetaData[] = {
		{ "Category", "SketchfabImporterComponent" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_is_searching_MetaData[] = {
		{ "Category", "SketchfabImporterComponent" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_api_token;
	static const UECodeGen_Private::FStrPropertyParams NewProp_last_search_query;
	static const UECodeGen_Private::FStructPropertyParams NewProp_search_results_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_search_results;
	static void NewProp_is_searching_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_is_searching;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<USketchfabImporterComponent>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_api_token = { "api_token", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(USketchfabImporterComponent, api_token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_api_token_MetaData), NewProp_api_token_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_last_search_query = { "last_search_query", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(USketchfabImporterComponent, last_search_query), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_last_search_query_MetaData), NewProp_last_search_query_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_search_results_Inner = { "search_results", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FSketchfabModel, METADATA_PARAMS(0, nullptr) }; // 2784328974
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_search_results = { "search_results", nullptr, (EPropertyFlags)0x0010000000002005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(USketchfabImporterComponent, search_results), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_search_results_MetaData), NewProp_search_results_MetaData) }; // 2784328974
void Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_is_searching_SetBit(void* Obj)
{
	((USketchfabImporterComponent*)Obj)->is_searching = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_is_searching = { "is_searching", nullptr, (EPropertyFlags)0x0010000000002005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(USketchfabImporterComponent), &Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_is_searching_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_is_searching_MetaData), NewProp_is_searching_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_USketchfabImporterComponent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_api_token,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_last_search_query,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_search_results_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_search_results,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_USketchfabImporterComponent_Statics::NewProp_is_searching,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_USketchfabImporterComponent_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_USketchfabImporterComponent_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UActorComponent,
	(UObject* (*)())Z_Construct_UPackage__Script_SketchfabImporter,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_USketchfabImporterComponent_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_USketchfabImporterComponent_Statics::ClassParams = {
	&USketchfabImporterComponent::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_USketchfabImporterComponent_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_USketchfabImporterComponent_Statics::PropPointers),
	0,
	0x00B000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_USketchfabImporterComponent_Statics::Class_MetaDataParams), Z_Construct_UClass_USketchfabImporterComponent_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_USketchfabImporterComponent()
{
	if (!Z_Registration_Info_UClass_USketchfabImporterComponent.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_USketchfabImporterComponent.OuterSingleton, Z_Construct_UClass_USketchfabImporterComponent_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_USketchfabImporterComponent.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UClass* StaticClass<USketchfabImporterComponent>()
{
	return USketchfabImporterComponent::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(USketchfabImporterComponent);
USketchfabImporterComponent::~USketchfabImporterComponent() {}
// End Class USketchfabImporterComponent

// Begin Class ASketchfabImportManager Function Client_UpdateProgress
struct SketchfabImportManager_eventClient_UpdateProgress_Parms
{
	FString model_uid;
	float progress;
};
static FName NAME_ASketchfabImportManager_Client_UpdateProgress = FName(TEXT("Client_UpdateProgress"));
void ASketchfabImportManager::Client_UpdateProgress(const FString& model_uid, const float progress)
{
	SketchfabImportManager_eventClient_UpdateProgress_Parms Parms;
	Parms.model_uid=model_uid;
	Parms.progress=progress;
	ProcessEvent(FindFunctionChecked(NAME_ASketchfabImportManager_Client_UpdateProgress),&Parms);
}
struct Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Client_UpdateProgress" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_progress_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_progress;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventClient_UpdateProgress_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::NewProp_progress = { "progress", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventClient_UpdateProgress_Parms, progress), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_progress_MetaData), NewProp_progress_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::NewProp_model_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::NewProp_progress,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_ASketchfabImportManager, nullptr, "Client_UpdateProgress", nullptr, nullptr, Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::PropPointers), sizeof(SketchfabImportManager_eventClient_UpdateProgress_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x05020CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::Function_MetaDataParams), Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::Function_MetaDataParams) };
static_assert(sizeof(SketchfabImportManager_eventClient_UpdateProgress_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ASketchfabImportManager::execClient_UpdateProgress)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_GET_PROPERTY(FFloatProperty,Z_Param_progress);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Client_UpdateProgress_Implementation(Z_Param_model_uid,Z_Param_progress);
	P_NATIVE_END;
}
// End Class ASketchfabImportManager Function Client_UpdateProgress

// Begin Class ASketchfabImportManager Function Multicast_NotifyDownloadCancelled
struct SketchfabImportManager_eventMulticast_NotifyDownloadCancelled_Parms
{
	FString model_uid;
};
static FName NAME_ASketchfabImportManager_Multicast_NotifyDownloadCancelled = FName(TEXT("Multicast_NotifyDownloadCancelled"));
void ASketchfabImportManager::Multicast_NotifyDownloadCancelled(const FString& model_uid)
{
	SketchfabImportManager_eventMulticast_NotifyDownloadCancelled_Parms Parms;
	Parms.model_uid=model_uid;
	ProcessEvent(FindFunctionChecked(NAME_ASketchfabImportManager_Multicast_NotifyDownloadCancelled),&Parms);
}
struct Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Multicast_NotifyDownloadCancelled" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventMulticast_NotifyDownloadCancelled_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::NewProp_model_uid,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_ASketchfabImportManager, nullptr, "Multicast_NotifyDownloadCancelled", nullptr, nullptr, Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::PropPointers), sizeof(SketchfabImportManager_eventMulticast_NotifyDownloadCancelled_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04024CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::Function_MetaDataParams), Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::Function_MetaDataParams) };
static_assert(sizeof(SketchfabImportManager_eventMulticast_NotifyDownloadCancelled_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ASketchfabImportManager::execMulticast_NotifyDownloadCancelled)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Multicast_NotifyDownloadCancelled_Implementation(Z_Param_model_uid);
	P_NATIVE_END;
}
// End Class ASketchfabImportManager Function Multicast_NotifyDownloadCancelled

// Begin Class ASketchfabImportManager Function Multicast_NotifyDownloadComplete
struct SketchfabImportManager_eventMulticast_NotifyDownloadComplete_Parms
{
	FString model_uid;
	bool success;
};
static FName NAME_ASketchfabImportManager_Multicast_NotifyDownloadComplete = FName(TEXT("Multicast_NotifyDownloadComplete"));
void ASketchfabImportManager::Multicast_NotifyDownloadComplete(const FString& model_uid, bool success)
{
	SketchfabImportManager_eventMulticast_NotifyDownloadComplete_Parms Parms;
	Parms.model_uid=model_uid;
	Parms.success=success ? true : false;
	ProcessEvent(FindFunctionChecked(NAME_ASketchfabImportManager_Multicast_NotifyDownloadComplete),&Parms);
}
struct Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Multicast_NotifyDownloadComplete" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_success_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static void NewProp_success_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_success;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventMulticast_NotifyDownloadComplete_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
void Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::NewProp_success_SetBit(void* Obj)
{
	((SketchfabImportManager_eventMulticast_NotifyDownloadComplete_Parms*)Obj)->success = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::NewProp_success = { "success", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(SketchfabImportManager_eventMulticast_NotifyDownloadComplete_Parms), &Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::NewProp_success_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_success_MetaData), NewProp_success_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::NewProp_model_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::NewProp_success,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_ASketchfabImportManager, nullptr, "Multicast_NotifyDownloadComplete", nullptr, nullptr, Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::PropPointers), sizeof(SketchfabImportManager_eventMulticast_NotifyDownloadComplete_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04024CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::Function_MetaDataParams), Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::Function_MetaDataParams) };
static_assert(sizeof(SketchfabImportManager_eventMulticast_NotifyDownloadComplete_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ASketchfabImportManager::execMulticast_NotifyDownloadComplete)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_GET_UBOOL(Z_Param_success);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Multicast_NotifyDownloadComplete_Implementation(Z_Param_model_uid,Z_Param_success);
	P_NATIVE_END;
}
// End Class ASketchfabImportManager Function Multicast_NotifyDownloadComplete

// Begin Class ASketchfabImportManager Function Multicast_NotifyDownloadStarted
struct SketchfabImportManager_eventMulticast_NotifyDownloadStarted_Parms
{
	FString model_uid;
};
static FName NAME_ASketchfabImportManager_Multicast_NotifyDownloadStarted = FName(TEXT("Multicast_NotifyDownloadStarted"));
void ASketchfabImportManager::Multicast_NotifyDownloadStarted(const FString& model_uid)
{
	SketchfabImportManager_eventMulticast_NotifyDownloadStarted_Parms Parms;
	Parms.model_uid=model_uid;
	ProcessEvent(FindFunctionChecked(NAME_ASketchfabImportManager_Multicast_NotifyDownloadStarted),&Parms);
}
struct Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Multicast_NotifyDownloadStarted" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventMulticast_NotifyDownloadStarted_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::NewProp_model_uid,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_ASketchfabImportManager, nullptr, "Multicast_NotifyDownloadStarted", nullptr, nullptr, Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::PropPointers), sizeof(SketchfabImportManager_eventMulticast_NotifyDownloadStarted_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04024CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::Function_MetaDataParams), Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::Function_MetaDataParams) };
static_assert(sizeof(SketchfabImportManager_eventMulticast_NotifyDownloadStarted_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ASketchfabImportManager::execMulticast_NotifyDownloadStarted)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Multicast_NotifyDownloadStarted_Implementation(Z_Param_model_uid);
	P_NATIVE_END;
}
// End Class ASketchfabImportManager Function Multicast_NotifyDownloadStarted

// Begin Class ASketchfabImportManager Function Server_CancelDownload
struct SketchfabImportManager_eventServer_CancelDownload_Parms
{
	FString model_uid;
};
static FName NAME_ASketchfabImportManager_Server_CancelDownload = FName(TEXT("Server_CancelDownload"));
void ASketchfabImportManager::Server_CancelDownload(const FString& model_uid)
{
	SketchfabImportManager_eventServer_CancelDownload_Parms Parms;
	Parms.model_uid=model_uid;
	ProcessEvent(FindFunctionChecked(NAME_ASketchfabImportManager_Server_CancelDownload),&Parms);
}
struct Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Server_CancelDownload" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventServer_CancelDownload_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::NewProp_model_uid,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_ASketchfabImportManager, nullptr, "Server_CancelDownload", nullptr, nullptr, Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::PropPointers), sizeof(SketchfabImportManager_eventServer_CancelDownload_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04220CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::Function_MetaDataParams), Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::Function_MetaDataParams) };
static_assert(sizeof(SketchfabImportManager_eventServer_CancelDownload_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ASketchfabImportManager::execServer_CancelDownload)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Server_CancelDownload_Implementation(Z_Param_model_uid);
	P_NATIVE_END;
}
// End Class ASketchfabImportManager Function Server_CancelDownload

// Begin Class ASketchfabImportManager Function Server_StartDownload
struct SketchfabImportManager_eventServer_StartDownload_Parms
{
	FString model_uid;
	FString api_token;
};
static FName NAME_ASketchfabImportManager_Server_StartDownload = FName(TEXT("Server_StartDownload"));
void ASketchfabImportManager::Server_StartDownload(const FString& model_uid, const FString& api_token)
{
	SketchfabImportManager_eventServer_StartDownload_Parms Parms;
	Parms.model_uid=model_uid;
	Parms.api_token=api_token;
	ProcessEvent(FindFunctionChecked(NAME_ASketchfabImportManager_Server_StartDownload),&Parms);
}
struct Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Server_StartDownload" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_api_token_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FStrPropertyParams NewProp_api_token;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventServer_StartDownload_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::NewProp_api_token = { "api_token", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(SketchfabImportManager_eventServer_StartDownload_Parms, api_token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_api_token_MetaData), NewProp_api_token_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::NewProp_model_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::NewProp_api_token,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_ASketchfabImportManager, nullptr, "Server_StartDownload", nullptr, nullptr, Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::PropPointers), sizeof(SketchfabImportManager_eventServer_StartDownload_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04220CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::Function_MetaDataParams), Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::Function_MetaDataParams) };
static_assert(sizeof(SketchfabImportManager_eventServer_StartDownload_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ASketchfabImportManager::execServer_StartDownload)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_GET_PROPERTY(FStrProperty,Z_Param_api_token);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Server_StartDownload_Implementation(Z_Param_model_uid,Z_Param_api_token);
	P_NATIVE_END;
}
// End Class ASketchfabImportManager Function Server_StartDownload

// Begin Class ASketchfabImportManager
void ASketchfabImportManager::StaticRegisterNativesASketchfabImportManager()
{
	UClass* Class = ASketchfabImportManager::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "Client_UpdateProgress", &ASketchfabImportManager::execClient_UpdateProgress },
		{ "Multicast_NotifyDownloadCancelled", &ASketchfabImportManager::execMulticast_NotifyDownloadCancelled },
		{ "Multicast_NotifyDownloadComplete", &ASketchfabImportManager::execMulticast_NotifyDownloadComplete },
		{ "Multicast_NotifyDownloadStarted", &ASketchfabImportManager::execMulticast_NotifyDownloadStarted },
		{ "Server_CancelDownload", &ASketchfabImportManager::execServer_CancelDownload },
		{ "Server_StartDownload", &ASketchfabImportManager::execServer_StartDownload },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ASketchfabImportManager);
UClass* Z_Construct_UClass_ASketchfabImportManager_NoRegister()
{
	return ASketchfabImportManager::StaticClass();
}
struct Z_Construct_UClass_ASketchfabImportManager_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "SketchfabImporterTypes.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_current_downloads_MetaData[] = {
		{ "Category", "SketchfabImportManager" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_import_queue_MetaData[] = {
		{ "Category", "SketchfabImportManager" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_max_concurrent_downloads_MetaData[] = {
		{ "Category", "SketchfabImportManager" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_current_downloads_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_current_downloads;
	static const UECodeGen_Private::FStrPropertyParams NewProp_import_queue_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_import_queue;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_max_concurrent_downloads;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_ASketchfabImportManager_Client_UpdateProgress, "Client_UpdateProgress" }, // 2393231995
		{ &Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadCancelled, "Multicast_NotifyDownloadCancelled" }, // 625779399
		{ &Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadComplete, "Multicast_NotifyDownloadComplete" }, // 2263782909
		{ &Z_Construct_UFunction_ASketchfabImportManager_Multicast_NotifyDownloadStarted, "Multicast_NotifyDownloadStarted" }, // 1875862884
		{ &Z_Construct_UFunction_ASketchfabImportManager_Server_CancelDownload, "Server_CancelDownload" }, // 1427025647
		{ &Z_Construct_UFunction_ASketchfabImportManager_Server_StartDownload, "Server_StartDownload" }, // 120018897
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ASketchfabImportManager>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_current_downloads_Inner = { "current_downloads", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_current_downloads = { "current_downloads", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ASketchfabImportManager, current_downloads), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_current_downloads_MetaData), NewProp_current_downloads_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_import_queue_Inner = { "import_queue", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_import_queue = { "import_queue", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ASketchfabImportManager, import_queue), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_import_queue_MetaData), NewProp_import_queue_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_max_concurrent_downloads = { "max_concurrent_downloads", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ASketchfabImportManager, max_concurrent_downloads), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_max_concurrent_downloads_MetaData), NewProp_max_concurrent_downloads_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ASketchfabImportManager_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_current_downloads_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_current_downloads,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_import_queue_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_import_queue,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ASketchfabImportManager_Statics::NewProp_max_concurrent_downloads,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ASketchfabImportManager_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_ASketchfabImportManager_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_SketchfabImporter,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ASketchfabImportManager_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_ASketchfabImportManager_Statics::ClassParams = {
	&ASketchfabImportManager::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_ASketchfabImportManager_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_ASketchfabImportManager_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_ASketchfabImportManager_Statics::Class_MetaDataParams), Z_Construct_UClass_ASketchfabImportManager_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_ASketchfabImportManager()
{
	if (!Z_Registration_Info_UClass_ASketchfabImportManager.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_ASketchfabImportManager.OuterSingleton, Z_Construct_UClass_ASketchfabImportManager_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_ASketchfabImportManager.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UClass* StaticClass<ASketchfabImportManager>()
{
	return ASketchfabImportManager::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(ASketchfabImportManager);
ASketchfabImportManager::~ASketchfabImportManager() {}
// End Class ASketchfabImportManager

// Begin Class UKainFunctionLibrary Function build_search_url
struct Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics
{
	struct KainFunctionLibrary_eventbuild_search_url_Parms
	{
		FString query;
		int64 count;
		FString sort_mode;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_query_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_count_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sort_mode_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_query;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_count;
	static const UECodeGen_Private::FStrPropertyParams NewProp_sort_mode;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_query = { "query", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventbuild_search_url_Parms, query), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_query_MetaData), NewProp_query_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_count = { "count", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventbuild_search_url_Parms, count), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_count_MetaData), NewProp_count_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_sort_mode = { "sort_mode", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventbuild_search_url_Parms, sort_mode), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sort_mode_MetaData), NewProp_sort_mode_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventbuild_search_url_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_query,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_count,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_sort_mode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "build_search_url", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::KainFunctionLibrary_eventbuild_search_url_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::KainFunctionLibrary_eventbuild_search_url_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_build_search_url()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_build_search_url_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execbuild_search_url)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_query);
	P_GET_PROPERTY(FInt64Property,Z_Param_count);
	P_GET_PROPERTY(FStrProperty,Z_Param_sort_mode);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UKainFunctionLibrary::build_search_url(Z_Param_query,Z_Param_count,Z_Param_sort_mode);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function build_search_url

// Begin Class UKainFunctionLibrary Function clear_cache
struct Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics
{
	struct KainFunctionLibrary_eventclear_cache_Parms
	{
		FSketchfabCache cache;
		FSketchfabCache ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cache_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_cache;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::NewProp_cache = { "cache", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventclear_cache_Parms, cache), Z_Construct_UScriptStruct_FSketchfabCache, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cache_MetaData), NewProp_cache_MetaData) }; // 1972516345
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventclear_cache_Parms, ReturnValue), Z_Construct_UScriptStruct_FSketchfabCache, METADATA_PARAMS(0, nullptr) }; // 1972516345
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::NewProp_cache,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "clear_cache", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::KainFunctionLibrary_eventclear_cache_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::KainFunctionLibrary_eventclear_cache_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_clear_cache()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_clear_cache_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execclear_cache)
{
	P_GET_STRUCT(FSketchfabCache,Z_Param_cache);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FSketchfabCache*)Z_Param__Result=UKainFunctionLibrary::clear_cache(Z_Param_cache);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function clear_cache

// Begin Class UKainFunctionLibrary Function create_auth_header
struct Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics
{
	struct KainFunctionLibrary_eventcreate_auth_header_Parms
	{
		FString api_token;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_api_token_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_api_token;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::NewProp_api_token = { "api_token", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventcreate_auth_header_Parms, api_token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_api_token_MetaData), NewProp_api_token_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventcreate_auth_header_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::NewProp_api_token,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "create_auth_header", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::KainFunctionLibrary_eventcreate_auth_header_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::KainFunctionLibrary_eventcreate_auth_header_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execcreate_auth_header)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_api_token);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UKainFunctionLibrary::create_auth_header(Z_Param_api_token);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function create_auth_header

// Begin Class UKainFunctionLibrary Function download_and_import_model
struct Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics
{
	struct KainFunctionLibrary_eventdownload_and_import_model_Parms
	{
		FString model_uid;
		FString api_token;
		FString import_path;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_api_token_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_import_path_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FStrPropertyParams NewProp_api_token;
	static const UECodeGen_Private::FStrPropertyParams NewProp_import_path;
	static void NewProp_ReturnValue_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventdownload_and_import_model_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_api_token = { "api_token", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventdownload_and_import_model_Parms, api_token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_api_token_MetaData), NewProp_api_token_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_import_path = { "import_path", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventdownload_and_import_model_Parms, import_path), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_import_path_MetaData), NewProp_import_path_MetaData) };
void Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_ReturnValue_SetBit(void* Obj)
{
	((KainFunctionLibrary_eventdownload_and_import_model_Parms*)Obj)->ReturnValue = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(KainFunctionLibrary_eventdownload_and_import_model_Parms), &Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_model_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_api_token,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_import_path,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "download_and_import_model", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::KainFunctionLibrary_eventdownload_and_import_model_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::KainFunctionLibrary_eventdownload_and_import_model_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execdownload_and_import_model)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_GET_PROPERTY(FStrProperty,Z_Param_api_token);
	P_GET_PROPERTY(FStrProperty,Z_Param_import_path);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=UKainFunctionLibrary::download_and_import_model(Z_Param_model_uid,Z_Param_api_token,Z_Param_import_path);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function download_and_import_model

// Begin Class UKainFunctionLibrary Function format_model_info
struct Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics
{
	struct KainFunctionLibrary_eventformat_model_info_Parms
	{
		FSketchfabModel model;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_model;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::NewProp_model = { "model", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventformat_model_info_Parms, model), Z_Construct_UScriptStruct_FSketchfabModel, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_MetaData), NewProp_model_MetaData) }; // 2784328974
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventformat_model_info_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::NewProp_model,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "format_model_info", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::KainFunctionLibrary_eventformat_model_info_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::KainFunctionLibrary_eventformat_model_info_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_format_model_info()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_format_model_info_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execformat_model_info)
{
	P_GET_STRUCT(FSketchfabModel,Z_Param_model);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UKainFunctionLibrary::format_model_info(Z_Param_model);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function format_model_info

// Begin Class UKainFunctionLibrary Function get_api_base_url
struct Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics
{
	struct KainFunctionLibrary_eventget_api_base_url_Parms
	{
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_api_base_url_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "get_api_base_url", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::KainFunctionLibrary_eventget_api_base_url_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::KainFunctionLibrary_eventget_api_base_url_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execget_api_base_url)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UKainFunctionLibrary::get_api_base_url();
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function get_api_base_url

// Begin Class UKainFunctionLibrary Function get_download_url
struct Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics
{
	struct KainFunctionLibrary_eventget_download_url_Parms
	{
		FString model_uid;
		FString api_token;
		FSketchfabDownloadResult ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_api_token_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FStrPropertyParams NewProp_api_token;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_download_url_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::NewProp_api_token = { "api_token", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_download_url_Parms, api_token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_api_token_MetaData), NewProp_api_token_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_download_url_Parms, ReturnValue), Z_Construct_UScriptStruct_FSketchfabDownloadResult, METADATA_PARAMS(0, nullptr) }; // 1567494457
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::NewProp_model_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::NewProp_api_token,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "get_download_url", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::KainFunctionLibrary_eventget_download_url_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::KainFunctionLibrary_eventget_download_url_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_get_download_url()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_get_download_url_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execget_download_url)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_GET_PROPERTY(FStrProperty,Z_Param_api_token);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FSketchfabDownloadResult*)Z_Param__Result=UKainFunctionLibrary::get_download_url(Z_Param_model_uid,Z_Param_api_token);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function get_download_url

// Begin Class UKainFunctionLibrary Function get_model_viewer_url
struct Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics
{
	struct KainFunctionLibrary_eventget_model_viewer_url_Parms
	{
		FString model_uid;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_model_uid;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::NewProp_model_uid = { "model_uid", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_model_viewer_url_Parms, model_uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_uid_MetaData), NewProp_model_uid_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_model_viewer_url_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::NewProp_model_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "get_model_viewer_url", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::KainFunctionLibrary_eventget_model_viewer_url_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::KainFunctionLibrary_eventget_model_viewer_url_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execget_model_viewer_url)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_model_uid);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UKainFunctionLibrary::get_model_viewer_url(Z_Param_model_uid);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function get_model_viewer_url

// Begin Class UKainFunctionLibrary Function get_sort_mode_string
struct Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics
{
	struct KainFunctionLibrary_eventget_sort_mode_string_Parms
	{
		ESketchfabSortMode mode;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
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
const UECodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::NewProp_mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::NewProp_mode = { "mode", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_sort_mode_string_Parms, mode), Z_Construct_UEnum_SketchfabImporter_ESketchfabSortMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_mode_MetaData), NewProp_mode_MetaData) }; // 1524689981
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventget_sort_mode_string_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::NewProp_mode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::NewProp_mode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "get_sort_mode_string", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::KainFunctionLibrary_eventget_sort_mode_string_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::KainFunctionLibrary_eventget_sort_mode_string_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execget_sort_mode_string)
{
	P_GET_ENUM(ESketchfabSortMode,Z_Param_mode);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UKainFunctionLibrary::get_sort_mode_string(ESketchfabSortMode(Z_Param_mode));
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function get_sort_mode_string

// Begin Class UKainFunctionLibrary Function is_cache_valid
struct Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics
{
	struct KainFunctionLibrary_eventis_cache_valid_Parms
	{
		FSketchfabCache cache;
		float current_time;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_cache_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_current_time_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_cache;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_current_time;
	static void NewProp_ReturnValue_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_cache = { "cache", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventis_cache_valid_Parms, cache), Z_Construct_UScriptStruct_FSketchfabCache, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_cache_MetaData), NewProp_cache_MetaData) }; // 1972516345
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_current_time = { "current_time", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventis_cache_valid_Parms, current_time), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_current_time_MetaData), NewProp_current_time_MetaData) };
void Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_ReturnValue_SetBit(void* Obj)
{
	((KainFunctionLibrary_eventis_cache_valid_Parms*)Obj)->ReturnValue = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(KainFunctionLibrary_eventis_cache_valid_Parms), &Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_cache,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_current_time,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "is_cache_valid", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::KainFunctionLibrary_eventis_cache_valid_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::KainFunctionLibrary_eventis_cache_valid_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execis_cache_valid)
{
	P_GET_STRUCT(FSketchfabCache,Z_Param_cache);
	P_GET_PROPERTY(FFloatProperty,Z_Param_current_time);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=UKainFunctionLibrary::is_cache_valid(Z_Param_cache,Z_Param_current_time);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function is_cache_valid

// Begin Class UKainFunctionLibrary Function is_valid_model_uid
struct Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics
{
	struct KainFunctionLibrary_eventis_valid_model_uid_Parms
	{
		FString uid;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_uid_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_uid;
	static void NewProp_ReturnValue_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::NewProp_uid = { "uid", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventis_valid_model_uid_Parms, uid), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_uid_MetaData), NewProp_uid_MetaData) };
void Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::NewProp_ReturnValue_SetBit(void* Obj)
{
	((KainFunctionLibrary_eventis_valid_model_uid_Parms*)Obj)->ReturnValue = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(KainFunctionLibrary_eventis_valid_model_uid_Parms), &Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::NewProp_uid,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "is_valid_model_uid", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::KainFunctionLibrary_eventis_valid_model_uid_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::KainFunctionLibrary_eventis_valid_model_uid_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execis_valid_model_uid)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_uid);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=UKainFunctionLibrary::is_valid_model_uid(Z_Param_uid);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function is_valid_model_uid

// Begin Class UKainFunctionLibrary Function parse_thumbnail_url
struct Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics
{
	struct KainFunctionLibrary_eventparse_thumbnail_url_Parms
	{
		FSketchfabModel model;
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_model_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_model;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::NewProp_model = { "model", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventparse_thumbnail_url_Parms, model), Z_Construct_UScriptStruct_FSketchfabModel, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_model_MetaData), NewProp_model_MetaData) }; // 2784328974
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventparse_thumbnail_url_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::NewProp_model,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "parse_thumbnail_url", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::KainFunctionLibrary_eventparse_thumbnail_url_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::KainFunctionLibrary_eventparse_thumbnail_url_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execparse_thumbnail_url)
{
	P_GET_STRUCT(FSketchfabModel,Z_Param_model);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=UKainFunctionLibrary::parse_thumbnail_url(Z_Param_model);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function parse_thumbnail_url

// Begin Class UKainFunctionLibrary Function search_sketchfab
struct Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics
{
	struct KainFunctionLibrary_eventsearch_sketchfab_Parms
	{
		FString query;
		int64 count;
		FString api_token;
		TArray<FSketchfabModel> ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_query_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_count_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_api_token_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_query;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_count;
	static const UECodeGen_Private::FStrPropertyParams NewProp_api_token;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_query = { "query", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventsearch_sketchfab_Parms, query), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_query_MetaData), NewProp_query_MetaData) };
const UECodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_count = { "count", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventsearch_sketchfab_Parms, count), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_count_MetaData), NewProp_count_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_api_token = { "api_token", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventsearch_sketchfab_Parms, api_token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_api_token_MetaData), NewProp_api_token_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_ReturnValue_Inner = { "ReturnValue", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FSketchfabModel, METADATA_PARAMS(0, nullptr) }; // 2784328974
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventsearch_sketchfab_Parms, ReturnValue), EArrayPropertyFlags::None, METADATA_PARAMS(0, nullptr) }; // 2784328974
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_query,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_count,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_api_token,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_ReturnValue_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "search_sketchfab", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::KainFunctionLibrary_eventsearch_sketchfab_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::KainFunctionLibrary_eventsearch_sketchfab_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execsearch_sketchfab)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_query);
	P_GET_PROPERTY(FInt64Property,Z_Param_count);
	P_GET_PROPERTY(FStrProperty,Z_Param_api_token);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(TArray<FSketchfabModel>*)Z_Param__Result=UKainFunctionLibrary::search_sketchfab(Z_Param_query,Z_Param_count,Z_Param_api_token);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function search_sketchfab

// Begin Class UKainFunctionLibrary Function validate_api_token
struct Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics
{
	struct KainFunctionLibrary_eventvalidate_api_token_Parms
	{
		FString token;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_token_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_token;
	static void NewProp_ReturnValue_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::NewProp_token = { "token", nullptr, (EPropertyFlags)0x0010000000000082, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(KainFunctionLibrary_eventvalidate_api_token_Parms, token), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_token_MetaData), NewProp_token_MetaData) };
void Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::NewProp_ReturnValue_SetBit(void* Obj)
{
	((KainFunctionLibrary_eventvalidate_api_token_Parms*)Obj)->ReturnValue = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(KainFunctionLibrary_eventvalidate_api_token_Parms), &Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::NewProp_token,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UKainFunctionLibrary, nullptr, "validate_api_token", nullptr, nullptr, Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::PropPointers), sizeof(Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::KainFunctionLibrary_eventvalidate_api_token_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::Function_MetaDataParams), Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::Function_MetaDataParams) };
static_assert(sizeof(Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::KainFunctionLibrary_eventvalidate_api_token_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UKainFunctionLibrary::execvalidate_api_token)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_token);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=UKainFunctionLibrary::validate_api_token(Z_Param_token);
	P_NATIVE_END;
}
// End Class UKainFunctionLibrary Function validate_api_token

// Begin Class UKainFunctionLibrary
void UKainFunctionLibrary::StaticRegisterNativesUKainFunctionLibrary()
{
	UClass* Class = UKainFunctionLibrary::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "build_search_url", &UKainFunctionLibrary::execbuild_search_url },
		{ "clear_cache", &UKainFunctionLibrary::execclear_cache },
		{ "create_auth_header", &UKainFunctionLibrary::execcreate_auth_header },
		{ "download_and_import_model", &UKainFunctionLibrary::execdownload_and_import_model },
		{ "format_model_info", &UKainFunctionLibrary::execformat_model_info },
		{ "get_api_base_url", &UKainFunctionLibrary::execget_api_base_url },
		{ "get_download_url", &UKainFunctionLibrary::execget_download_url },
		{ "get_model_viewer_url", &UKainFunctionLibrary::execget_model_viewer_url },
		{ "get_sort_mode_string", &UKainFunctionLibrary::execget_sort_mode_string },
		{ "is_cache_valid", &UKainFunctionLibrary::execis_cache_valid },
		{ "is_valid_model_uid", &UKainFunctionLibrary::execis_valid_model_uid },
		{ "parse_thumbnail_url", &UKainFunctionLibrary::execparse_thumbnail_url },
		{ "search_sketchfab", &UKainFunctionLibrary::execsearch_sketchfab },
		{ "validate_api_token", &UKainFunctionLibrary::execvalidate_api_token },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UKainFunctionLibrary);
UClass* Z_Construct_UClass_UKainFunctionLibrary_NoRegister()
{
	return UKainFunctionLibrary::StaticClass();
}
struct Z_Construct_UClass_UKainFunctionLibrary_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "SketchfabImporterTypes.h" },
		{ "ModuleRelativePath", "Public/SketchfabImporterTypes.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UKainFunctionLibrary_build_search_url, "build_search_url" }, // 2262897350
		{ &Z_Construct_UFunction_UKainFunctionLibrary_clear_cache, "clear_cache" }, // 61165663
		{ &Z_Construct_UFunction_UKainFunctionLibrary_create_auth_header, "create_auth_header" }, // 2663635219
		{ &Z_Construct_UFunction_UKainFunctionLibrary_download_and_import_model, "download_and_import_model" }, // 4181239309
		{ &Z_Construct_UFunction_UKainFunctionLibrary_format_model_info, "format_model_info" }, // 3009077751
		{ &Z_Construct_UFunction_UKainFunctionLibrary_get_api_base_url, "get_api_base_url" }, // 3460813437
		{ &Z_Construct_UFunction_UKainFunctionLibrary_get_download_url, "get_download_url" }, // 3675694850
		{ &Z_Construct_UFunction_UKainFunctionLibrary_get_model_viewer_url, "get_model_viewer_url" }, // 3463995326
		{ &Z_Construct_UFunction_UKainFunctionLibrary_get_sort_mode_string, "get_sort_mode_string" }, // 1111689182
		{ &Z_Construct_UFunction_UKainFunctionLibrary_is_cache_valid, "is_cache_valid" }, // 4263260788
		{ &Z_Construct_UFunction_UKainFunctionLibrary_is_valid_model_uid, "is_valid_model_uid" }, // 1139281178
		{ &Z_Construct_UFunction_UKainFunctionLibrary_parse_thumbnail_url, "parse_thumbnail_url" }, // 2450897866
		{ &Z_Construct_UFunction_UKainFunctionLibrary_search_sketchfab, "search_sketchfab" }, // 4050358116
		{ &Z_Construct_UFunction_UKainFunctionLibrary_validate_api_token, "validate_api_token" }, // 2883681247
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UKainFunctionLibrary>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_UKainFunctionLibrary_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UBlueprintFunctionLibrary,
	(UObject* (*)())Z_Construct_UPackage__Script_SketchfabImporter,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UKainFunctionLibrary_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UKainFunctionLibrary_Statics::ClassParams = {
	&UKainFunctionLibrary::StaticClass,
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
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UKainFunctionLibrary_Statics::Class_MetaDataParams), Z_Construct_UClass_UKainFunctionLibrary_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UKainFunctionLibrary()
{
	if (!Z_Registration_Info_UClass_UKainFunctionLibrary.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UKainFunctionLibrary.OuterSingleton, Z_Construct_UClass_UKainFunctionLibrary_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UKainFunctionLibrary.OuterSingleton;
}
template<> SKETCHFABIMPORTER_API UClass* StaticClass<UKainFunctionLibrary>()
{
	return UKainFunctionLibrary::StaticClass();
}
UKainFunctionLibrary::UKainFunctionLibrary(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(UKainFunctionLibrary);
UKainFunctionLibrary::~UKainFunctionLibrary() {}
// End Class UKainFunctionLibrary

// Begin Registration
struct Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ ESketchfabSortMode_StaticEnum, TEXT("ESketchfabSortMode"), &Z_Registration_Info_UEnum_ESketchfabSortMode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1524689981U) },
	};
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FSketchfabModel::StaticStruct, Z_Construct_UScriptStruct_FSketchfabModel_Statics::NewStructOps, TEXT("SketchfabModel"), &Z_Registration_Info_UScriptStruct_SketchfabModel, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FSketchfabModel), 2784328974U) },
		{ FSketchfabSearchOptions::StaticStruct, Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics::NewStructOps, TEXT("SketchfabSearchOptions"), &Z_Registration_Info_UScriptStruct_SketchfabSearchOptions, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FSketchfabSearchOptions), 1985417764U) },
		{ FSketchfabDownloadResult::StaticStruct, Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics::NewStructOps, TEXT("SketchfabDownloadResult"), &Z_Registration_Info_UScriptStruct_SketchfabDownloadResult, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FSketchfabDownloadResult), 1567494457U) },
		{ FSketchfabCache::StaticStruct, Z_Construct_UScriptStruct_FSketchfabCache_Statics::NewStructOps, TEXT("SketchfabCache"), &Z_Registration_Info_UScriptStruct_SketchfabCache, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FSketchfabCache), 1972516345U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_USketchfabImporterComponent, USketchfabImporterComponent::StaticClass, TEXT("USketchfabImporterComponent"), &Z_Registration_Info_UClass_USketchfabImporterComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(USketchfabImporterComponent), 1494984908U) },
		{ Z_Construct_UClass_ASketchfabImportManager, ASketchfabImportManager::StaticClass, TEXT("ASketchfabImportManager"), &Z_Registration_Info_UClass_ASketchfabImportManager, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ASketchfabImportManager), 856357977U) },
		{ Z_Construct_UClass_UKainFunctionLibrary, UKainFunctionLibrary::StaticClass, TEXT("UKainFunctionLibrary"), &Z_Registration_Info_UClass_UKainFunctionLibrary, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UKainFunctionLibrary), 1926714800U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_561399961(TEXT("/Script/SketchfabImporter"),
	Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_Statics::ClassInfo),
	Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_Statics::ScriptStructInfo),
	Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
