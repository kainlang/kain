// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "SketchfabImporterTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
enum class ESketchfabSortMode : uint8;
struct FSketchfabCache;
struct FSketchfabDownloadResult;
struct FSketchfabModel;
#ifdef SKETCHFABIMPORTER_SketchfabImporterTypes_generated_h
#error "SketchfabImporterTypes.generated.h already included, missing '#pragma once' in SketchfabImporterTypes.h"
#endif
#define SKETCHFABIMPORTER_SketchfabImporterTypes_generated_h

#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_25_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FSketchfabModel_Statics; \
	static class UScriptStruct* StaticStruct(); \
	typedef FTableRowBase Super;


template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<struct FSketchfabModel>();

#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_58_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FSketchfabSearchOptions_Statics; \
	static class UScriptStruct* StaticStruct();


template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<struct FSketchfabSearchOptions>();

#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_79_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FSketchfabDownloadResult_Statics; \
	static class UScriptStruct* StaticStruct();


template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<struct FSketchfabDownloadResult>();

#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_97_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FSketchfabCache_Statics; \
	static class UScriptStruct* StaticStruct();


template<> SKETCHFABIMPORTER_API UScriptStruct* StaticStruct<struct FSketchfabCache>();

#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_115_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUSketchfabImporterComponent(); \
	friend struct Z_Construct_UClass_USketchfabImporterComponent_Statics; \
public: \
	DECLARE_CLASS(USketchfabImporterComponent, UActorComponent, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/SketchfabImporter"), NO_API) \
	DECLARE_SERIALIZER(USketchfabImporterComponent)


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_115_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	USketchfabImporterComponent(USketchfabImporterComponent&&); \
	USketchfabImporterComponent(const USketchfabImporterComponent&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, USketchfabImporterComponent); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(USketchfabImporterComponent); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(USketchfabImporterComponent) \
	NO_API virtual ~USketchfabImporterComponent();


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_112_PROLOG
#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_115_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_115_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_115_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> SKETCHFABIMPORTER_API UClass* StaticClass<class USketchfabImporterComponent>();

#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_RPC_WRAPPERS_NO_PURE_DECLS \
	virtual void Multicast_NotifyDownloadCancelled_Implementation(const FString& model_uid); \
	virtual void Multicast_NotifyDownloadComplete_Implementation(const FString& model_uid, bool success); \
	virtual void Multicast_NotifyDownloadStarted_Implementation(const FString& model_uid); \
	virtual void Client_UpdateProgress_Implementation(const FString& model_uid, const float progress); \
	virtual void Server_CancelDownload_Implementation(const FString& model_uid); \
	virtual void Server_StartDownload_Implementation(const FString& model_uid, const FString& api_token); \
	DECLARE_FUNCTION(execMulticast_NotifyDownloadCancelled); \
	DECLARE_FUNCTION(execMulticast_NotifyDownloadComplete); \
	DECLARE_FUNCTION(execMulticast_NotifyDownloadStarted); \
	DECLARE_FUNCTION(execClient_UpdateProgress); \
	DECLARE_FUNCTION(execServer_CancelDownload); \
	DECLARE_FUNCTION(execServer_StartDownload);


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_CALLBACK_WRAPPERS
#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesASketchfabImportManager(); \
	friend struct Z_Construct_UClass_ASketchfabImportManager_Statics; \
public: \
	DECLARE_CLASS(ASketchfabImportManager, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/SketchfabImporter"), NO_API) \
	DECLARE_SERIALIZER(ASketchfabImportManager)


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	ASketchfabImportManager(ASketchfabImportManager&&); \
	ASketchfabImportManager(const ASketchfabImportManager&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ASketchfabImportManager); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ASketchfabImportManager); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(ASketchfabImportManager) \
	NO_API virtual ~ASketchfabImportManager();


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_133_PROLOG
#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_CALLBACK_WRAPPERS \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_136_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> SKETCHFABIMPORTER_API UClass* StaticClass<class ASketchfabImportManager>();

#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_172_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execclear_cache); \
	DECLARE_FUNCTION(execis_cache_valid); \
	DECLARE_FUNCTION(execget_api_base_url); \
	DECLARE_FUNCTION(execget_model_viewer_url); \
	DECLARE_FUNCTION(execis_valid_model_uid); \
	DECLARE_FUNCTION(execcreate_auth_header); \
	DECLARE_FUNCTION(execformat_model_info); \
	DECLARE_FUNCTION(execparse_thumbnail_url); \
	DECLARE_FUNCTION(execbuild_search_url); \
	DECLARE_FUNCTION(execvalidate_api_token); \
	DECLARE_FUNCTION(execget_sort_mode_string); \
	DECLARE_FUNCTION(execdownload_and_import_model); \
	DECLARE_FUNCTION(execget_download_url); \
	DECLARE_FUNCTION(execsearch_sketchfab);


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_172_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUKainFunctionLibrary(); \
	friend struct Z_Construct_UClass_UKainFunctionLibrary_Statics; \
public: \
	DECLARE_CLASS(UKainFunctionLibrary, UBlueprintFunctionLibrary, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/SketchfabImporter"), NO_API) \
	DECLARE_SERIALIZER(UKainFunctionLibrary)


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_172_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UKainFunctionLibrary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UKainFunctionLibrary(UKainFunctionLibrary&&); \
	UKainFunctionLibrary(const UKainFunctionLibrary&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UKainFunctionLibrary); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UKainFunctionLibrary); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UKainFunctionLibrary) \
	NO_API virtual ~UKainFunctionLibrary();


#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_169_PROLOG
#define FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_172_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_172_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_172_INCLASS_NO_PURE_DECLS \
	FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h_172_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> SKETCHFABIMPORTER_API UClass* StaticClass<class UKainFunctionLibrary>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_KainPluginFactory_Plugins_SketchFab_Source_SketchfabImporter_Public_SketchfabImporterTypes_h


#define FOREACH_ENUM_ESKETCHFABSORTMODE(op) \
	op(ESketchfabSortMode::Relevance) \
	op(ESketchfabSortMode::LikeCount) \
	op(ESketchfabSortMode::ViewCount) \
	op(ESketchfabSortMode::PublishedDate) \
	op(ESketchfabSortMode::CreatedDate) 

enum class ESketchfabSortMode : uint8;
template<> struct TIsUEnumClass<ESketchfabSortMode> { enum { Value = true }; };
template<> SKETCHFABIMPORTER_API UEnum* StaticEnum<ESketchfabSortMode>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
