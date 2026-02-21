// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "AssetImporter/Types/SMAssetImporterJson.h"

#include "ISMAssetToolsModule.h"
#include "ISMGraphGeneration.h"
#include "SMAssetToolsLog.h"
#include "Utilities/SMImportExportUtils.h"
#include "Utilities/SMJsonUtils.h"

#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "BlueprintNodeSpawner.h"
#include "Dom/JsonObject.h"
#include "EdGraphNode_Comment.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"

USMAssetImporter::EImportStatus USMAssetImporterJson::OnReadImportFile(const FString& InFilePath,
                                                                       const FImportArgs& InImportArgs)
{
	if (IFileManager::Get().FileExists(*InFilePath))
	{
		const TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*InFilePath));
		if (Ar)
		{
			const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Ar.Get());

			if (FJsonSerializer::Deserialize(JsonReader, RootJsonObject) && RootJsonObject.IsValid())
			{
				if (!InImportArgs.bCheckVersion)
				{
					return EImportStatus::Success;
				}

				int32 Version = 0;
				if (RootJsonObject->TryGetNumberField(LD::JsonFields::FIELD_JSON_VERSION, Version) &&
					Version <= LD::JsonUtils::CurrentVersion)
				{
					return EImportStatus::Success;
				}

				LDASSETTOOLS_LOG_ERROR(TEXT("Could not import %s because the version field %s was missing or invalid."),
					*InFilePath, *LD::JsonFields::FIELD_JSON_VERSION);
				return EImportStatus::Failure;
			}
		}
	}
	return EImportStatus::Failure;
}

USMAssetImporter::EImportStatus USMAssetImporterJson::OnReadImportData(void* InData,
	const FImportArgs& InImportArgs)
{
	if (InData == nullptr)
	{
		return EImportStatus::Failure;
	}
	RootJsonObject = MakeShareable(new FJsonObject(*static_cast<FJsonObject*>(InData)));
	return EImportStatus::Success;
}

void USMAssetImporterJson::OnGetBlueprintCreationArgs(const FImportArgs& InImportArgs,
                                                      ISMAssetManager::FCreateStateMachineBlueprintArgs& InOutCreationArgs)
{
	check(RootJsonObject.IsValid());

	// Try to use the exported asset name.
	FString Name;
	if (RootJsonObject->TryGetStringField(LD::JsonFields::FIELD_NAME, Name))
	{
		InOutCreationArgs.Name = *Name;
	}

	// Find the correct parent class to use for this asset.
	const TSubclassOf<USMInstance> ParentClass = LD::JsonUtils::GetClassFromStringField(RootJsonObject, LD::JsonFields::FIELD_PARENT_CLASS);
	InOutCreationArgs.ParentClass = ParentClass;
}

USMAssetImporter::EImportStatus USMAssetImporterJson::OnImportCDO(UObject* InCDO)
{
	const TSharedPtr<FJsonObject>& JsonObject = RootJsonObject->GetObjectField(LD::JsonFields::FIELD_CDO);
	const bool bResult = JsonObjectToUObject(JsonObject, InCDO);
	return bResult ? EImportStatus::Success : EImportStatus::Failure;
}

