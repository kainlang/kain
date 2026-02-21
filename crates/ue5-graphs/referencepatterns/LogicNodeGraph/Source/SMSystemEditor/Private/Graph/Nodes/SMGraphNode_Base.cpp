// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_Base.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Compilers/SMKismetCompiler.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMNodeSettings.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Construction/ISMEditorConstructionManager.h"
#include "Customization/SMEditorCustomization.h"
#include "FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Schema/SMGraphSchema.h"
#include "Graph/SMGraph.h"
#include "Graph/SMPropertyGraph.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMEditorCustomVersion.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "ISMEditorGraphNode_StateBaseInterface.h"
#include "SMUtils.h"

#include "EdGraphUtilities.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectThreadContext.h"

#define LOCTEXT_NAMESPACE "SMGraphNodeBase"

/** Log a message to the message log up to 4 arguments long. */
#define LOG_MESSAGE(LOG_TYPE, MESSAGE, ARGS, ARGS_COUNT) \
	do { \
		if (ARGS_COUNT == 0)\
			MessageLog.LOG_TYPE(MESSAGE); \
		else if (ARGS_COUNT == 1) \
			MessageLog.LOG_TYPE(MESSAGE, ARGS[0]); \
		else if (ARGS_COUNT == 2) \
			MessageLog.LOG_TYPE(MESSAGE, ARGS[0], ARGS[1]); \
		else if (ARGS_COUNT == 3) \
			MessageLog.LOG_TYPE(MESSAGE, ARGS[0], ARGS[1], ARGS[2]); \
		else if (ARGS_COUNT >= 4) \
			MessageLog.LOG_TYPE(MESSAGE, ARGS[0], ARGS[1], ARGS[2], ARGS[3]); \
	} while (0)

class FSMNodeNameValidator : public FStringSetNameValidator
{
public:
	FSMNodeNameValidator(const USMGraphNode_Base* InNode)
		: FStringSetNameValidator(FString())
	{
		check(InNode);
		TArray<USMGraphNode_Base*> Nodes;
		const USMGraph* StateMachine = CastChecked<USMGraph>(InNode->GetOuter());

		const TArray<TSubclassOf<USMGraphNode_Base>> AllowedDuplicateTypes = InNode->GetAllowedDuplicateNodeNameTypes();
		
		StateMachine->GetNodesOfClass<USMGraphNode_Base>(Nodes);
		for (auto NodeIt = Nodes.CreateIterator(); NodeIt; ++NodeIt)
		{
			const USMGraphNode_Base* Node = *NodeIt;
			if (Node != InNode && !AllowedDuplicateTypes.Contains(Node->GetClass()))
			{
				Names.Add(Node->GetNodeName());
			}
		}
	}

	virtual EValidatorResult IsValid(const FString& Name, bool bOriginal) override
	{
		EValidatorResult Result = FStringSetNameValidator::IsValid(Name, bOriginal);

		if (Result == EValidatorResult::Ok)
		{
			if (Name.Len() > FKismetNameValidator::GetMaximumNameLength())
			{
				Result = EValidatorResult::TooLong;
			}
			else if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRestrictInvalidCharacters)
			{
				FText Reason;
				if (!FName(Name).IsValidXName(Reason, LD_INVALID_STATENAME_CHARACTERS))
				{
					Result = EValidatorResult::ContainsInvalidCharacters;
				}
				else if (Name.Compare(TEXT("Sequencer"), ESearchCase::IgnoreCase) == 0)
				{
					// Can cause problems during copy & paste.
					Result = EValidatorResult::LocallyInUse;
				}
			}
		}

		return Result;
	}
};

USMGraphNode_Base::USMGraphNode_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
	DebugTotalTime = 0.f;
	bIsDebugActive = false;
	bWasDebugActive = false;
	bCreatePropertyGraphsOnPropertyChange = true;
	bCreatePropertyGraphsSilently = false;
	bChangeFromRedirect = false;
	MaxTimeToShowDebug = 1.f;
	BoundGraph = nullptr;
	NodeInstanceTemplate = nullptr;
	CachedBrush = MakeShared<FSlateBrush>(FSlateNoResource());
	bJustPasted = false;
	bIsEditUndo = false;
	bGenerateTemplateOnNodePlacement = true;
	bIsPreCompiling = false;
	bIsRunningConstructionScripts = false;
	bPostEditChangeConstructionRequiresFullRefresh = true;
	bNativeGuidConversion = false;
	bRequiresGuidRegeneration = false;
	bNeedsStateStackConversion = false;
	bTEST_ForceNoTemplateGuid = false;

	OnPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &USMGraphNode_Base::HandleOnPropertyChangedEvent);

	if (GIsEditor && !HasAnyFlags(RF_Transient))
	{
		OnCacheClearedHandle = FSMBlueprintEditorUtils::OnCacheClearedEvent.AddUObject(this, &USMGraphNode_Base::OnBlueprintCacheCleared);
	}
}

USMGraphNode_Base::~USMGraphNode_Base()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("USMGraphNode_Base::~USMGraphNode_Base"), STAT_USMGraphNodeDestructor, STATGROUP_LogicDriverEditor);
	
	if (OnPropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandle);
	}

	if (OnCacheClearedHandle.IsValid())
	{
		FSMBlueprintEditorUtils::OnCacheClearedEvent.Remove(OnCacheClearedHandle);
	}
}

void USMGraphNode_Base::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSMGraphNodeCustomVersion::GUID);
	Super::Serialize(Ar);

	if (Ar.IsLoading() && Ar.CustomVer(FSMGraphNodeCustomVersion::GUID) < FSMGraphNodeCustomVersion::NativePropertyGuid)
	{
		bNativeGuidConversion = true;
	}
}

void USMGraphNode_Base::DestroyNode()
{
	Super::DestroyNode();
	DestroyTemplate();
	DestroyAllPropertyGraphs();
}

void USMGraphNode_Base::PostLoad()
{
	Super::PostLoad();

	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Standard)
	{
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			FSMConstructionConfiguration Configuration;
			Configuration.bDoNotDirty = true;
			Configuration.bFromLoad = true;
			ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprint(Blueprint, MoveTemp(Configuration));
		}
	}
}

void USMGraphNode_Base::PostPasteNode()
{
	bJustPasted = true;
	Super::PostPasteNode();

	if (UEdGraph* Graph = GetBoundGraph())
	{
		// Add the new graph as a child of our parent graph
		UEdGraph* ParentGraph = GetGraph();

		if (ParentGraph->SubGraphs.Find(Graph) == INDEX_NONE)
		{
			ParentGraph->SubGraphs.Add(Graph);
		}

		// Restore transactional flag that is lost during copy/paste process
		Graph->SetFlags(RF_Transactional);
	}

	SetReadOnlyNodePosition();
	InitTemplate();
	CreateGraphPropertyGraphs(true);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
	
	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Standard)
	{
		ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprint(Blueprint);
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	bJustPasted = false;
}

void USMGraphNode_Base::PostEditUndo()
{
	bIsEditUndo = true;
	
	Super::PostEditUndo();

	if (NodeInstanceTemplate)
	{
		NodeInstanceTemplate->ClearFlags(RF_Transient);
	}
	
	// No bound graph prevents the property graphs from finding their blueprint. This could happen if a graph deletion was being redone.
	if (BoundGraph == nullptr)
	{
		return;
	}
	RefreshAllProperties(false);

	// If the transaction being undone contains property graph deletion, the graphs can get added back in incorrectly.
	// So far this is the best place to catch this, but ideally these graphs wouldn't be added back in at all.
	// Test cases show only construction script modification of arrays to be responsible for this.
	FSMBlueprintEditorUtils::CleanupInvalidPropertyGraphs(FSMBlueprintEditorUtils::FindBlueprintForNode(this));
	
	bIsEditUndo = false;
}

void USMGraphNode_Base::PostPlacedNewNode()
{
	SetToCurrentVersion();
	Super::PostPlacedNewNode();
}

TSharedPtr<INameValidatorInterface> USMGraphNode_Base::MakeNameValidator() const
{
	return MakeShared<FSMNodeNameValidator>(this);
}

void USMGraphNode_Base::OnRenameNode(const FString& NewName)
{
	FBlueprintEditorUtils::RenameGraph(GetBoundGraph(), NewName);
}

UObject* USMGraphNode_Base::GetJumpTargetForDoubleClick() const
{
	return BoundGraph;
}

bool USMGraphNode_Base::CanJumpToDefinition() const
{
	return GetJumpTargetForDoubleClick() != nullptr;
}

void USMGraphNode_Base::JumpToDefinition() const
{
	if (UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		if (HyperlinkTarget->GetPackage() != GetPackage())
		{
			if (USMNodeBlueprint* BlueprintTarget = Cast<USMNodeBlueprint>(HyperlinkTarget))
			{
				// For node blueprint targets set the debug object.
				FSMBlueprintEditorUtils::GetNodeBlueprintFromClassAndSetDebugObject(GetNodeClass(), this);
			}
		}
		
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(HyperlinkTarget);
	}
}

bool USMGraphNode_Base::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(USMGraphSchema::StaticClass());
}

void USMGraphNode_Base::ReconstructNode()
{
	Super::ReconstructNode();

	for (const TTuple<FGuid, TObjectPtr<UEdGraph>>& KeyVal : GraphPropertyGraphs)
	{
		CastChecked<USMPropertyGraph>(KeyVal.Value)->RefreshProperty(false, false);
	}

	if (GraphPropertyGraphs.Num() && AreTemplatesFullyLoaded())
	{
		UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(Blueprint);
	}

	// Node instance references may need to be updated if the node class changed.
	if (const UEdGraph* Graph = GetBoundGraph())
	{
		TArray<USMGraphK2Node_StateReadNode_GetNodeInstance*> NodesToReconstruct;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested(Graph, NodesToReconstruct);

		for (USMGraphK2Node_StateReadNode_GetNodeInstance* Node : NodesToReconstruct)
		{
			Node->ReconstructNode();
		}
	}
}

