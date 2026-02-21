// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "AssetImporter/SMAssetImporter.h"

#include "SMAssetImporterJson.generated.h"

class FJsonValue;
class FJsonValueObject;
class FJsonObject;
class USMGraphNode_Base;

UCLASS(NotPlaceable, NotBlueprintable, Transient)
class SMASSETTOOLS_API USMAssetImporterJson : public USMAssetImporter
{
	GENERATED_BODY()

public:

	// USMAssetImporter
	virtual EImportStatus OnReadImportFile(const FString& InFilePath, const FImportArgs& InImportArgs) override;
	virtual EImportStatus OnReadImportData(void* InData, const FImportArgs& InImportArgs) override;
	virtual void OnGetBlueprintCreationArgs(const FImportArgs& InImportArgs, ISMAssetManager::FCreateStateMachineBlueprintArgs& InOutCreationArgs) override;
	virtual EImportStatus OnImportCDO(UObject* InCDO) override;
	virtual EImportStatus OnImportRootGraph(USMGraph* InGraph) override;
	virtual void OnFinishImport(USMBlueprint* InBlueprint, EImportStatus InStatus) override;
	// ~USMAssetImporter

protected:

	struct FJsonGraphNode
	{
		// Owning state machine guid.
		FGuid OwnerGuid;
		// This node guid.
		FGuid NodeGuid;

		FGuid FromGuid;
		FGuid ToGuid;

		// State name only.
		FString NodeName;
		// Position on graph,
		FVector2D NodePosition;
		// Connected to entry on graph.
		bool bIsEntryNode = false;

		// Only set if true for conduits and transitions.
		TOptional<bool> bDefaultEval;

		// Node object as json.
		TSharedPtr<FJsonObject> JsonObject;
		// The graph node class to create the graph node with.
		TSubclassOf<USMGraphNode_Base> GraphNodeClass;
		// The real graph node, not created initially.
		USMGraphNode_Base* GraphNode;

		// Parent node to this node.
		TWeakPtr<FJsonGraphNode> ParentNode;
		// Any children if this is an SM.
		TArray<TWeakPtr<FJsonGraphNode>> ChildrenNodes;
	};

	void CreateStateNodeRecursive(TSharedPtr<FJsonGraphNode> InJsonGraphNode, USMGraph* InGraph, USMBlueprint* InBlueprint);
	void CreateTransitionNode(TSharedPtr<FJsonGraphNode> InJsonGraphNode, USMBlueprint* InBlueprint);

	static bool JsonObjectToUObject(const TSharedPtr<FJsonObject>& InJsonObject, UObject* InOutObject);
	static TSharedPtr<FJsonGraphNode> JsonObjectToJsonGraphNode(const TSharedPtr<FJsonObject>& InJsonObject);

protected:
	TSharedPtr<FJsonObject> RootJsonObject;
	TSharedPtr<FJsonGraphNode> RootJsonGraphNode;
	/** Owning state machine guids mapped to an array of contained node guids. */
	TMap<FGuid, TArray<TSharedPtr<FJsonGraphNode>>> OwningGuidToGraphNodes;
	/** Each node guid mapped to an individual nodes. */
	TMap<FGuid, TSharedPtr<FJsonGraphNode>> NodeGuidToNode;
	/** Owning state machine guids mapped to their entry node. */
	TMap<FGuid, TSharedPtr<FJsonGraphNode>> OwningGuidToEntryNode;
};