USMAssetImporter::EImportStatus USMAssetImporterJson::OnImportRootGraph(USMGraph* InGraph)
{
	check(InGraph);
	check(RootJsonObject.IsValid());

	const FString RootGuidString = RootJsonObject->GetField<EJson::String>(LD::JsonFields::FIELD_ROOT_GUID)->AsString();

	RootJsonGraphNode = MakeShared<FJsonGraphNode>();
	RootJsonGraphNode->NodeGuid = FGuid(RootGuidString);

	// Record entry nodes
	{
		const TSharedPtr<FJsonValue> JsonEntryNodes = RootJsonObject->GetField<EJson::Array>(LD::JsonFields::FIELD_ENTRY_NODES);
		if (!JsonEntryNodes.IsValid())
		{
			return EImportStatus::Failure;
		}

		for (const TSharedPtr<FJsonValue>& JsonEntryNode : JsonEntryNodes->AsArray())
		{
			TSharedPtr<FJsonObject> JsonEntryNodeObject = JsonEntryNode->AsObject();
			const TSharedPtr<FJsonGraphNode> JsonGraphNode = JsonObjectToJsonGraphNode(JsonEntryNodeObject);
			if (!JsonGraphNode.IsValid())
			{
				return EImportStatus::Failure;
			}

			OwningGuidToEntryNode.Add(JsonGraphNode->OwnerGuid, JsonGraphNode);
		}
	}

	// States
	{
		const TSharedPtr<FJsonValue> JsonStates = RootJsonObject->GetField<EJson::Object>(LD::JsonFields::FIELD_STATES);
		if (!JsonStates.IsValid())
		{
			return EImportStatus::Failure;
		}

		for (const TTuple<FString, TSharedPtr<FJsonValue>>& JsonStateKeyVal : JsonStates->AsObject()->Values)
		{
			TSharedPtr<FJsonObject> JsonStateObject = JsonStateKeyVal.Value->AsObject();
			const TSharedPtr<FJsonGraphNode> JsonGraphNode = JsonObjectToJsonGraphNode(JsonStateObject);
			if (!JsonGraphNode.IsValid())
			{
				return EImportStatus::Failure;
			}

			TArray<TSharedPtr<FJsonGraphNode>>& GraphNodes = OwningGuidToGraphNodes.FindOrAdd(JsonGraphNode->OwnerGuid);
			GraphNodes.Add(JsonGraphNode);
		}

		// Build a proper tree starting from the root state machine.
		for (const TTuple<FGuid, TArray<TSharedPtr<FJsonGraphNode>>>& JsonGraphNodeKeyVal : OwningGuidToGraphNodes)
		{
			for (const TSharedPtr<FJsonGraphNode>& JsonGraphNode : JsonGraphNodeKeyVal.Value)
			{
				NodeGuidToNode.Add(JsonGraphNode->NodeGuid, JsonGraphNode);

				// Check if the nodes belonging to this state machine actually contain children nodes as well.
				if (const TArray<TSharedPtr<FJsonGraphNode>>* ChildrenNodes = OwningGuidToGraphNodes.Find(JsonGraphNode->NodeGuid))
				{
					for (const TSharedPtr<FJsonGraphNode>& ChildNode : *ChildrenNodes)
					{
						ChildNode->ParentNode = JsonGraphNode;
					}

					JsonGraphNode->ChildrenNodes.Append(*ChildrenNodes);
				}

				// Look for root children.
				if (JsonGraphNode->OwnerGuid == RootJsonGraphNode->NodeGuid)
				{
					RootJsonGraphNode->ChildrenNodes.Add(JsonGraphNode);
				}
			}
		}
	}

	USMBlueprint* Blueprint = CastChecked<USMBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraphChecked(InGraph));

	// Create the real graph nodes.
	CreateStateNodeRecursive(RootJsonGraphNode, InGraph, Blueprint);

	// Discover and wire transitions.
	{
		const TSharedPtr<FJsonValue> JsonTransitions = RootJsonObject->GetField<EJson::Object>(LD::JsonFields::FIELD_TRANSITIONS);
		if (!JsonTransitions.IsValid())
		{
			return EImportStatus::Failure;
		}

		for (const TTuple<FString, TSharedPtr<FJsonValue>>& JsonTransitionKeyVal : JsonTransitions->AsObject()->Values)
		{
			TSharedPtr<FJsonObject> JsonTransitionObject = JsonTransitionKeyVal.Value->AsObject();
			const TSharedPtr<FJsonGraphNode> JsonGraphNode = JsonObjectToJsonGraphNode(JsonTransitionObject);
			if (!JsonGraphNode.IsValid())
			{
				return EImportStatus::Failure;
			}

			TArray<TSharedPtr<FJsonGraphNode>>& GraphNodes = OwningGuidToGraphNodes.FindOrAdd(JsonGraphNode->OwnerGuid);
			GraphNodes.Add(JsonGraphNode);

			CreateTransitionNode(JsonGraphNode, Blueprint);
		}
	}

	// Import comments.
	{
		const TSharedPtr<FJsonValue> JsonComments = RootJsonObject->GetField<EJson::Array>(LD::JsonFields::FIELD_COMMENTS);
		if (!JsonComments.IsValid())
		{
			return EImportStatus::Failure;
		}

		for (const TSharedPtr<FJsonValue>& JsonCommentNode : JsonComments->AsArray())
		{
			const TSharedPtr<FJsonObject>& JsonCommentNodeObject = JsonCommentNode->AsObject();

			FString OwnerGuidString;
			FString GraphName;
			if (ensure(JsonCommentNodeObject->TryGetStringField(LD::JsonFields::FIELD_OWNER_GUID, OwnerGuidString)
				&& ensure(JsonCommentNodeObject->TryGetStringField(LD::JsonFields::FIELD_GRAPH_NAME, GraphName))))
			{
				FGuid OwnerGuid(OwnerGuidString);

				UEdGraph* OwningGraph = FSMBlueprintEditorUtils::FindGraphFromRuntimeNodeGuid(InGraph, OwnerGuid, GraphName);
				if (ensure(OwningGraph))
				{
					FVector2D Position;
					JsonCommentNodeObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UEdGraphNode, NodePosX), Position.X);
					JsonCommentNodeObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UEdGraphNode, NodePosY), Position.Y);
				
					const UBlueprintNodeSpawner* CommentSpawner = UBlueprintNodeSpawner::Create(UEdGraphNode_Comment::StaticClass(), OwningGraph);
					UEdGraphNode_Comment* CommentNode = CastChecked<UEdGraphNode_Comment>(CommentSpawner->Invoke(OwningGraph, IBlueprintNodeBinder::FBindingSet(), Position));

					JsonObjectToUObject(JsonCommentNodeObject, CommentNode);
				
					JsonCommentNodeObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UEdGraphNode, NodeWidth), CommentNode->NodeWidth);
					JsonCommentNodeObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UEdGraphNode, NodeHeight), CommentNode->NodeHeight);

					continue;
				}
			}
			
			return EImportStatus::Failure;
		}
	}

	return EImportStatus::Success;
}