void USMGraphNode_Base::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bChangeFromRedirect = PropertyChangedEvent.ChangeType == EPropertyChangeType::Redirected;
	
	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Standard)
	{
		FSMConstructionConfiguration Config;
		Config.bFullRefreshNeeded = bPostEditChangeConstructionRequiresFullRefresh;
		ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprint(this, Config);
	}
	
	/* BoundGraph could be null if undoing/redoing deletion. */
	if (bCreatePropertyGraphsOnPropertyChange && BoundGraph && !bJustPasted)
	{
		CreateGraphPropertyGraphs();
	}

	FSMBlueprintEditorUtils::InvalidateCaches(FSMBlueprintEditorUtils::FindBlueprintForNode(this));

	bChangeFromRedirect = false;
}

void USMGraphNode_Base::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	FSMBlueprintEditorUtils::InvalidateCaches(FSMBlueprintEditorUtils::FindBlueprintForNode(this));
}

void USMGraphNode_Base::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	for (const FSMGraphNodeLog& Log : CollectedLogs)
	{
		switch (Log.LogType)
		{
		case EMessageSeverity::Info:
		{
			LOG_MESSAGE(Note, *Log.ConsoleMessage, Log.ReferenceList, Log.ReferenceList.Num());
			break;
		}
		case EMessageSeverity::Warning:
		{
			LOG_MESSAGE(Warning, *Log.ConsoleMessage, Log.ReferenceList, Log.ReferenceList.Num());
			break;
		}
		case EMessageSeverity::Error:
		{
			LOG_MESSAGE(Error, *Log.ConsoleMessage, Log.ReferenceList, Log.ReferenceList.Num());
			break;
		}
		}
	}
}

TScriptInterface<ISMEditorGraphPropertyNodeInterface> USMGraphNode_Base::GetEditorGraphProperty(FName PropertyName, const USMNodeInstance* NodeInstance, int32 ArrayIndex) const
{
	return GetGraphPropertyNode(PropertyName, NodeInstance, ArrayIndex);
}

TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> USMGraphNode_Base::GetEditorGraphPropertyAsArray(FName PropertyName, const USMNodeInstance* NodeInstance, int32 ArrayIndex) const
{
	TArray<USMGraphK2Node_PropertyNode_Base*> PropertyNodes = GetGraphPropertyNodes(PropertyName, NodeInstance, ArrayIndex);

	TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> Output =
		TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>>(MoveTemp(PropertyNodes));

	return Output;
}

TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> USMGraphNode_Base::GetAllEditorGraphProperties(
	const USMNodeInstance* NodeInstance) const
{
	TArray<USMGraphK2Node_PropertyNode_Base*> PropertyNodes = GetAllPropertyGraphNodesAsArray(NodeInstance);

	TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> Output =
		TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>>(MoveTemp(PropertyNodes));

	return Output;
}

void USMGraphNode_Base::CreateGraphPropertyGraphs(bool bGenerateNewGuids)
{
	bGenerateNewGuids = bGenerateNewGuids || bRequiresGuidRegeneration;
	
	TSet<FGuid> LiveGuids;
	bool bHasChanged = CreateGraphPropertyGraphsForTemplate(NodeInstanceTemplate, bGenerateNewGuids, LiveGuids);

	if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(this))
	{
		// State nodes may have extra property graphs with the state stack.
		for (const FStateStackContainer& StackTemplate : StateNode->GetAllNodeStackTemplates())
		{
			if (CreateGraphPropertyGraphsForTemplate(StackTemplate.NodeStackInstanceTemplate, bGenerateNewGuids, LiveGuids) && !bHasChanged)
			{
				bHasChanged = true;
			}
		}
	}

	ArraySwapData.Empty();
	
	// Remove graphs no longer used.
	TArray<FGuid> CurrentKeys;
	GraphPropertyGraphs.GetKeys(CurrentKeys);

	for (const FGuid& Guid : CurrentKeys)
	{
		if (!LiveGuids.Contains(Guid))
		{
			TObjectPtr<UEdGraph> GraphToRemove = GraphPropertyGraphs[Guid];
			RemovePropertyGraph(Cast<USMPropertyGraph>(GraphToRemove), false);
			GraphPropertyTemplates.Remove(Guid);
			GraphPropertyGraphs.Remove(Guid);
			GraphPropertyNodes.Remove(Guid);
			bHasChanged = true;
		}
	}

	if (bHasChanged && !bJustPasted)
	{
		ReconstructNode();
		if (GraphPropertyGraphs.Num() == 0 && !bChangeFromRedirect && AreTemplatesFullyLoaded())
		{
			// Reconstruct node only refreshes with > 0 property graphs as an optimization.
			// If the user deleted all property graphs we may have to compile again to refresh.
			UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
			FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(Blueprint);
		}
		
		PropertyCategoriesExpanded.Reset();
	}

	bRequiresGuidRegeneration = false;
	bNeedsStateStackConversion = false;
}

