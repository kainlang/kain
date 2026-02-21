// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "AssetExporter/SMAssetExporter.h"

#include "SMAssetExporterJson.generated.h"

class FJsonValue;
class FJsonValueObject;
class FJsonObject;

UCLASS(NotPlaceable, NotBlueprintable, Transient)
class SMASSETTOOLS_API USMAssetExporterJson : public USMAssetExporter
{
	GENERATED_BODY()

public:

	// USMAssetExporter
	virtual EExportStatus OnBeginExport(const FExportArgs& InExportArgs) override;
	virtual EExportStatus OnExportCDO(const UObject* InCDO) override;
	virtual EExportStatus OnExportNode(const UEdGraphNode* InGraphNode) override;
	virtual void OnFinishExport(USMBlueprint* InBlueprint, EExportStatus InStatus) override;
	// ~USMAssetExporter

	/** Return the exported json object. Only complete during OnFinishExport. */
	TSharedPtr<FJsonObject> GetExportedJsonObject() const { return RootJsonObject; }

	/** Convert a USMGraphNode_Base to a json object. */
	static EExportStatus GraphNodeToJsonValue(const UEdGraphNode* InGraphNode, TSharedPtr<FJsonValueObject>& OutJsonValue);

protected:
	static TSharedPtr<FJsonObject> CreateJsonObject(const UObject* InObject);
	static TSharedPtr<FJsonValueObject> CreateJsonValueObject(const UObject* InObject);

	/** Remove any properties that aren't meant to be serialized. */
	static void CleanupJsonObject(TSharedPtr<FJsonObject> JsonObject);

	/** Called during json object export. */
	static TSharedPtr<FJsonValue> OnExportJsonProperty(FProperty* InProperty, const void* Value, const UObject* OwningObject);

protected:
	TSharedPtr<FJsonObject> RootJsonObject;
	TArray<TSharedPtr<FJsonValue>> StateJsonArray;
	TArray<TSharedPtr<FJsonValue>> TransitionJsonArray;
	TArray<TSharedPtr<FJsonValue>> EntryJsonArray;
	TArray<TSharedPtr<FJsonValue>> CommentJsonArray;
	FExportArgs ExportArgs;
};