void USMAssetImporterJson::OnFinishImport(USMBlueprint* InBlueprint, EImportStatus InStatus)
{
	RootJsonObject.Reset();
	RootJsonGraphNode.Reset();
	OwningGuidToGraphNodes.Empty();
	NodeGuidToNode.Empty();
	OwningGuidToEntryNode.Empty();

	if (InStatus != EImportStatus::Failure)
	{
		FBlueprintEditorUtils::RefreshAllNodes(InBlueprint);
	}
}

void USMAssetImporterJson::CreateStateNodeRecursive(TSharedPtr<FJsonGraphNode> InJsonGraphNode, USMGraph* InGraph, USMBlueprint* InBlueprint)
{
	check(InJsonGraphNode.IsValid());
	check(InGraph);
	check(InBlueprint);

	USMGraph* GraphToUse = InGraph;

	const bool bIsRootNode = InJsonGraphNode == RootJsonGraphNode;
	// The root node doesn't need a node created for it.
	if (!bIsRootNode)
	{
		const ISMAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<ISMAssetToolsModule>(
		LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

		ISMGraphGeneration::FCreateStateNodeArgs CreateStateNodeArgs;
		CreateStateNodeArgs.GraphNodeClass = InJsonGraphNode->GraphNodeClass;
		CreateStateNodeArgs.NodeGuid = InJsonGraphNode->NodeGuid;
		CreateStateNodeArgs.NodePosition = InJsonGraphNode->NodePosition;
		CreateStateNodeArgs.GraphOwner = InGraph;
		CreateStateNodeArgs.StateName = InJsonGraphNode->NodeName;
		CreateStateNodeArgs.bIsEntryState = InJsonGraphNode->bIsEntryNode;
		InJsonGraphNode->GraphNode = AssetToolsModule.GetGraphGenerationInterface()->
											  CreateStateNode(InBlueprint, MoveTemp(CreateStateNodeArgs));

		if (ensure(InJsonGraphNode->GraphNode))
		{
			// Load saved properties.
			ensure(JsonObjectToUObject(InJsonGraphNode->JsonObject, InJsonGraphNode->GraphNode));

			// Clean out any outdated graphs, such as arrays that were added in from class defaults, but changed
			// from the instance data.
			InJsonGraphNode->GraphNode->CreateGraphPropertyGraphs();
		}

		GraphToUse = Cast<USMGraph>(InJsonGraphNode->GraphNode->GetBoundGraph());
	}

	if (GraphToUse)
	{
		// Identify the entry node and set any values it might have.
		if (FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(GraphToUse))
		{
			if (const TSharedPtr<FJsonGraphNode>* EntryNodeJson = OwningGuidToEntryNode.Find(RuntimeNode->GetNodeGuid()))
			{
				ensure(JsonObjectToUObject((*EntryNodeJson)->JsonObject, GraphToUse->GetEntryNode()));
				// Clear so this isn't counted again.
				OwningGuidToEntryNode.Remove(RuntimeNode->GetNodeGuid());
			}
			else if (bIsRootNode)
			{
				RuntimeNode->SetNodeGuid(RootJsonGraphNode->NodeGuid);
			}
		}
	}

	// Create children states.
	for (TWeakPtr<FJsonGraphNode>& ChildJsonNode : InJsonGraphNode->ChildrenNodes)
	{
		CreateStateNodeRecursive(ChildJsonNode.Pin(), GraphToUse, InBlueprint);
	}
}

void USMAssetImporterJson::CreateTransitionNode(TSharedPtr<FJsonGraphNode> InJsonGraphNode, USMBlueprint* InBlueprint)
{
	check(InJsonGraphNode.IsValid());
	check(InBlueprint);

	const ISMAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<ISMAssetToolsModule>(
	LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);

	ISMGraphGeneration::FCreateTransitionEdgeArgs CreateTransitionEdgeArgs;
	CreateTransitionEdgeArgs.NodeGuid = InJsonGraphNode->NodeGuid;

	if (const TSharedPtr<FJsonGraphNode>* FromStateNode = NodeGuidToNode.Find(InJsonGraphNode->FromGuid))
	{
		CreateTransitionEdgeArgs.FromStateNode = Cast<USMGraphNode_StateNodeBase>((*FromStateNode)->GraphNode);
	}
	if (!ensure(CreateTransitionEdgeArgs.FromStateNode))
	{
		return;
	}

	if (const TSharedPtr<FJsonGraphNode>* ToStateNode = NodeGuidToNode.Find(InJsonGraphNode->ToGuid))
	{
		CreateTransitionEdgeArgs.ToStateNode = Cast<USMGraphNode_StateNodeBase>((*ToStateNode)->GraphNode);
	}
	if (!ensure(CreateTransitionEdgeArgs.ToStateNode))
	{
		return;
	}

	if (InJsonGraphNode->bDefaultEval.IsSet())
	{
		CreateTransitionEdgeArgs.bDefaultToTrue = *InJsonGraphNode->bDefaultEval;
	}

	InJsonGraphNode->GraphNode = AssetToolsModule.GetGraphGenerationInterface()->CreateTransitionEdge(InBlueprint, MoveTemp(CreateTransitionEdgeArgs));
	if (ensure(InJsonGraphNode->GraphNode))
	{
		// Load serialized properties.
		ensure(JsonObjectToUObject(InJsonGraphNode->JsonObject, InJsonGraphNode->GraphNode));
	}
}

bool USMAssetImporterJson::JsonObjectToUObject(const TSharedPtr<FJsonObject>& InJsonObject, UObject* InOutObject)
{
	if (InJsonObject.IsValid() && InOutObject)
	{
		InOutObject->Modify();

		for (TFieldIterator<FProperty> PropertyItr(InOutObject->GetClass()); PropertyItr; ++PropertyItr)
		{
			FProperty* Property = *PropertyItr;
			if (LD::ImportExportUtils::ShouldPropertyBeImportedOrExported(Property, InOutObject))
			{
				TSharedPtr<FJsonValue> JsonField = InJsonObject->TryGetField(Property->GetName());

				if (!JsonField.IsValid())
				{
					LDASSETTOOLS_LOG_WARNING(TEXT("Could not locate property %s for import."), *Property->GetName());
					continue;
				}

				// Make sure the property is empty first. UE can struggle if there's a container with existing data
				// being overwritten.
				void* Data = Property->ContainerPtrToValuePtr<void*>(InOutObject);
				Property->ClearValue(Data);

				if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(InOutObject))
				{
					// No special handling, let it deserialize as normal.
				}
				else if (USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(InOutObject))
				{
					if (USMGraphNode_StateMachineStateNode* StateMachineStateNode = Cast<USMGraphNode_StateMachineStateNode>(GraphNode))
					{
						// Reference templates need to be initialized first or nasty crashes around text graph structs can show up later
						// when the owning BP is being compiled.
						if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, ReferencedInstanceTemplate))
						{
							if (USMBlueprint* StateMachineBlueprint = Cast<USMBlueprint>(
								LD::JsonUtils::GetObjectFromStringField(InJsonObject,
									USMGraphNode_StateMachineStateNode::GetStateMachineReferencePropertyName().ToString())))
							{
								StateMachineStateNode->ReferenceStateMachine(StateMachineBlueprint);

								const TSharedPtr<FJsonObject>* TemplateJsonObject = nullptr;
								if (InJsonObject->TryGetObjectField(GET_MEMBER_NAME_STRING_CHECKED(USMGraphNode_StateMachineStateNode, ReferencedInstanceTemplate),
									TemplateJsonObject))
								{
									JsonObjectToUObject(*TemplateJsonObject, StateMachineStateNode->ReferencedInstanceTemplate);
								}

								continue;
							}
						}
					}

					// Set any node instance data manually. Don't let json JsonValueToUProperty treat this as a sub-property.
					// Otherwise property flags are lost and there are many issues with the instance data.
					if (Property->GetFName() == GraphNode->GetNodeTemplatePropertyName() && GraphNode->CanExistAtRuntime())
					{
						// Base node template.
						FName NodeClassName = GraphNode->GetNodeClassPropertyName();
						check(!NodeClassName.IsNone());
						UClass* NodeClass = LD::JsonUtils::GetClassFromStringField(InJsonObject, NodeClassName.ToString());

						GraphNode->SetNodeClass(NodeClass);

						const TSharedPtr<FJsonObject>* TemplateJsonObject = nullptr;
						if (InJsonObject->TryGetObjectField(GraphNode->GetNodeTemplatePropertyName().ToString(), TemplateJsonObject))
						{
							JsonObjectToUObject(*TemplateJsonObject, GraphNode->GetNodeTemplate());
						}

						continue;
					}

					if (Property->GetFName() == GraphNode->GetNodeStackPropertyName())
					{
						// State stack template.
						FName StackName = GraphNode->GetNodeClassPropertyName();
						check(!StackName.IsNone());

						const TArray<TSharedPtr<FJsonValue>>& StackJsonArray = InJsonObject->GetArrayField(GraphNode->GetNodeStackPropertyName().ToString());
						for (const TSharedPtr<FJsonValue>& StackJsonValue : StackJsonArray)
						{
							const TSharedPtr<FJsonObject>& StackJsonObject = StackJsonValue->AsObject();
							if (StackJsonObject.IsValid())
							{
								UClass* NodeClass = LD::JsonUtils::GetClassFromStringField(StackJsonObject,
									GraphNode->GetNodeStackElementClassPropertyName().ToString());

								if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(GraphNode))
								{
									const ISMAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<ISMAssetToolsModule>(
									LOGICDRIVER_ASSET_TOOLS_MODULE_NAME);
									ISMGraphGeneration::FCreateStateStackArgs StateStackArgs;
									StateStackArgs.StateStackInstanceClass = NodeClass;

									USMStateInstance* StackInstance = AssetToolsModule.GetGraphGenerationInterface()->CreateStateStackInstance(StateNode, MoveTemp(StateStackArgs));
									if (ensure(StackInstance))
									{
										const TSharedPtr<FJsonObject>* TemplateJsonObject = nullptr;
										if (StackJsonObject->TryGetObjectField(GET_MEMBER_NAME_STRING_CHECKED(FNodeStackContainer, NodeStackInstanceTemplate), TemplateJsonObject))
										{
											JsonObjectToUObject(*TemplateJsonObject, StackInstance);
										}
									}
								}
							}
						}

						continue;
					}
				}

				// Normal property conversion.
				{
					const uint64 CheckFlags = CPF_BlueprintVisible | CPF_Edit | CPF_ContainsInstancedReference;
					const uint64 SkipFlags = CPF_Transient;
					void* Value = Property->ContainerPtrToValuePtr<void>(InOutObject);
					if (!FJsonObjectConverter::JsonValueToUProperty(JsonField, Property, Value, CheckFlags, SkipFlags))
					{
						LDASSETTOOLS_LOG_ERROR(TEXT("Could not set property %s value for import."), *Property->GetName());
						continue;
					}
				}
			}
		}

		return true;
	}

	return false;
}