bool USMGraphNode_Base::CreateGraphPropertyGraphsForTemplate(USMNodeInstance* Template, bool bGenerateNewGuids, TSet<FGuid>& LiveGuidsInOut, bool bResetNonVariableGuids)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("USMGraphNode_Base::CreateGraphPropertyGraphsForTemplate"), STAT_CreateGraphPropertyGraphsForTemplate, STATGROUP_LogicDriverEditor);
	
	bool bHasChanged = false;
	
	if (Template && SupportsPropertyGraphs())
	{
		if (!bCreatePropertyGraphsSilently)
		{
			Modify();
			if (BoundGraph)
			{
				BoundGraph->Modify();
			}
		}

		UClass* TemplateClass = Template->GetClass();
		for (TFieldIterator<FProperty> It(TemplateClass); It; ++It)
		{
			FProperty* Property = *It;
			bool bIsActualGraphProperty = false;

			FName VarName = Property->GetFName();
			if (VarName == GET_MEMBER_NAME_CHECKED(USMNodeInstance, ExposedPropertyOverrides))
			{
				continue;
			}

			// So custom graph details can be displayed.
			if (FStructProperty* StructProperty = LD::PropertyUtils::GetGraphPropertyFromProperty(Property))
			{
				bIsActualGraphProperty = true;
				FSMStructCustomization::RegisterNewStruct<FSMGraphPropertyCustomization>(StructProperty->Struct->GetFName());
			}

			// Only properties that are instance editable.
			if (!bIsActualGraphProperty && !LD::PropertyUtils::IsPropertyExposedToGraphNode(Property))
			{
				continue;
			}

			FProperty* TargetProperty = Property;
			int32 ArraySize = 1;
			FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
			if (ArrayProperty)
			{
				TargetProperty = ArrayProperty->Inner;

				FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<uint8>(Template));
				ArraySize = Helper.Num();

				// No array initialized yet.
				if (ArraySize == 0)
				{
					continue;
				}
			}

			// Storage for properties initialized only within this scope.
			TArray<TSharedPtr<FSMGraphProperty>> TempGraphProperties;
			TempGraphProperties.Reserve(ArraySize);

			// Look for an override.
			FSMGraphProperty* MatchedGraphProperty = Template->ExposedPropertyOverrides.FindByPredicate([&](const FSMGraphProperty_Base& GraphProperty)
			{
				return GraphProperty.VariableName == VarName;
			});
			
			// Contains temp properties or pointers to stored properties.
			TArray<FSMGraphProperty_Base*> GraphProperties;
			if (bIsActualGraphProperty)
			{
				// This property itself is a graph property.
				USMUtils::BlueprintPropertyToNativeProperty(Property, Template, GraphProperties);

				// Set the variable information. This may still be used for lookup later.
				for (int32 Idx = 0; Idx < GraphProperties.Num(); ++Idx)
				{
					if (MatchedGraphProperty)
					{
						// Read only and hidden overrides are still used on real properties so save them.
						GraphProperties[Idx]->bReadOnly = MatchedGraphProperty->bReadOnly;
						GraphProperties[Idx]->bHidden = MatchedGraphProperty->bHidden;
					}

					if ((Template->WasArrayPropertyModified(Property->GetFName()) && Template->ArrayIndexChanged == Idx) &&
						(Template->ArrayChangeType == EPropertyChangeType::Duplicate || Template->ArrayChangeType == EPropertyChangeType::ValueSet))
					{
						// Guid will have been duplicated too and needs to be unique.
						GraphProperties[Idx]->InvalidateGuid();
					}

					LD::PropertyUtils::SetGraphPropertyFromProperty(*GraphProperties[Idx], TargetProperty, Template, Idx, false);

					if (bResetNonVariableGuids)
					{
						// Template guid will have been set from above.
						GraphProperties[Idx]->SetTemplateGuid(Template->GetTemplateGuid(), true);
					}
				}
			}
			else
			{
				const bool bArrayModified = Template->WasArrayPropertyModified(Property->GetFName());
				for (int32 Idx = 0; Idx < ArraySize; ++Idx)
				{
					// Check if the array has been modified. This requires special handling for adding or removing elements.
					if (bArrayModified && Template->ArrayIndexChanged == Idx)
					{
						if (Template->ArrayChangeType == EPropertyChangeType::ArrayRemove)
						{
							HandlePropertyGraphArrayRemoval(GraphProperties, TempGraphProperties,
								TargetProperty, Idx, ArraySize, MatchedGraphProperty, Template);
						}
						else if (Template->ArrayChangeType == EPropertyChangeType::ArrayAdd || Template->ArrayChangeType == EPropertyChangeType::Duplicate)
						{
							HandlePropertyGraphArrayInsertion(GraphProperties, TempGraphProperties, TargetProperty,
								Idx, ArraySize, MatchedGraphProperty, Template, Template->ArrayChangeType == EPropertyChangeType::Duplicate);
						}

						// Always trigger an update if the array was modified.
						bHasChanged = true;
						break;
					}

					// Default handling with no array modification.
					{
						TSharedPtr<FSMGraphProperty> TempProperty = MakeShared<FSMGraphProperty>();

						if (MatchedGraphProperty)
						{
							// Assign override defaults before assigning a guid.
							*TempProperty = *MatchedGraphProperty;
						}

						LD::PropertyUtils::SetGraphPropertyFromProperty(*TempProperty, TargetProperty, Template, Idx, true, !bNeedsStateStackConversion);
						if (bNeedsStateStackConversion && bGenerateNewGuids && !bTEST_ForceNoTemplateGuid)
						{
							/*
							 * Special conversion for anyone who was on the dev branch when the pre 2.4 import change went live.
							 */
							if (!GraphPropertyGraphs.FindRef(TempProperty->GetGuid()))
							{
								FSMGraphProperty UpToDateTempProperty;
								LD::PropertyUtils::SetGraphPropertyFromProperty(UpToDateTempProperty, TargetProperty, Template, Idx, true, true);
								if (GraphPropertyGraphs.FindRef(UpToDateTempProperty.GetGuid()))
								{
									*TempProperty = UpToDateTempProperty;
								}
							}
						}

						if (bNativeGuidConversion && !TempProperty->MemberReference.GetMemberGuid().IsValid()/*Native property*/)
						{
							// Special conversion for anyone who was on dev 5.0 / ue5-main after member reference search string was changed.
							// Native properties will have the wrong guid.
							const FGuid& CurrentGuid = TempProperty->GetGuid();
							if (!GraphPropertyGraphs.FindRef(CurrentGuid))
							{
								const bool bUseTempNativeGuidForUE5 = true;
								FSMGraphProperty UpToDateTempProperty;
								LD::PropertyUtils::SetGraphPropertyFromProperty(UpToDateTempProperty, TargetProperty, Template, Idx,
									true, !bNeedsStateStackConversion, bUseTempNativeGuidForUE5);

								const FGuid& GuidSetByUE5 = UpToDateTempProperty.GetGuid();
								if (TObjectPtr<UEdGraph> ExistingPropertyGraph = GraphPropertyGraphs.FindRef(GuidSetByUE5))
								{
									GraphPropertyGraphs.Remove(GuidSetByUE5);
									GraphPropertyGraphs.Add(CurrentGuid, ExistingPropertyGraph);
								}
							}
						}

						TempGraphProperties.Add(TempProperty);
						GraphProperties.Add(TempProperty.Get());
					}
				}

				if (FArraySwapData* MatchedSwapData = ArraySwapData.Find(Template))
				{
					// Array modification shouldn't be happening from a swap. Currently that's an unspecified change by UE.
					// We have to handle it manually through context menu items increasing or decreasing the index.
					if (!bArrayModified && MatchedSwapData->ArrayPropertyNameToSwap == Property->GetFName()
					&& MatchedSwapData->ArrayIndexToSwapA != INDEX_NONE && MatchedSwapData->ArrayIndexToSwapB != INDEX_NONE)
					{
						HandlePropertyGraphArraySwap(GraphProperties, MatchedSwapData->ArrayIndexToSwapA, MatchedSwapData->ArrayIndexToSwapB, Template);
					}
				}
			}

			for (int32 Idx = 0; Idx < GraphProperties.Num(); ++Idx)
			{
				FSMGraphProperty_Base* GraphProperty = GraphProperties[Idx];
				check(GraphProperty);
				GraphProperty->RealDisplayName = Property->GetDisplayNameText();
				GraphProperty->ArrayIndex = Idx;

				GraphProperty->SetTemplateGuid(Template->GetTemplateGuid());
				FGuid Guid = GraphProperty->GenerateNewGuidIfNotValid();

				// Make sure reference is up to date.. if this was a copy paste operation it won't be.
				if (TObjectPtr<USMGraphK2Node_PropertyNode_Base> ExistingGraphPropertyNode = GraphPropertyNodes.FindRef(Guid))
				{
					ExistingGraphPropertyNode->SyncWithContainer();
				}

				TObjectPtr<UEdGraph> OldGraph = GraphPropertyGraphs.FindRef(Guid);
				if (bGenerateNewGuids && OldGraph)
				{
					// Remove these so they don't get deleted later, we just want to reassign the guid.
					GraphPropertyGraphs.Remove(Guid);
					GraphPropertyNodes.Remove(Guid);
					GraphPropertyTemplates.Remove(Guid);

					// Create a new guid if requested.
					if (!GraphProperty->ShouldGenerateGuidFromVariable())
					{
						Guid = GraphProperty->GenerateNewGuid();
					}
					else if (bNeedsStateStackConversion)
					{
						if (!bTEST_ForceNoTemplateGuid)
						{
							// This will apply the template guid.
							LD::PropertyUtils::SetGraphPropertyFromProperty(*GraphProperty, TargetProperty, Template, Idx, true);
						}
						Guid = GraphProperty->GetGuid();
					}
				}

				LiveGuidsInOut.Add(Guid);

				// Refresh the runtime node property in case it has changed.
				if (TObjectPtr<USMGraphK2Node_PropertyNode_Base> ExistingGraphPropertyNode = GraphPropertyNodes.FindRef(Guid))
				{
					ExistingGraphPropertyNode->SetPropertyNode(GraphProperty);
				}

				if (GraphPropertyGraphs.Contains(Guid))
				{
					// Make sure the correct template is recorded. If guids aren't being regenerated it's possible
					// the graph property is pointing to a stale template.
					GraphPropertyTemplates.Add(Guid, Template);
					continue;
				}

				TObjectPtr<UEdGraph> PropertyGraph = nullptr;
				if (bGenerateNewGuids && OldGraph)
				{
					PropertyGraph = OldGraph;
					CastChecked<USMPropertyGraph>(PropertyGraph)->RefreshProperty();
				}
				else
				{
					// Load the package for this module. This is needed to find the correct class to load.
					UPackage* Package = GraphProperty->GetEditorModule();
					if (!Package)
					{
						FSMGraphNodeLog NodeLog(EMessageSeverity::Error);
						NodeLog.ConsoleMessage = TEXT("Could not find editor module for node @@.");
						NodeLog.NodeMessage = FString::Printf(TEXT("Could not find editor module for node @@!"));
						NodeLog.ReferenceList.Add(this);
						AddNodeLogMessage(NodeLog);
						continue;
					}
					UClass* GraphClass = GraphProperty->GetGraphClass(Package);
					if (!GraphClass)
					{
						FSMGraphNodeLog NodeLog(EMessageSeverity::Error);
						NodeLog.ConsoleMessage = TEXT("Could not find graph class for node @@.");
						NodeLog.NodeMessage = FString::Printf(TEXT("Could not find graph class for node @@!"));
						NodeLog.ReferenceList.Add(this);
						AddNodeLogMessage(NodeLog);
						continue;
					}
					UClass* SchemaClass = GraphProperty->GetGraphSchemaClass(Package);
					if (!SchemaClass)
					{
						FSMGraphNodeLog NodeLog(EMessageSeverity::Error);
						NodeLog.ConsoleMessage = TEXT("Could not find schema class for node @@.");
						NodeLog.NodeMessage = FString::Printf(TEXT("Could not find schema class for node @@!"));
						NodeLog.ReferenceList.Add(this);
						AddNodeLogMessage(NodeLog);
						continue;
					}

					PropertyGraph = FBlueprintEditorUtils::CreateNewGraph(
						BoundGraph,
						NAME_None,
						GraphClass,
						SchemaClass);
					check(PropertyGraph);

					FEdGraphUtilities::RenameGraphToNameOrCloseToName(PropertyGraph, *GraphProperty->GetDisplayName().ToString());

					CastChecked<USMPropertyGraph>(PropertyGraph)->TempGraphProperty = GraphProperty;

					// Initialize the property graph
					const UEdGraphSchema* Schema = PropertyGraph->GetSchema();
					Schema->CreateDefaultNodesForGraph(*PropertyGraph);
				}

				BoundGraph->SubGraphs.AddUnique(PropertyGraph);

				// Record the template used.
				GraphPropertyTemplates.Add(Guid, Template);
				
				// Look for placed property nodes and link them.
				InitPropertyGraphNodes(PropertyGraph, GraphProperty);

				// Clear out temp property as it won't be valid after this scope.
				CastChecked<USMPropertyGraph>(PropertyGraph)->TempGraphProperty = nullptr;

				GraphPropertyGraphs.Add(Guid, PropertyGraph);
				bHasChanged = true;
			}
		}
		
		bNativeGuidConversion = false;
		Template->ResetArrayCheck();
		ArraySwapData.Remove(Template);
	}

	return bHasChanged;
}

void USMGraphNode_Base::RemoveGraphPropertyGraphsForTemplate(USMNodeInstance* Template)
{
	if (Template == nullptr)
	{
		return;
	}
	
	TArray<FGuid> GuidsToRemove;
	for (const TTuple<FGuid, TObjectPtr<USMNodeInstance>>& GuidToTemplateKeyVal : GraphPropertyTemplates)
	{
		if (GuidToTemplateKeyVal.Value == Template)
		{
			GuidsToRemove.Add(GuidToTemplateKeyVal.Key);
		}
	}

	for (const FGuid& Guid : GuidsToRemove)
	{
		if (USMPropertyGraph* PropertyGraph = Cast<USMPropertyGraph>(GetGraphPropertyGraph(Guid)))
		{
			RemovePropertyGraph(PropertyGraph, true);
		}
	}
}

UEdGraph* USMGraphNode_Base::GetGraphPropertyGraph(const FGuid& Guid) const
{
	return GraphPropertyGraphs.FindRef(Guid);
}

USMGraphK2Node_PropertyNode_Base* USMGraphNode_Base::GetGraphPropertyNode(const FGuid& Guid) const
{
	return GraphPropertyNodes.FindRef(Guid);
}

USMGraphK2Node_PropertyNode_Base* USMGraphNode_Base::GetGraphPropertyNode(const FName& VariableName, const USMNodeInstance* TemplateMatch, int32 Index) const
{
	TArray<USMGraphK2Node_PropertyNode_Base*> FoundPropertyNodes = GetGraphPropertyNodes(VariableName, TemplateMatch, Index);
	if (FoundPropertyNodes.Num() > 0)
	{
		return FoundPropertyNodes[0];
	}

	return nullptr;
}

TArray<USMGraphK2Node_PropertyNode_Base*> USMGraphNode_Base::GetGraphPropertyNodes(const FName& VariableName,
	const USMNodeInstance* TemplateMatch, int32 Index) const
{
	TArray<USMGraphK2Node_PropertyNode_Base*> FoundProperties;

	for (const TTuple<FGuid, TObjectPtr<USMGraphK2Node_PropertyNode_Base>>& KeyVal : GraphPropertyNodes)
	{
		if (!KeyVal.Value)
		{
			continue;
		}
		const FSMGraphProperty_Base* GraphProperty = KeyVal.Value->GetPropertyNodeChecked();
		if (GraphProperty->VariableName == VariableName)
		{
			if (TemplateMatch && TemplateMatch->GetTemplateGuid() != GraphProperty->GetTemplateGuid())
			{
				continue;
			}

			if (Index != INDEX_NONE && GraphProperty->ArrayIndex != Index)
			{
				continue;
			}

			FoundProperties.Add(KeyVal.Value);
		}
	}

	return FoundProperties;
}

TArray<USMGraphK2Node_PropertyNode_Base*> USMGraphNode_Base::GetAllPropertyGraphNodesAsArray(const USMNodeInstance* TemplateMatch) const
{
	TArray<USMGraphK2Node_PropertyNode_Base*> Nodes;
	Nodes.Reserve(GraphPropertyNodes.Num());
	
	for (const TTuple<FGuid, TObjectPtr<USMGraphK2Node_PropertyNode_Base>>& KeyVal : GraphPropertyNodes)
	{
		if (KeyVal.Value)
		{
			const FSMGraphProperty_Base* GraphProperty = KeyVal.Value->GetPropertyNodeChecked();
			if (TemplateMatch && TemplateMatch->GetTemplateGuid() != GraphProperty->GetTemplateGuid())
			{
				continue;
			}

			Nodes.Add(KeyVal.Value);
		}
	}

	return Nodes;
}

void USMGraphNode_Base::InitPropertyGraphNodes(UEdGraph* PropertyGraph, FSMGraphProperty_Base* Property)
{
	Modify();
	
	TArray<USMGraphK2Node_PropertyNode_Base*> PropertyNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_PropertyNode_Base>(PropertyGraph, PropertyNodes);
	for (USMGraphK2Node_PropertyNode_Base* PlacedPropertyNode : PropertyNodes)
	{
		PlacedPropertyNode->Modify();
		PlacedPropertyNode->OwningGraphNode = this;
		PlacedPropertyNode->SetPropertyNode(Property);
		PlacedPropertyNode->SetPinValueFromPropertyDefaults(bJustPasted, /*bUseArchetype*/ !bJustPasted);
		GraphPropertyNodes.Add(Property->GetGuid(), PlacedPropertyNode);
	}
}

void USMGraphNode_Base::RefreshAllProperties(bool bModify, bool bSetFromPinFirst)
{
	for (const auto& KeyVal : GetAllPropertyGraphs())
	{
		if (USMPropertyGraph* PropertyGraph = Cast<USMPropertyGraph>(KeyVal.Value))
		{
			PropertyGraph->RefreshProperty(bModify, bSetFromPinFirst);
		}
	}
}

void USMGraphNode_Base::RefreshPropertyByGuid(const FGuid& Guid, bool bModify, bool bSetFromPinFirst)
{
	if (USMPropertyGraph* PropertyGraph = Cast<USMPropertyGraph>(GetGraphPropertyGraph(Guid)))
	{
		PropertyGraph->RefreshProperty(bModify, bSetFromPinFirst);
	}
}

void USMGraphNode_Base::ForceRecreateProperties()
{
	CreateGraphPropertyGraphs();
	RefreshAllProperties(false, false);
}

void USMGraphNode_Base::SetGraphPropertyDefaultsFromPins()
{
	for (const auto& Property : GraphPropertyNodes)
	{
		Property.Value->SetPropertyDefaultsFromPin();
	}
}

void USMGraphNode_Base::SetPinsFromGraphProperties(bool bUseArchetype)
{
	for (const auto& Property : GraphPropertyNodes)
	{
		Property.Value->SetPinValueFromPropertyDefaults(false, bUseArchetype, true);
	}
}

USMGraphK2Node_PropertyNode_Base* USMGraphNode_Base::GetPropertyNodeUnderMouse() const
{
	for (const auto& KeyVal : GetAllPropertyGraphNodes())
	{
		if (KeyVal.Value->bMouseOverNodeProperty)
		{
			return KeyVal.Value;
		}
	}

	return nullptr;
}

UEdGraphPin* USMGraphNode_Base::GetInputPin() const
{
	if (Pins.Num() == 0 || Pins[INDEX_PIN_INPUT]->Direction == EGPD_Output)
	{
		return nullptr;
	}

	return Pins[INDEX_PIN_INPUT];
}

UEdGraphPin* USMGraphNode_Base::GetOutputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			return Pin;
		}
	}

	return nullptr;
}

UEdGraphNode* USMGraphNode_Base::GetOutputNode() const
{
	UEdGraphPin* OutputPin = GetOutputPin();

	if (OutputPin)
	{
		if (OutputPin->LinkedTo.Num() > 0 && OutputPin->LinkedTo[INDEX_PIN_INPUT]->GetOwningNode() != nullptr)
		{
			return OutputPin->LinkedTo[INDEX_PIN_INPUT]->GetOwningNode();
		}
	}

	return nullptr;
}

void USMGraphNode_Base::GetAllOutputNodes(TArray<UEdGraphNode*>& OutNodes) const
{
	UEdGraphPin* OutputPin = GetOutputPin();

	if (OutputPin)
	{
		for (int32 Idx = 0; Idx < OutputPin->LinkedTo.Num(); ++Idx)
		{
			OutNodes.Add(OutputPin->LinkedTo[Idx]->GetOwningNode());
		}
	}
}

FString USMGraphNode_Base::GetNodeName() const
{
	return BoundGraph ? *(BoundGraph->GetName()) : TEXT("(null)");
}

bool USMGraphNode_Base::SetNodeName(const FString& NewName, FText& OutErrorMessage)
{
	const FString SanitizedName = FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRestrictInvalidCharacters
									? FSMBlueprintEditorUtils::GetSafeStateName(NewName)
									: NewName;
	
	TSharedPtr<INameValidatorInterface> NameValidator = MakeNameValidator();
	if (!NameValidator.IsValid())
	{
		if (const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			const FName NodeName(GetNodeTitle(ENodeTitleType::EditableTitle).ToString());
			NameValidator = MakeShareable(new FKismetNameValidator(Blueprint, NodeName));
		}
	}
	const EValidatorResult ValidatorResult = NameValidator->IsValid(SanitizedName);
	if (ValidatorResult == EValidatorResult::AlreadyInUse)
	{
		OutErrorMessage = FText::Format(LOCTEXT("RenameFailed_InUse", "'{0}' is in use by another variable or function!"), FText::FromString(SanitizedName));
	}
	else if (ValidatorResult == EValidatorResult::EmptyName)
	{
		OutErrorMessage = LOCTEXT("RenameFailed_LeftBlank", "Names cannot be left blank!");
	}
	else if (ValidatorResult == EValidatorResult::TooLong)
	{
		OutErrorMessage = FText::Format(LOCTEXT("RenameFailed_NameTooLong", "Names must have fewer than {0} characters!"), FText::AsNumber(FKismetNameValidator::GetMaximumNameLength()));
	}
	else if (ValidatorResult == EValidatorResult::ContainsInvalidCharacters)
	{
		OutErrorMessage = LOCTEXT("RenameFailed_InvalidCharacters", "Name contains invalid characters!");
	}
	else if (ValidatorResult == EValidatorResult::LocallyInUse)
	{
		OutErrorMessage = LOCTEXT("RenameFailed_LocallyInUse", "Name is locally in use!");
	}
	else if (ValidatorResult == EValidatorResult::Ok)
	{
		SetNodeName_Direct(SanitizedName);
		return true;
	}

	if (!OutErrorMessage.IsEmpty())
	{
		// Don't log error or warning, up to caller how they want to treat this.
		LDEDITOR_LOG_INFO(TEXT("Cannot rename node '%s'. %s"), *GetNodeName(), *OutErrorMessage.ToString());
	}

	return false;
}

void USMGraphNode_Base::ResetNodeName()
{
	USMGraphSchema::RenameNodeFromTemplateData(this);
}

TScriptInterface<ISMEditorGraphNode_StateBaseInterface> USMGraphNode_Base::AsStateBaseInterface()
{
	return Implements<USMEditorGraphNode_StateBaseInterface>() ? this : nullptr;
}

void USMGraphNode_Base::SetNodeName_Direct(const FString& InNewName)
{
	if (BoundGraph)
	{
		const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
		FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, InNewName);
	}
}