TSharedPtr<USMAssetImporterJson::FJsonGraphNode> USMAssetImporterJson::JsonObjectToJsonGraphNode(const TSharedPtr<FJsonObject>& InJsonObject)
{
	TSharedPtr<FJsonGraphNode> Result = MakeShared<FJsonGraphNode>();
	check(InJsonObject.IsValid());

	Result->JsonObject = InJsonObject;

	Result->GraphNodeClass = LD::JsonUtils::GetClassFromStringField(InJsonObject, LD::JsonFields::FIELD_GRAPH_NODE_CLASS);

	FString NodeGuidString;
	if (InJsonObject->TryGetStringField(LD::JsonFields::FIELD_NODE_GUID, NodeGuidString))
	{
		Result->NodeGuid = FGuid(NodeGuidString);
	}

	FString OwnerGuidString;
	if (ensure(InJsonObject->TryGetStringField(LD::JsonFields::FIELD_OWNER_GUID, OwnerGuidString)))
	{
		Result->OwnerGuid = FGuid(OwnerGuidString);
	}

	// Optional transition ones
	FString FromGuidString;
	if (InJsonObject->TryGetStringField(LD::JsonFields::FIELD_FROM_GUID, FromGuidString))
	{
		Result->FromGuid = FGuid(FromGuidString);
	}

	FString ToGuidString;
	if (InJsonObject->TryGetStringField(LD::JsonFields::FIELD_TO_GUID, ToGuidString))
	{
		Result->ToGuid = FGuid(ToGuidString);
	}

	FVector2D NodePosition;
	const TSharedPtr<FJsonObject> NodePositionObject = InJsonObject->GetObjectField(TEXT("NodePosition"));
	if (NodePositionObject.IsValid())
	{
		FJsonObjectConverter::JsonObjectToUStruct(NodePositionObject.ToSharedRef(), TBaseStructure<FVector2D>::Get(), &NodePosition);
		Result->NodePosition = MoveTemp(NodePosition);
	}

	Result->NodeName = InJsonObject->GetStringField(LD::JsonFields::FIELD_NAME);

	Result->bIsEntryNode = false; // Use TryGet, this may not be set and will raise an error when null.
	InJsonObject->TryGetBoolField(LD::JsonFields::FIELD_CONNECTED_TO_ENTRY, Result->bIsEntryNode);

	bool bEval;
	if (InJsonObject->TryGetBoolField(LD::JsonFields::FIELD_EVAL_DEFAULT, bEval))
	{
		Result->bDefaultEval = bEval;
	}

	return MoveTemp(Result);
}