void USMGraphNode_Base::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	// Always set by compiler if needed.
	DuplicatedNodeGuids.Empty();

	if (!BoundGraph)
	{
		return;
	}

	bIsPreCompiling = true;
	
	/** Call in case the version manager hasn't run. */
	ConvertToCurrentVersion();
	
	ResetLogMessages();

	if (CompilerContext.CompileOptions.CompileType == EKismetCompileType::Full &&
		GetNodeClass() && !GetNodeClass()->bLayoutChanging &&
		NodeInstanceTemplate &&
		NodeInstanceTemplate->GetClass()->GetName().StartsWith(TEXT("REINST_")))
	{
		// Not sure how we can get into this state, possibly due to changing the node class in some way
		// and unreal saving the REINST class to disk. Catch the issue here, otherwise a check fails at runtime.
		// It's not safe to reinit the template in this case unless it's a full compile.
		CompilerContext.MessageLog.Warning(TEXT("Fixing template REINST class mismatch for node @@."), this);
		InitTemplate();
	}
	
	TMap<FGuid, TObjectPtr<UEdGraph>> PropertyGraphs = GetAllPropertyGraphs();

	for (const auto& KeyVal : PropertyGraphs)
	{
		if (KeyVal.Value == nullptr)
		{
			GraphPropertyGraphs.Remove(KeyVal.Key);
			GraphPropertyNodes.Remove(KeyVal.Key);
			GraphPropertyTemplates.Remove(KeyVal.Key);
			
			CompilerContext.MessageLog.Error(TEXT("Property graph missing on load on node @@."), this);
		}
	}

	SetReadOnlyNodePosition(FVector2D(NodePosX, NodePosY));
	ForceRecreateProperties();

	for (const TTuple<FGuid, TObjectPtr<USMGraphK2Node_PropertyNode_Base>>& GraphPropertyKeyVal : GraphPropertyNodes)
	{
		if (GraphPropertyKeyVal.Value)
		{
			GraphPropertyKeyVal.Value->PreCompile(CompilerContext);
		}
	}

	bIsPreCompiling = false;
}

void USMGraphNode_Base::PreCompileNodeInstanceValidation(FCompilerResultsLog& MessageLog, USMCompilerLog* CompilerLog, USMGraphNode_Base* OwningNode)
{
	check(CompilerLog);

	TWeakObjectPtr<USMGraphNode_Base> GraphNodeWeakPtr(OwningNode);
	TWeakObjectPtr<USMGraphNode_Base> GraphNodeWeakPtrThis(this);

	// Standard compiler log.
	CompilerLog->OnCompilerLogEvent.BindLambda([&, GraphNodeWeakPtr, GraphNodeWeakPtrThis](ESMCompilerLogType Severity, const FString& Message)
	{
		if (!GraphNodeWeakPtrThis.IsValid())
		{
			return;
		}
		if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting == ESMEditorConstructionScriptProjectSetting::SM_Legacy)
		{
			LDEDITOR_LOG_WARNING(TEXT("OnPreCompileValidate called with EditorNodeConstructionScriptSetting set to Legacy. This will limit functionality. Set to 'Compile' or 'Standard'."))
		}
		
		const FString FinalMessage = Message + (GraphNodeWeakPtr.IsValid() ? TEXT(" - Reference @@ node @@") : TEXT(" - @@"));
		FSMKismetCompilerContext::LogCompilerMessage(MessageLog, FinalMessage, Severity, GraphNodeWeakPtr.Get(),
			GraphNodeWeakPtrThis.Get());
	});

	// Property logging.
	CompilerLog->OnCompilerLogPropertyEvent.BindLambda([&, GraphNodeWeakPtr, GraphNodeWeakPtrThis](
		const FName& PropertyName, int32 ArrayIndex, const FString& Message,
		ESMCompilerLogType Severity, bool bHighlight, bool bSilent, const USMNodeInstance* NodeInstance)
	{
		if (!GraphNodeWeakPtrThis.IsValid())
		{
			return;
		}
		
		if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting == ESMEditorConstructionScriptProjectSetting::SM_Legacy)
		{
			LDEDITOR_LOG_WARNING(TEXT("OnPreCompileValidate called with EditorNodeConstructionScriptSetting set to Legacy. This will limit functionality. Set to 'Compile' or 'Standard'."))
		}

		TArray<USMGraphK2Node_PropertyNode_Base*> PropertyNodes = GetGraphPropertyNodes(PropertyName, NodeInstance, ArrayIndex);
		if (PropertyNodes.Num() == 0)
		{
			LDEDITOR_LOG_WARNING(TEXT("OnPreCompileValidate could not locate graph properties for property name '%s' on node '%s'."), *PropertyName.ToString(), *GetNodeName());
		}
		else
		{
			for (USMGraphK2Node_PropertyNode_Base* PropertyNode : PropertyNodes)
			{
				USMGraphK2Node_PropertyNode_Base::FNotifyArgs NotifyArgs;
				NotifyArgs.bEnable = true;
				NotifyArgs.Message = Message;
				NotifyArgs.LogType = static_cast<ESMLogType>(Severity);
				PropertyNode->SetNotificationArgs(MoveTemp(NotifyArgs));

				if (bHighlight)
				{
					USMGraphK2Node_PropertyNode_Base::FHighlightArgs HighlightArgs;
					HighlightArgs.bEnable = true;
					HighlightArgs.Color = ISMEditorGraphPropertyNodeInterface::GetHighlightColorFromSeverity(static_cast<ESMLogType>(Severity));

					PropertyNode->SetHighlightedArgs(MoveTemp(HighlightArgs));
				}
			}
		}
		if (!bSilent)
		{
			const FString FinalMessage = Message + (GraphNodeWeakPtr.IsValid() ? TEXT(" - Reference @@ node @@") : TEXT(" - @@"));
			FSMKismetCompilerContext::LogCompilerMessage(MessageLog, FinalMessage, Severity, GraphNodeWeakPtr.Get(),
				GraphNodeWeakPtrThis.Get());
		}
	});
	
	RunPreCompileValidateForNodeInstance(NodeInstanceTemplate, CompilerLog);
}

void USMGraphNode_Base::RunPreCompileValidateForNodeInstance(const USMNodeInstance* InNodeInstance, USMCompilerLog* InCompilerLog)
{
	if (InNodeInstance && !FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		InNodeInstance->OnPreCompileValidate(InCompilerLog);
	}
}

void USMGraphNode_Base::OnCompile(FSMKismetCompilerContext& CompilerContext)
{
	if (!BoundGraph)
	{
		return;
	}

	FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(BoundGraph);
	check(RuntimeNode);
	RuntimeNode->SetNodeInstanceClass(GetNodeClass());
	if (NodeInstanceTemplate && !IsUsingDefaultNodeClass())
	{
		// We don't need the default template at runtime.
		CompilerContext.AddDefaultObjectTemplate(RuntimeNode->GetNodeGuid(), NodeInstanceTemplate, FTemplateContainer::NodeTemplate);
	}
}

void USMGraphNode_Base::OnBoundGraphRenamed(UObject* OldOuter, const FName OldName)
{
	if (GetNodeName() != OldName.ToString())
	{
		TArray<USMNodeInstance*> NodeInstances;
		GetAllNodeTemplates(NodeInstances);
	
		for (USMNodeInstance* NodeInstance : NodeInstances)
		{
			// Only generic post edit change available since the name property isn't technically changing on the node instance.
			if (NodeInstance)
			{
				NodeInstance->PostEditChange();
			}
		}
	}
}

void USMGraphNode_Base::ResetDebugState()
{
	// Prevents a previous cycle from showing it as running.
	if (const FSMNode_Base* DebugNode = GetDebugNode())
	{
		DebugNode->bWasActive = bWasDebugActive = false;
	}
}

void USMGraphNode_Base::OnWidgetConstruct()
{
	ResetDebugState();
}

void USMGraphNode_Base::UpdateTime(float DeltaTime)
{
	if (const FSMNode_Base* DebugNode = GetDebugNode())
	{
		MaxTimeToShowDebug = GetMaxDebugTime();
		
		// Toggle active status and reset time if switching active states.
		if (DebugNode->IsActive() || (DebugNode->bWasActive && !WasDebugNodeActive()))
		{
			bWasDebugActive = false;

			// Was active is debug only data and exists to help us determine if we should draw an active state.
			DebugNode->bWasActive = false;
			if (!IsDebugNodeActive())
			{
				bIsDebugActive = true;
				DebugTotalTime = 0.f;
			}
		}
		else if (IsDebugNodeActive())
		{
			bWasDebugActive = true;
			bIsDebugActive = false;
			DebugTotalTime = 0.f;
		}
		// In the event a node is no longer active but is still being reported it is and we don't want to display it any more.
		else if (WasDebugNodeActive() && !IsDebugNodeActive() && DebugTotalTime >= MaxTimeToShowDebug)
		{
			bWasDebugActive = false;
		}
		else
		{
			DebugTotalTime += DeltaTime;
		}
	}
	else
	{
		bIsDebugActive = bWasDebugActive = false;
	}
}

void USMGraphNode_Base::ResetLogMessages()
{
	CollectedLogs.Empty();
	bHasCompilerMessage = false;
}

void USMGraphNode_Base::UpdateErrorMessageFromLogs()
{
	bHasCompilerMessage = TryGetNodeLogMessage(ErrorMsg, ErrorType);
}

void USMGraphNode_Base::AddNodeLogMessage(const FSMGraphNodeLog& Message)
{
	CollectedLogs.Add(Message);
}

bool USMGraphNode_Base::TryGetNodeLogMessage(FString& OutMessage, int32& OutSeverity) const
{
	int32 Severity = EMessageSeverity::Info;
	FString Message;
	for (const FSMGraphNodeLog& Log : CollectedLogs)
	{
		if (!Message.IsEmpty())
		{
			Message += "\n";
		}

		Message += Log.NodeMessage;

		if (Log.LogType < Severity)
		{
			Severity = Log.LogType;
		}
	}

	OutMessage = Message;
	OutSeverity = Severity;

	return CollectedLogs.Num() > 0;
}

void USMGraphNode_Base::OnNodeMoved(const FVector2D& NewPosition)
{
	SetReadOnlyNodePosition(NewPosition);
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void USMGraphNode_Base::SetReadOnlyNodePosition()
{
	SetReadOnlyNodePosition(
		FVector2D(
			static_cast<float>(NodePosX),
			static_cast<float>(NodePosY)));
}

void USMGraphNode_Base::GoToLocalGraph() const
{
	return JumpToDefinition();
}

const FGuid& USMGraphNode_Base::GetCorrectNodeGuid(bool* bIsRunTimeGuid) const
{
	if (const FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(GetBoundGraph()))
	{
		if (bIsRunTimeGuid)
		{
			*bIsRunTimeGuid = true;
		}

		return RuntimeNode->GetNodeGuid();
	}

	if (bIsRunTimeGuid)
	{
		*bIsRunTimeGuid = false;
	}

	return NodeGuid;
}

void USMGraphNode_Base::SetReadOnlyNodePosition(const FVector2D& Position)
{
	NodePosition = Position;
}

void USMGraphNode_Base::InitTemplate()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("USMGraphNode_Base::InitTemplate"), STAT_InitTemplate, STATGROUP_LogicDriverEditor);
	
	const UClass* NodeClass = GetNodeClass();
	UClass* DefaultNodeClass = GetDefaultNodeClass();
	if (NodeClass == nullptr)
	{
		/* No longer allow null classes.
		 * The default class is used to configure shared properties for all states.
		 * A default template is not needed at runtime and won't be copied to the CDO. */
		if (!DefaultNodeClass)
		{
			ensureAlways(GetClass() == USMGraphNode_StateMachineEntryNode::StaticClass() ||
				GetClass() == USMGraphNode_AnyStateNode::StaticClass() ||
				GetClass() == USMGraphNode_LinkStateNode::StaticClass() ||
				GetClass() == USMGraphNode_RerouteNode::StaticClass());
			return;
		}
		
		SetNodeClass(DefaultNodeClass);
		return;
	}

	if (NodeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this);
		LDEDITOR_LOG_ERROR(TEXT("Blueprint %s tried to instantiate the abstract node class %s for node %s. Please update the node class."),
			Blueprint ? *Blueprint->GetName() : TEXT("(null)"), *NodeClass->GetName(), *GetNodeName());
		return;
	}

	Modify();

	FString NodeName = GetName();
	NodeName = FSMBlueprintEditorUtils::GetSafeName(NodeName);
	
	const FString TemplateName = FString::Printf(TEXT("NODE_TEMPLATE_%s_%s_%s"), *NodeName, *NodeClass->GetName(), *FGuid::NewGuid().ToString());
	USMNodeInstance* NewTemplate = NewObject<USMNodeInstance>(this, NodeClass, *TemplateName, RF_ArchetypeObject | RF_Transactional | RF_Public);

	UClass* OldClass = nullptr;
	
	if (NodeInstanceTemplate)
	{
		OldClass = NodeInstanceTemplate->GetClass();
		
		NodeInstanceTemplate->Modify();
		
		if (NewTemplate)
		{
			UEngine::CopyPropertiesForUnrelatedObjects(NodeInstanceTemplate, NewTemplate);
		}

		// Original template isn't needed any more.
		DestroyTemplate();
	}

	NodeInstanceTemplate = NewTemplate;

	// We only want a template for default classes.
	if (NodeClass != DefaultNodeClass && NodeInstanceTemplate)
	{
		const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
		if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Legacy && !IsBeingPasted() && !FUObjectThreadContext::Get().IsRoutingPostLoad)
		{
			NodeInstanceTemplate->RunConstructionScript();
		}
	}
	
	// Need to recreate property graphs before reconstructing the node otherwise properties will mismatch and cause a crash.
	if (BoundGraph && !bJustPasted)
	{
		const bool bGenerateNewGuids = (OldClass && LoadedVersion == CURRENT_VERSION) &&
			(OldClass->IsChildOf(NodeClass) || NodeClass->IsChildOf(OldClass));
		// We only need new guids if manually switching to/from an inherited class.
		// Otherwise the properties won't evaluate properly and are still tied to the old class.
		CreateGraphPropertyGraphs(bGenerateNewGuids);
	}

	if (NodeClass != DefaultNodeClass)
	{
		PlaceDefaultInstanceNodes();
	}
	
	// Template may have new widgets to display.
	if (!bJustPasted)
	{
		// Pasting by default will call reconstruct node.
		ReconstructNode();
	}
}

void USMGraphNode_Base::DestroyTemplate()
{
	if (NodeInstanceTemplate)
	{
		NodeInstanceTemplate->Modify();
		FSMBlueprintEditorUtils::TrashObject(NodeInstanceTemplate);
		NodeInstanceTemplate = nullptr;
	}
}

void USMGraphNode_Base::RunAllConstructionScripts()
{
	if (IsRunningConstructionScripts() || HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
	{
		return;
	}

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("USMGraphNode_Base::RunAllConstructionScripts"), STAT_RunAllNodeConstructionScripts, STATGROUP_LogicDriverEditor);

	bIsRunningConstructionScripts = true;
	
	// Load user entered pins to the default object. Even nodes without construction scripts still need to do this so other nodes can read them.
	SetGraphPropertyDefaultsFromPins();

	// Optimization for nodes that have no construction scripts defined.
	if (!DoesNodePossiblyHaveConstructionScripts())
	{
		RestoreArchetypeValuesPriorToConstruction();
		// Updating pins will make sure the LastAutoGeneratedDefaultValue is set and handle FText stable localization keys.
		SetPinsFromGraphProperties(false);
		bIsRunningConstructionScripts = false;
		return;
	}
	
	// Run construction script -- It's possible the script further updates the default values.
	RunAllConstructionScripts_Internal();

	// Variable structure could have been modified by construction script -- ie changing number of array elements.
	if (!bNeedsStateStackConversion)
	{
		// Do not recreate if we are converting. It isn't necessary as construction scripts weren't supported and this
		// can prematurely reset bNeedsStateStackConversion.

		// Create silently or we risk dirtying on load. TODO: There should be a better way of preventing this.
		bCreatePropertyGraphsSilently = true;
		ForceRecreateProperties();
		bCreatePropertyGraphsSilently = false;
	}
	
	// Load the default values back to the pins in case the construction script modified them.
	SetPinsFromGraphProperties(false);
	
	bIsRunningConstructionScripts = false;
}

bool USMGraphNode_Base::DoesNodePossiblyHaveConstructionScripts() const
{
	return NodeInstanceTemplate && NodeInstanceTemplate->bHasEditorConstructionScripts;
}

void USMGraphNode_Base::DestroyAllPropertyGraphs()
{
	Modify();
	
	for (const auto& KeyVal : GetAllPropertyGraphNodes())
	{
		if (KeyVal.Value)
		{
			USMPropertyGraph* Graph = KeyVal.Value->GetPropertyGraph();
			RemovePropertyGraph(Graph, false);
		}
	}

	GraphPropertyNodes.Empty();
	GraphPropertyGraphs.Empty();
	GraphPropertyTemplates.Empty();
}

void USMGraphNode_Base::PlaceDefaultInstanceNodes()
{
	Modify();

	if (BoundGraph)
	{
		BoundGraph->Modify();
	}
}

void USMGraphNode_Base::RunAllConstructionScripts_Internal()
{
	if (NodeInstanceTemplate && !FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		NodeInstanceTemplate->RunConstructionScript();
	}
}

void USMGraphNode_Base::RestoreArchetypeValuesPriorToConstruction()
{
	if (NodeInstanceTemplate)
	{
		NodeInstanceTemplate->RestoreArchetypeValuesPriorToConstruction();
	}
}

bool USMGraphNode_Base::IsSafeToConditionallyCompile(EPropertyChangeType::Type ChangeType) const
{
	return ChangeType != EPropertyChangeType::Redirected && AreTemplatesFullyLoaded();
}

void USMGraphNode_Base::SetNodeClass(UClass* Class)
{
	InitTemplate();
}

UClass* USMGraphNode_Base::GetDefaultNodeClass() const
{
	if (FSMNode_Base* RuntimeNode = FindRuntimeNode())
	{
		return RuntimeNode->GetDefaultNodeInstanceClass();
	}

	return nullptr;
}

bool USMGraphNode_Base::IsNodeClassNative() const
{
	if (const UClass* Class = GetNodeClass())
	{
		return Class->IsNative();
	}

	return false;
}

bool USMGraphNode_Base::IsNodeFastPathEnabled() const
{
	if (BoundGraph == nullptr)
	{
		return false;
	}
	
	if (bFastPathEnabledCached.IsSet())
	{
		return *bFastPathEnabledCached;
	}

	bool bIsFastPath = true;
	
	TArray<USMGraphK2Node_RuntimeNode_Base*> RootNodeList;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNode_Base>(BoundGraph, RootNodeList);

	for (const USMGraphK2Node_RuntimeNode_Base* RootNode : RootNodeList)
	{
		if (RootNode->IsConsideredForEntryConnection() && !RootNode->IsFastPathEnabled())
		{
			bIsFastPath = false;
			break;
		}
	}

	bFastPathEnabledCached = bIsFastPath;
	return bIsFastPath;
}

USMNodeInstance* USMGraphNode_Base::GetNodeTemplateFromGuid(const FGuid& Guid) const
{
	return GraphPropertyTemplates.FindRef(Guid);
}

bool USMGraphNode_Base::AreTemplatesFullyLoaded() const
{
	return NodeInstanceTemplate && !NodeInstanceTemplate->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad);
}

void USMGraphNode_Base::GetAllNodeTemplates(TArray<USMNodeInstance*>& OutNodeInstances) const
{
	OutNodeInstances.Add(GetNodeTemplate());
}

USMGraph* USMGraphNode_Base::GetOwningStateMachineGraph() const
{
	return Cast<USMGraph>(GetGraph());
}

FSMNode_Base* USMGraphNode_Base::FindRuntimeNode() const
{
	return FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(GetBoundGraph());
}

const FSMNode_Base* USMGraphNode_Base::GetDebugNode() const
{
	const USMBlueprint* Blueprint = CastChecked<USMBlueprint>(FSMBlueprintEditorUtils::FindBlueprintForNode(this));

	if (const USMInstance* Instance = Cast<USMInstance>(Blueprint->GetObjectBeingDebugged()))
	{
		if (const FSMNode_Base* RuntimeNode = FindRuntimeNode())
		{
			Instance = Instance->GetPrimaryReferenceOwnerConst();
			const FSMDebugStateMachine& DebugMachine = Instance->GetDebugStateMachineConst();

			auto IsValidAndActiveRuntimeNode = [](const FSMNode_Base* InNode)
			{
				return InNode != nullptr && (InNode->IsDebugActive() || InNode->WasDebugActive());
			};

			// Find the real runtime node being debugged. It's possible this node may been duplicated multiple
			// times at run-time but only has one graph node representation, such as an Any State transition.

			const FSMNode_Base* RealRuntimeNode = DebugMachine.GetRuntimeNode(RuntimeNode->GetNodeGuid());
			if (!IsValidAndActiveRuntimeNode(RealRuntimeNode))
			{
				for (const FGuid& DuplicateNodeGuid : DuplicatedNodeGuids)
				{
					const FSMNode_Base* DuplicateRuntimeNode = DebugMachine.GetRuntimeNode(DuplicateNodeGuid);
					if (IsValidAndActiveRuntimeNode(DuplicateRuntimeNode))
					{
						// In the event there are multiple duplications, choose the one that is most active.
						return DuplicateRuntimeNode;
					}

					if (!RealRuntimeNode && DuplicateRuntimeNode)
					{
						// Fall back to any node; we would rather have a node that isn't active then none at all.
						RealRuntimeNode = DuplicateRuntimeNode;
					}
				}
			}

			return RealRuntimeNode;
		}
	}

	return nullptr;
}

float USMGraphNode_Base::GetMaxDebugTime() const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	return Settings->TimeToDisplayLastActiveState + Settings->TimeToFadeLastActiveState;
}

FLinearColor USMGraphNode_Base::GetBackgroundColor() const
{
	const FLinearColor* CustomColor = GetCustomBackgroundColor();
	const FLinearColor BaseColor = Internal_GetBackgroundColor() * (CustomColor ? *CustomColor : FLinearColor(1.f, 1.f, 1.f, 1.f));
	const FLinearColor ActiveColor = GetActiveBackgroundColor();

	if (GetDebugNode() == nullptr)
	{
		return BaseColor;
	}

	if (IsDebugNodeActive())
	{
		return ActiveColor;
	}

	const float TimeToFade = 0.7f;
	const float DebugTime = GetDebugTime();

	if (bWasDebugActive && DebugTime < TimeToFade)
	{
		return FLinearColor::LerpUsingHSV(ActiveColor, BaseColor, DebugTime / TimeToFade);
	}

	return BaseColor;
}

FLinearColor USMGraphNode_Base::GetActiveBackgroundColor() const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	return Settings->ActiveStateColor;
}

const FSlateBrush* USMGraphNode_Base::GetNodeIcon() const
{
	if (const USMNodeInstance* Instance = GetNodeTemplate())
	{
		if (Instance->HasCustomIcon())
		{
			UTexture2D* Texture = Instance->GetNodeIcon();
			const FString TextureName = Texture ? Texture->GetFullName() : FString();
			const FVector2D Size = Instance->GetNodeIconSize();
			const FLinearColor TintColor = Instance->GetNodeIconTintColor();
			if (CachedTexture != TextureName || CachedTextureSize != Size || CachedNodeTintColor != TintColor)
			{
				CachedTexture = TextureName;
				CachedTextureSize = Size;
				CachedNodeTintColor = TintColor;
				FSlateBrush Brush;
				if (Texture)
				{
					Brush.SetResourceObject(Texture);
					Brush.ImageSize = Size.GetMax() > 0 ? Size : FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
					Brush.TintColor = TintColor;
				}
				else
				{
					Brush = FSlateNoResource();
				}
				
				CachedBrush = MakeShared<FSlateBrush>(Brush);
			}

			return CachedBrush.Get();
		}
	}

	return nullptr;
}

bool USMGraphNode_Base::ConvertToCurrentVersion(bool bOnlyOnLoad)
{	
	if ((!IsTemplate() && GetLinker() && GetLinker()->IsPersistent() && GetLinker()->IsLoading()) || !bOnlyOnLoad)
	{
		if (GetDefaultNodeClass())
		{
			if (NodeInstanceTemplate == nullptr)
			{
				// Configure pre 2.3 nodes that are missing node instance templates.
				InitTemplate();
			}
			if (LoadedVersion < TEMPLATE_PROPERTY_VERSION)
			{
				// Pre 2.3 nodes need to have their properties imported to the node instance template.
				ImportDeprecatedProperties();
			}
			if (LoadedVersion < STATESTACK_VERSION)
			{
				// Pre 2.4 nodes need their graph property guids updated to account for different node templates.
				
				bRequiresGuidRegeneration = true;
				bNeedsStateStackConversion = true;

				if (!bIsPreCompiling)
				{
					// If pre-compile isn't what called the upgrade method we need to force recreate the properties
					// as a precaution because the booleans aren't saved and this is a one time upgrade. Testing
					// show that this branch shouldn't be called.
					ForceRecreateProperties();
					bRequiresGuidRegeneration = false;
					bNeedsStateStackConversion = false;
				}
			}
		}

		OnConvertToCurrentVersion(bOnlyOnLoad);
		return SetToCurrentVersion();
	}

	return false;
}

bool USMGraphNode_Base::SetToCurrentVersion()
{
	const int32 LastVersion = LoadedVersion;
	LoadedVersion = CURRENT_VERSION;

	return LastVersion != LoadedVersion;
}

void USMGraphNode_Base::ForceSetVersion(int32 NewVersion)
{
	LoadedVersion = NewVersion;
}

void USMGraphNode_Base::RequestSlateRefresh(bool bFullRefresh)
{
	OnGraphNodeRefreshRequestedEvent.Broadcast(this, bFullRefresh);
}

void USMGraphNode_Base::ResetCachedValues()
{
	bFastPathEnabledCached.Reset();
}

void USMGraphNode_Base::RecordDuplicatedNodeGuid(const FGuid& InGuid)
{
	DuplicatedNodeGuids.Add(InGuid);
}

void USMGraphNode_Base::OnBlueprintCacheCleared(const USMBlueprint* Blueprint)
{
	if (Blueprint == FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		ResetCachedValues();
	}
}

FLinearColor USMGraphNode_Base::Internal_GetBackgroundColor() const
{
	return FLinearColor::Black;
}

const FLinearColor* USMGraphNode_Base::GetCustomBackgroundColor(const USMNodeInstance* NodeInstance) const
{
	if (!NodeInstance)
	{
		NodeInstance = NodeInstanceTemplate;
	}
	
	if (!NodeInstance || !NodeInstance->HasCustomColor())
	{
		return nullptr;
	}

	return &NodeInstance->GetNodeColor();
}

void USMGraphNode_Base::RemovePropertyGraph(USMPropertyGraph* PropertyGraph, bool RemoveFromMaps)
{
	if (!PropertyGraph)
	{
		return;
	}

	PropertyGraph->Modify();
	PropertyGraph->ResultNode->Modify();
	
	if (RemoveFromMaps)
	{
		const FGuid& Guid = PropertyGraph->ResultNode->GetPropertyNode()->GetGuid();
		GraphPropertyGraphs.Remove(Guid);
		GraphPropertyNodes.Remove(Guid);
		GraphPropertyTemplates.Remove(Guid);
	}
	
	if (FSMBlueprintEditor* Editor = FSMBlueprintEditorUtils::GetStateMachineEditor(this))
	{
		Editor->CloseDocumentTab(PropertyGraph);
	}

	if (UEdGraph* ParentGraph = Cast<UEdGraph>(PropertyGraph->GetOuter()))
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Remove(PropertyGraph);
	}

	if (PropertyGraph->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
	{
		FSMBlueprintEditorUtils::TrashObject(PropertyGraph);
	}
	else
	{
		UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		FSMBlueprintEditorUtils::RemoveGraph(Blueprint, PropertyGraph, EGraphRemoveFlags::None);
	}
}

void USMGraphNode_Base::HandlePropertyGraphArrayRemoval(TArray<FSMGraphProperty_Base*>& GraphProperties,
	TArray<TSharedPtr<FSMGraphProperty>>& TempGraphProperties, FProperty* TargetProperty, int32 RemovalIndex, int32 ArraySize,
	const FSMGraphProperty* OverrideGraphProperty, USMNodeInstance* Template)
{
	FSMGraphProperty TempProperty;

	if (OverrideGraphProperty)
	{
		// Assign override defaults before assigning a guid.
		TempProperty = *OverrideGraphProperty;
	}

	// The property index being removed.
	LD::PropertyUtils::SetGraphPropertyFromProperty(TempProperty, TargetProperty, Template, RemovalIndex);
	TObjectPtr<UEdGraph> PropertyGraphToRemove = GraphPropertyGraphs.FindRef(TempProperty.GetGuid());
	
	// Remove the graph for the deleted index.
	if (PropertyGraphToRemove)
	{
		RemovePropertyGraph(Cast<USMPropertyGraph>(PropertyGraphToRemove), true);
	}

	// The current graph array size hasn't been adjusted for the removal yet and we need to iterate through everything.
	ArraySize++; 

	// Reassign all guids that follow next in the array to their current index - 1.
	for (int32 NextIdx = RemovalIndex + 1; NextIdx < ArraySize; ++NextIdx)
	{
		TSharedPtr<FSMGraphProperty> NextGraphProperty = MakeShared<FSMGraphProperty>();

		if (OverrideGraphProperty)
		{
			// Assign override defaults before assigning a guid.
			*NextGraphProperty = *OverrideGraphProperty;
		}

		LD::PropertyUtils::SetGraphPropertyFromProperty(*NextGraphProperty, TargetProperty, Template, NextIdx);
		const FGuid OldGuid = NextGraphProperty->GetGuid();

		TObjectPtr<UEdGraph> NextGraph = GraphPropertyGraphs.FindRef(OldGuid);
		TObjectPtr<USMGraphK2Node_PropertyNode_Base> NextNode = GraphPropertyNodes.FindRef(OldGuid);
		
		// Remove the old guids.
		GraphPropertyGraphs.Remove(OldGuid);
		GraphPropertyNodes.Remove(OldGuid);
		TObjectPtr<USMNodeInstance> OldTemplate = nullptr;
		GraphPropertyTemplates.RemoveAndCopyValue(OldGuid, OldTemplate);

		// Update the new guids.
		if (OldTemplate != nullptr)
		{
			LD::PropertyUtils::SetGraphPropertyFromProperty(*NextGraphProperty, TargetProperty, OldTemplate, NextIdx - 1);
		}
		if (NextGraph && NextNode)
		{
			const FGuid& NewGuid = NextGraphProperty->GetGuid();
			
			GraphPropertyGraphs.Add(NewGuid, NextGraph);
			GraphPropertyNodes.Add(NewGuid, NextNode);
			GraphPropertyTemplates.Add(NewGuid, Template);
		}

		TempGraphProperties.Add(NextGraphProperty);
		GraphProperties.Add(NextGraphProperty.Get());
	}
}

void USMGraphNode_Base::HandlePropertyGraphArrayInsertion(TArray<FSMGraphProperty_Base*>& GraphProperties,
	TArray<TSharedPtr<FSMGraphProperty>>& TempGraphProperties, FProperty* TargetProperty, int32 InsertionIndex, int32 ArraySize,
	const FSMGraphProperty* OverrideGraphProperty, USMNodeInstance* Template, bool bDuplicate)
{
	// Reassign this guid and all guids that follow next in the array to their current index + 1.
	// Go backwards since the previous index would overwrite the next index.
	for (int32 NextIdx = ArraySize - 1; NextIdx >= InsertionIndex; --NextIdx)
	{
		FSMGraphProperty NextGraphProperty;

		if (OverrideGraphProperty)
		{
			// Assign override defaults before assigning a guid.
			NextGraphProperty = *OverrideGraphProperty;
		}

		LD::PropertyUtils::SetGraphPropertyFromProperty(NextGraphProperty, TargetProperty, Template, NextIdx);
		const FGuid OldGuid = NextGraphProperty.GetGuid();

		TObjectPtr<UEdGraph> NextGraph = GraphPropertyGraphs.FindRef(OldGuid);
		TObjectPtr<USMGraphK2Node_PropertyNode_Base> NextNode = GraphPropertyNodes.FindRef(OldGuid);

		// Remove the old guids.
		GraphPropertyGraphs.Remove(OldGuid);
		GraphPropertyNodes.Remove(OldGuid);
		TObjectPtr<USMNodeInstance> OldTemplate = nullptr;
		GraphPropertyTemplates.RemoveAndCopyValue(OldGuid, OldTemplate);

		// Update the new guids.
		if (OldTemplate != nullptr)
		{
			// Can be nullptr on array add (not insertion)
			LD::PropertyUtils::SetGraphPropertyFromProperty(NextGraphProperty, TargetProperty, OldTemplate, NextIdx + 1);
		}
		
		if (NextGraph && NextNode)
		{
			const FGuid& NewGuid = NextGraphProperty.GetGuid();

			GraphPropertyGraphs.Add(NewGuid, NextGraph);
			GraphPropertyNodes.Add(NewGuid, NextNode);
			GraphPropertyTemplates.Add(NewGuid, OldTemplate);
		}
	}

	// Add the temp graph properties including the insertion property now that their guids have been updated properly.
	for (int32 Idx = InsertionIndex; Idx < ArraySize; ++Idx)
	{
		TSharedPtr<FSMGraphProperty> NextGraphProperty = MakeShared<FSMGraphProperty>();

		if (OverrideGraphProperty)
		{
			// Assign override defaults before assigning a guid.
			*NextGraphProperty = *OverrideGraphProperty;
		}
		
		LD::PropertyUtils::SetGraphPropertyFromProperty(*NextGraphProperty, TargetProperty, Template, Idx);

		TempGraphProperties.Add(NextGraphProperty);
		GraphProperties.Add(NextGraphProperty.Get());
	}

	if (bDuplicate)
	{
		const int32 IndexToDuplicate = InsertionIndex + 1;
		check(IndexToDuplicate < GraphProperties.Num());

		FSMGraphProperty_Base* GraphPropertyToDuplicate = GraphProperties[IndexToDuplicate];

		// This guid is technically the original property. The duplicate is inserted previously.
		const FGuid& OriginalGuid = GraphPropertyToDuplicate->GenerateNewGuidIfNotValid();
		if (const TObjectPtr<UEdGraph> GraphToDuplicate = GraphPropertyGraphs.FindRef(OriginalGuid))
		{
			CastChecked<USMPropertyGraph>(GraphToDuplicate)->SetAllowDuplication(true);
			const TObjectPtr<UEdGraph> DuplicatedGraph = GraphToDuplicate->GetSchema()->DuplicateGraph(GraphToDuplicate);
			if (ensure(DuplicatedGraph))
			{
				DuplicatedGraph->Rename(nullptr, GraphToDuplicate->GetOuter(), REN_DontCreateRedirectors);

				FSMGraphProperty_Base* InsertedGraphProperty = GraphProperties[InsertionIndex];
				const FGuid& NewGuid = InsertedGraphProperty->GenerateNewGuidIfNotValid();

				// Add the graph -- similar to when a new graph is created under CreateGraphPropertyGraphsForTemplate.

				CastChecked<USMPropertyGraph>(DuplicatedGraph)->TempGraphProperty = InsertedGraphProperty;

				BoundGraph->SubGraphs.AddUnique(DuplicatedGraph);
				GraphPropertyTemplates.Add(NewGuid, Template);

				// Look for placed property nodes and link them.
				InitPropertyGraphNodes(DuplicatedGraph, InsertedGraphProperty);

				// Clear out temp property as it won't be valid after this scope.
				CastChecked<USMPropertyGraph>(DuplicatedGraph)->TempGraphProperty = nullptr;

				GraphPropertyGraphs.Add(NewGuid, DuplicatedGraph);
			}
			CastChecked<USMPropertyGraph>(GraphToDuplicate)->SetAllowDuplication(false);
		}
	}
}

void USMGraphNode_Base::HandlePropertyGraphArraySwap(TArray<FSMGraphProperty_Base*>& GraphProperties, int32 IndexA,
	int32 IndexB, USMNodeInstance* Template)
{
	check(IndexA >= 0 && IndexA < GraphProperties.Num());
	check(IndexB >= 0 && IndexB < GraphProperties.Num());

	// Swap the positions in the array, as well as the associated graphs.

	FSMGraphProperty_Base* GraphPropertyA = GraphProperties[IndexA];
	FSMGraphProperty_Base* GraphPropertyB = GraphProperties[IndexB];
	GraphProperties[IndexA] = GraphPropertyB;
	GraphProperties[IndexB] = GraphPropertyA;

	const FGuid& GuidA = GraphPropertyA->GetGuid();
	const FGuid& GuidB = GraphPropertyB->GetGuid();

	const TObjectPtr<UEdGraph> GraphA = GraphPropertyGraphs.FindChecked(GuidA);
	const TObjectPtr<UEdGraph> GraphB = GraphPropertyGraphs.FindChecked(GuidB);
	const TObjectPtr<USMGraphK2Node_PropertyNode_Base> NodeA = GraphPropertyNodes.FindChecked(GuidA);
	const TObjectPtr<USMGraphK2Node_PropertyNode_Base> NodeB = GraphPropertyNodes.FindChecked(GuidB);

	GraphPropertyGraphs.Add(GuidA, GraphB);
	GraphPropertyNodes.Add(GuidA, NodeB);
	GraphPropertyTemplates.Add(GuidA, Template);

	GraphPropertyGraphs.Add(GuidB, GraphA);
	GraphPropertyNodes.Add(GuidB, NodeA);
	GraphPropertyTemplates.Add(GuidB, Template);
}

void USMGraphNode_Base::NotifySwapPropertyGraphArrayElements(const FName& InPropertyName, int32 IndexA, int32 IndexB, USMNodeInstance* InNodeInstance)
{
	const FArraySwapData NewSwapData
	{
		InPropertyName,
		IndexA,
		IndexB
	};

	ArraySwapData.Add(InNodeInstance, NewSwapData);
}

void USMGraphNode_Base::HandleOnPropertyChangedEvent(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	// For when a value is officially set, such as through a CDO change propagating to instances.
	// Only look for ValueSet (optimization) and refresh property graphs matching this property.
	if (InPropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet &&
		InObject == NodeInstanceTemplate && !ISMEditorConstructionManager::Get().IsRunningConstructionScripts())
	{
		TArray<FGuid> GraphPropertyGuids;
		if (FStructProperty* StructProperty = LD::PropertyUtils::GetGraphPropertyFromProperty(InPropertyChangedEvent.MemberProperty))
		{
			// actual graph properties
			TArray<FSMGraphProperty_Base*> GraphProperties;

			USMUtils::BlueprintPropertyToNativeProperty(StructProperty, NodeInstanceTemplate, GraphProperties);

			for (int32 Idx = 0; Idx < GraphProperties.Num(); ++Idx)
			{
				GraphPropertyGuids.Add(LD::PropertyUtils::SetGraphPropertyFromProperty(*GraphProperties[Idx], StructProperty, NodeInstanceTemplate, Idx, false));
			}
		}
		else if (LD::PropertyUtils::IsPropertyExposedToGraphNode(InPropertyChangedEvent.Property))
		{
			// variable properties
			int32 ArraySize = 1;
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InPropertyChangedEvent.Property))
			{
				const FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<uint8>(NodeInstanceTemplate));
				ArraySize = Helper.Num();
			}

			for (int32 Idx = 0; Idx < ArraySize; ++Idx)
			{
				FSMGraphProperty_Base GraphProperty;
				GraphPropertyGuids.Add(LD::PropertyUtils::SetGraphPropertyFromProperty(GraphProperty,
					InPropertyChangedEvent.Property, NodeInstanceTemplate, Idx));
			}
		}

		for (const FGuid& Guid : GraphPropertyGuids)
		{
			RefreshPropertyByGuid(Guid, true, false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
