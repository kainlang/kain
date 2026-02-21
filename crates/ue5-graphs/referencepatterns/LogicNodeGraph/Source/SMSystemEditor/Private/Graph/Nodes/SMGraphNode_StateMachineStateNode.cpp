// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"

#include "Compilers/SMKismetCompiler.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMEditorStyle.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "FunctionNodes/SMGraphK2Node_StateReadNodes.h"
#include "Graph/Schema/SMGraphSchema.h"
#include "Graph/Schema/SMIntermediateGraphSchema.h"
#include "Graph/SMGraph.h"
#include "Graph/SMIntermediateGraph.h"
#include "Graph/SMPropertyGraph.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "SMInstance.h"

#include "Engine/Engine.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/App.h"
#include "ScopedTransaction.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SMGraphStateMachineStateNode"

USMGraphNode_StateMachineStateNode::USMGraphNode_StateMachineStateNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), DynamicClassVariable(NAME_None), bReuseCurrentState_DEPRECATED(false),
	  bReuseIfNotEndState_DEPRECATED(false),
	  bAllowIndependentTick(false), bCallTickOnManualUpdate(true), bReuseReference_DEPRECATED(false), bUseTemplate(false),
	  ReferencedInstanceTemplate(nullptr), ReferencedStateMachine(nullptr), bShouldUseIntermediateGraph(false),
	  bNeedsNewReference(false), bSwitchingGraphTypes(false)
{
	DesiredNodeName = "State Machine";
}

void USMGraphNode_StateMachineStateNode::PostLoad()
{
	Super::PostLoad();

	// Check not CDO
	FLinkerLoad* Linker = GetLinker();
	if (!IsTemplate() && Linker && Linker->IsPersistent() && Linker->IsLoading())
	{
		// Make sure the state machine default instance is setup.
		InitStateMachineReferenceTemplate(true);
	}
}

void USMGraphNode_StateMachineStateNode::PostPlacedNewNode()
{
	SetToCurrentVersion();
	
	CreateBoundGraph();
	UpdateEditState();

	if (bGenerateTemplateOnNodePlacement)
	{
		InitTemplate();
	}
}

void USMGraphNode_StateMachineStateNode::PostPasteNode()
{
	// Update the runtime node guid to prevent duplicate guid generation during runtime initialization.
	// Intermediate graphs take care of this on their own from their container node.
	if (BoundGraph && !HasIntermediateGraph())
	{
		FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(BoundGraph);
		check(RuntimeNode);
		RuntimeNode->GenerateNewNodeGuid();
	}
	
	Super::PostPasteNode();
	
	if (IsStateMachineReference())
	{
		if (!HasIntermediateGraph())
		{
			FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(BoundGraph);

			const UEdGraphSchema* Schema = BoundGraph->GetSchema();
			Schema->CreateDefaultNodesForGraph(*BoundGraph);
		}

		InitStateMachineReferenceTemplate();
	}

	UpdateEditState();
}

void USMGraphNode_StateMachineStateNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Likely happens after an import. We need to cancel out because if we are being pasted PostPasteNode hasn't been called yet.
	// Creating a BoundGraph at this stage would crash during graph rename. Fixes #124
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Unspecified && PropertyChangedEvent.Property == nullptr)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	// Reference force changes and bound graph checks.
	{
		// Check if reference was forcefully changed.
		bool bReferencedChanged = false;
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Redirected)
		{
			if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, ReferencedStateMachine))
			{
				bReferencedChanged = true;
			}
		}

		if (IsBoundGraphInvalid())
		{
			if (!BoundGraph)
			{
				CreateBoundGraph();
			}

			if (bReferencedChanged)
			{
				if (ReferencedStateMachine == nullptr)
				{
					bNeedsNewReference = true;
				}
			}
		}

		if (bReferencedChanged)
		{
			if (UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(this))
			{
				// Needed to fix GeneratedClassBy as null errors. Can't directly call compile at this stage either.
				// TODO: 4.24 has removed blueprint bytecode compile. Seems to work fine, tested with deleting / changing references.
				//FKismetEditorUtilities::RecompileBlueprintBytecode(Blueprint);
				FSMBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				CheckSetErrorMessages();
			}
		}
	}

	bool bStateChange = false;
	
	// Enable reference templates
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, bUseTemplate))
	{
		ConfigureInitialReferenceTemplate();
	}
	// Enable class templates
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateMachineStateNode, StateMachineClass))
	{
		InitTemplate();
		// Disable property graph refresh because InitTemplate handles it.
		bCreatePropertyGraphsOnPropertyChange = false;
		bStateChange = true;
	}
	else
	{
		bPostEditChangeConstructionRequiresFullRefresh = false;
	}

	UClass* OldNodeClass = GetNodeClass();
	SetNodeClassFromReferenceTemplate();
	if (OldNodeClass != GetNodeClass())
	{
		bStateChange = true;
	}
	
	UpdateEditState();

	Super::PostEditChangeProperty(PropertyChangedEvent);
	bCreatePropertyGraphsOnPropertyChange = true;
	bPostEditChangeConstructionRequiresFullRefresh = true;
	
	if (bStateChange && IsSafeToConditionallyCompile(PropertyChangedEvent.ChangeType))
	{
		FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this), false);
	}
}

void USMGraphNode_StateMachineStateNode::PostEditUndo()
{
	Super::PostEditUndo();
	if (BoundGraph)
	{
		BoundGraph->ClearFlags(RF_Transient);
	}
	if (ReferencedInstanceTemplate)
	{
		ReferencedInstanceTemplate->ClearFlags(RF_Transient);
	}
	UpdateEditState();
	InitStateMachineReferenceTemplate();
}

void USMGraphNode_StateMachineStateNode::DestroyNode()
{
	DestroyReferenceTemplate();
	Super::DestroyNode();
}

void USMGraphNode_StateMachineStateNode::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (IsBoundGraphInvalid() || bNeedsNewReference)
	{
		MessageLog.Error(TEXT("Nested State Machine node is invalid for @@. Was a state machine reference deleted or replaced?"), this);
	}

	if (!DynamicClassVariable.IsNone())
	{
		if (const UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			if (Blueprint->SkeletonGeneratedClass)
			{
				FProperty* Property = Blueprint->SkeletonGeneratedClass->FindPropertyByName(DynamicClassVariable);
				if (Property == nullptr)
				{
					MessageLog.Error(TEXT("Dynamic Class Variable was not found in the blueprint for node @@."), this);
				}
			}
		}
	}
}

UObject* USMGraphNode_StateMachineStateNode::GetJumpTargetForDoubleClick() const
{
	const bool bFavorLocalGraph = FSMBlueprintEditorUtils::GetEditorSettings()->ReferenceDoubleClickBehavior == ESMJumpToGraphBehavior::PreferLocalGraph;
	
	if (IsStateMachineReference() && !ShouldUseIntermediateGraph() && !bFavorLocalGraph)
	{
		return GetReferenceToJumpTo();
	}
	
	return Super::GetJumpTargetForDoubleClick();
}

void USMGraphNode_StateMachineStateNode::JumpToDefinition() const
{
	const bool bFavorLocalGraph = FSMBlueprintEditorUtils::GetEditorSettings()->ReferenceDoubleClickBehavior == ESMJumpToGraphBehavior::PreferLocalGraph;
	
	if (IsStateMachineReference() && (!IsUsingIntermediateGraph() || !bFavorLocalGraph) && GetReferenceToJumpTo())
	{
		JumpToReference();
		return;
	}

	Super::JumpToDefinition();
}

FSlateIcon USMGraphNode_StateMachineStateNode::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), TEXT("GraphEditor.StateMachine_16x"));
}

void USMGraphNode_StateMachineStateNode::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	SetNodeClassFromReferenceTemplate();
	Super::PreCompile(CompilerContext);

	if (bReuseReference_DEPRECATED)
	{
		bReuseReference_DEPRECATED = false;
		CompilerContext.MessageLog.Warning(TEXT("bReuseReference has been deprecated. It was previously set for node @@ and is now disabled."), this);
	}
}

void USMGraphNode_StateMachineStateNode::OnConvertToCurrentVersion(bool bOnlyOnLoad)
{
	Super::OnConvertToCurrentVersion(bOnlyOnLoad);

	if ((!IsTemplate() && GetLinker() && GetLinker()->IsPersistent() && GetLinker()->IsLoading()) || !bOnlyOnLoad)
	{
		// 2.7 requires intermediate graph created for references now.
		if (NeedsIntermediateGraph())
		{
			CreateBoundGraph();
			UpdateEditState();
		}
	}
}

void USMGraphNode_StateMachineStateNode::ImportDeprecatedProperties()
{
	Super::ImportDeprecatedProperties();

	if (USMStateMachineInstance* Instance = Cast<USMStateMachineInstance>(GetNodeTemplate()))
	{
		Instance->SetReuseIfNotEndState(bReuseIfNotEndState_DEPRECATED);
		Instance->SetReuseCurrentState(bReuseCurrentState_DEPRECATED);
	}
}

void USMGraphNode_StateMachineStateNode::CheckSetErrorMessages()
{
	Super::CheckSetErrorMessages();

	if (IsBoundGraphInvalid() || bNeedsNewReference)
	{
		ErrorMsg = "Invalid Reference";
		ErrorType = EMessageSeverity::Error;
		bHasCompilerMessage = true;
	}
	else
	{
		ErrorMsg.Empty();
		bHasCompilerMessage = false;
	}
}

void USMGraphNode_StateMachineStateNode::SetNodeClass(UClass* Class)
{
	StateMachineClass = Class;
	Super::SetNodeClass(Class);
}

const FSlateBrush* USMGraphNode_StateMachineStateNode::GetNodeIcon() const
{
	if (const FSlateBrush* Icon = Super::GetNodeIcon())
	{
		return Icon;
	}

	if (IsStateMachineReference())
	{
		return FSMEditorStyle::Get()->GetBrush(TEXT("SMGraph.StateMachineReference_16x"));
	}

	return FSMUnrealAppStyle::Get().GetBrush(TEXT("GraphEditor.StateMachine_16x"));
}

void USMGraphNode_StateMachineStateNode::GoToLocalGraph() const
{
	if (CanGoToLocalGraph())
	{
		if (const UEdGraph* Graph = GetBoundGraph())
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Graph);
		}
	}
}

bool USMGraphNode_StateMachineStateNode::CanGoToLocalGraph() const
{
	if (IsStateMachineReference())
	{
		return IsUsingIntermediateGraph();
	}

	return Super::CanGoToLocalGraph();
}

bool USMGraphNode_StateMachineStateNode::IsNodeFastPathEnabled() const
{
	if (IsStateMachineReference())
	{
		return false;
	}
	return Super::IsNodeFastPathEnabled();
}

void USMGraphNode_StateMachineStateNode::SetRuntimeDefaults(FSMState_Base& State) const
{
	Super::SetRuntimeDefaults(State);
	FSMStateMachine& StateMachine = static_cast<FSMStateMachine&>(State);
	StateMachine.SetReuseCurrentState(ShouldReuseCurrentState(), ShouldReuseIfNotEndState());
	StateMachine.bHasAdditionalLogic = ShouldUseIntermediateGraph();
	StateMachine.bAllowIndependentTick = bAllowIndependentTick;
	StateMachine.bCallReferenceTickOnManualUpdate = bCallTickOnManualUpdate;
	StateMachine.bWaitForEndState = ShouldWaitForEndState();
	StateMachine.SetDynamicReferenceVariableName(DynamicClassVariable);
}

USMGraph* USMGraphNode_StateMachineStateNode::GetBoundStateMachineGraph() const
{
	return Cast<USMGraph>(GetBoundGraph());
}

UObject* USMGraphNode_StateMachineStateNode::GetReferenceToJumpTo() const
{
	if (USMBlueprint* StateMachineReference = GetStateMachineReference())
	{
		// Only lookup the immediate graph of the reference blueprint.
		if (UObject* Target = FSMBlueprintEditorUtils::GetRootStateMachineGraph(Cast<UBlueprint>(StateMachineReference), false))
		{
			return Target;
		}
		// The graph doesn't exist, let's just return the top level one instead and leave it to the user to figure out.
		return FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(Cast<UBlueprint>(StateMachineReference));
	}

	return nullptr;
}

void USMGraphNode_StateMachineStateNode::JumpToReference() const
{
	if (const UObject* HyperlinkTarget = GetReferenceToJumpTo())
	{
		SetDebugObjectForReference();
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(HyperlinkTarget);
	}
}

void USMGraphNode_StateMachineStateNode::SetDebugObjectForReference() const
{
	if (UObject* HyperlinkTarget = GetReferenceToJumpTo())
	{
		// Automatically set the debug object to the correct instance of the referenced blueprint.
		if (UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			if (const USMInstance* CurrentDebugObject = Cast<USMInstance>(Blueprint->GetObjectBeingDebugged()))
			{
				CurrentDebugObject = CurrentDebugObject->GetPrimaryReferenceOwnerConst();
				UBlueprint* OtherBlueprint = FSMBlueprintEditorUtils::FindBlueprintForGraph(Cast<UEdGraph>(HyperlinkTarget));
				if (OtherBlueprint && Blueprint != OtherBlueprint)
				{
					if (const FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(BoundGraph))
					{
						// Find the correct runtime instance mapping to this node.
						if (const FSMNode_Base* RealRuntimeNode = CurrentDebugObject->GetDebugStateMachineConst().GetRuntimeNode(RuntimeNode->GetNodeGuid()))
						{
							// The real node has access to the full path guid.
							if (USMInstance* OtherInstance = CurrentDebugObject->GetReferencedInstanceByGuid(RealRuntimeNode->GetGuid()))
							{
								OtherBlueprint->SetObjectBeingDebugged(OtherInstance);
							}
						}
					}
				}
			}
		}
	}
}

bool USMGraphNode_StateMachineStateNode::ReferenceStateMachine(USMBlueprint* OtherStateMachine)
{
	const UBlueprint* ThisBlueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
	
	// Can't reference itself.
	if (ThisBlueprint == OtherStateMachine)
	{
		LDEDITOR_LOG_ERROR(TEXT("Cannot directly reference the same state machine."))
		if (FApp::CanEverRender())
		{
			FNotificationInfo Info(LOCTEXT("TriedToReferenceSelf", "Cannot directly reference the same state machine."));

			Info.bUseLargeFont = false;
			Info.ExpireDuration = 5.0f;

			const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}

		return false;
	}

	// Check to make sure the reference doesn't have any nodes that reference this state machine.
	const USMGraph* ReferencedRootStateMachineGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(OtherStateMachine, true);
	if (ReferencedRootStateMachineGraph != nullptr)
	{
		TArray<USMGraphNode_StateMachineStateNode*> FoundNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_StateMachineStateNode>(ReferencedRootStateMachineGraph, FoundNodes);
		for (const USMGraphNode_StateMachineStateNode* Node : FoundNodes)
		{
			if (Node->GetStateMachineReference() == ThisBlueprint)
			{
				LDEDITOR_LOG_ERROR(TEXT("Cannot reference a state machine which contains a reference to the caller."))
				if (FApp::CanEverRender())
				{
					FNotificationInfo Info(LOCTEXT("CircularReference", "Cannot reference a state machine which contains a reference to the caller."));

					Info.bUseLargeFont = false;
					Info.ExpireDuration = 5.0f;

					const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (Notification.IsValid())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
					}
				}

				return false;
			}
		}
	}

	ReferencedStateMachine = OtherStateMachine;
	bNeedsNewReference = ReferencedStateMachine == nullptr;

	const USMInstance* DefaultReference = (ReferencedStateMachine && ReferencedStateMachine->GetGeneratedClass()) ? CastChecked<USMInstance>(ReferencedStateMachine->GetGeneratedClass()->GetDefaultObject()) : nullptr;
	const TSubclassOf<USMStateMachineInstance> DefaultClass = DefaultReference ? DefaultReference->GetRootStateMachineNodeClass() : nullptr;
	
	bUseTemplate = bUseTemplate || (DefaultClass != nullptr && DefaultClass != USMStateMachineInstance::StaticClass()) || FSMBlueprintEditorUtils::GetProjectEditorSettings()->bEnableReferenceTemplatesByDefault;
	
	InitStateMachineReferenceTemplate();
	SetNodeClassFromReferenceTemplate();

	if (BoundGraph == nullptr || NeedsIntermediateGraph())
	{
		CreateBoundGraph();
	}
	else
	{
		// Look for references to this and update the nodes.
		TArray<USMGraphK2Node_StateReadNode_GetStateMachineReference*> References;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_StateReadNode_GetStateMachineReference>(BoundGraph, References);
		for (USMGraphK2Node_StateReadNode_GetStateMachineReference* Reference : References)
		{
			Reference->ReconstructNode();
		}
	}

	CheckSetErrorMessages();

	UpdateEditState();

	RefreshConnectedTransitions();

	return true;
}

void USMGraphNode_StateMachineStateNode::InitStateMachineReferenceTemplate(bool bInitialLoad)
{
	if (!ShouldUseTemplate() || ReferencedStateMachine == nullptr || (bInitialLoad && ReferencedInstanceTemplate &&
		ReferencedStateMachine && ReferencedInstanceTemplate->GetClass() == ReferencedStateMachine->GeneratedClass))
	{
		return;
	}

	Modify();

	const FString TemplateName = FString::Printf(TEXT("NODE_TEMPLATE_%s_%s_%s"), *GetName(), *ReferencedStateMachine->GeneratedClass->GetName(), *FGuid::NewGuid().ToString());
	USMInstance* NewTemplate = ReferencedStateMachine ? NewObject<USMInstance>(this, ReferencedStateMachine->GeneratedClass, *TemplateName,
		RF_ArchetypeObject | RF_Transactional | RF_Public) : nullptr;

	if (ReferencedInstanceTemplate)
	{
		if (NewTemplate)
		{
			UEngine::CopyPropertiesForUnrelatedObjects(ReferencedInstanceTemplate, NewTemplate);
		}

		// Original template isn't needed any more.
		DestroyReferenceTemplate();
	}

	ReferencedInstanceTemplate = NewTemplate;
}

void USMGraphNode_StateMachineStateNode::DestroyReferenceTemplate()
{
	if (ReferencedInstanceTemplate)
	{
		ReferencedInstanceTemplate->Modify();
		FSMBlueprintEditorUtils::TrashObject(ReferencedInstanceTemplate);
		ReferencedInstanceTemplate = nullptr;
	}
}

void USMGraphNode_StateMachineStateNode::SetUseIntermediateGraph(bool bValue)
{
	if (bShouldUseIntermediateGraph == bValue)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "UseIntermediateGraph", "Use Intermediate Graph"));
	Modify();

	bShouldUseIntermediateGraph = bValue;

	if (NeedsIntermediateGraph())
	{
		CreateBoundGraph();
	}

	UpdateEditState();

	FSMBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this));
}

void USMGraphNode_StateMachineStateNode::CreateBoundGraph()
{
	UEdGraph* ParentGraph = GetGraph();
	check(ParentGraph);
	
	FName GraphName = NAME_None;
	FSMNode_Base* OriginalStateMachine = nullptr;

	Modify();
	ParentGraph->Modify();
	
	TArray<TObjectPtr<UEdGraph>> BoundGraphPropertySubGraphs;
	if (BoundGraph != nullptr)
	{
		for (const TObjectPtr<UEdGraph>& SubGraph : BoundGraph->SubGraphs)
		{
			if (SubGraph->IsA<USMPropertyGraph>())
			{
				BoundGraphPropertySubGraphs.Add(SubGraph);
			}
		}

		BoundGraph->Modify();

		if (HasIntermediateGraph())
		{
			ParentGraph->SubGraphs.Remove(BoundGraph);
		}
		else
		{
			OriginalStateMachine = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(BoundGraph);
			GraphName = BoundGraph->GetFName();

			// bSwitchingGraphTypes signals to the schema not to remove this node on graph deletion.
			bSwitchingGraphTypes = true;
			if (BoundGraph->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
			{
				ParentGraph->SubGraphs.Remove(BoundGraph);
				FSMBlueprintEditorUtils::TrashObject(BoundGraph);
			}
			else
			{
				FSMBlueprintEditorUtils::RemoveGraph(FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this), BoundGraph);
			}
			bSwitchingGraphTypes = false;
		}
		BoundGraph = nullptr;
	}

	if (NeedsIntermediateGraph())
	{
		BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
			this,
			GraphName,
			USMIntermediateGraph::StaticClass(),
			USMIntermediateGraphSchema::StaticClass());
	}
	else
	{
		BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
			this,
			GraphName,
			USMGraph::StaticClass(),
			USMGraphSchema::StaticClass());
	}
	check(BoundGraph);

	// Name the graph if it wasn't set properly already.
	if (GraphName.IsNone() || BoundGraph->GetFName() != GraphName)
	{
		const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
		FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, DesiredNodeName);
	}

	// Initialize the state machine graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Set original state machine guid if it exists.
	if (OriginalStateMachine)
	{
		FSMBlueprintEditorUtils::UpdateRuntimeNodeForGraph(OriginalStateMachine, BoundGraph);
	}

	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}

	// Move any children graphs over (property graphs)
	BoundGraph->Modify();
	BoundGraph->SubGraphs = MoveTemp(BoundGraphPropertySubGraphs);
	for (const TObjectPtr<UEdGraph>& Subgraph : BoundGraph->SubGraphs)
	{
		Subgraph->Rename(nullptr, BoundGraph, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	}
}

void USMGraphNode_StateMachineStateNode::UpdateEditState()
{
	if (BoundGraph != nullptr)
	{
		BoundGraph->bEditable = !IsStateMachineReference() || (IsUsingIntermediateGraph() && !bNeedsNewReference);
	}
}

bool USMGraphNode_StateMachineStateNode::IsBoundGraphInvalid() const
{
	return !BoundGraph || (!IsStateMachineReference() && HasIntermediateGraph());
}

bool USMGraphNode_StateMachineStateNode::NeedsIntermediateGraph() const
{
	return IsStateMachineReference() && !HasIntermediateGraph();
}

bool USMGraphNode_StateMachineStateNode::HasIntermediateGraph() const
{
	return BoundGraph && BoundGraph->IsA<USMIntermediateGraph>();
}

bool USMGraphNode_StateMachineStateNode::IsUsingIntermediateGraph() const
{
	return ShouldUseIntermediateGraph() && HasIntermediateGraph();
}

bool USMGraphNode_StateMachineStateNode::ShouldUseIntermediateGraph() const
{
	return IsStateMachineReference() && bShouldUseIntermediateGraph;
}

bool USMGraphNode_StateMachineStateNode::ShouldUseTemplate() const
{
	return bUseTemplate;
}

bool USMGraphNode_StateMachineStateNode::ShouldReuseCurrentState() const
{
	if (USMStateMachineInstance* Instance = Cast<USMStateMachineInstance>(GetNodeTemplate()))
	{
		return Instance->GetReuseCurrentState();
	}

	return false;
}

bool USMGraphNode_StateMachineStateNode::ShouldReuseIfNotEndState() const
{
	if (USMStateMachineInstance* Instance = Cast<USMStateMachineInstance>(GetNodeTemplate()))
	{
		return Instance->GetReuseIfNotEndState();
	}

	return false;
}

bool USMGraphNode_StateMachineStateNode::ShouldWaitForEndState() const
{
	if (USMStateMachineInstance* Instance = Cast<USMStateMachineInstance>(GetNodeTemplate()))
	{
		return Instance->GetWaitForEndState();
	}

	return false;
}

bool USMGraphNode_StateMachineStateNode::IsSwitchingGraphTypes() const
{
	return bSwitchingGraphTypes;
}

FLinearColor USMGraphNode_StateMachineStateNode::Internal_GetBackgroundColor() const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	const FLinearColor ColorModifier(0.5f, 0.9f, 0.9f, IsStateMachineReference() ? 0.25f : 0.7f);

	if (IsEndState())
	{
		return Settings->EndStateColor * ColorModifier;
	}

	const FLinearColor DefaultColor = Settings->StateMachineDefaultColor;

	// No input -- node unreachable.
	if (!HasInputConnections())
	{
		return DefaultColor * ColorModifier;
	}

	// State is active
	if (HasLogicStates())
	{
		return Settings->StateMachineWithLogicColor * ColorModifier;
	}

	return DefaultColor * ColorModifier;
}

bool USMGraphNode_StateMachineStateNode::HasLogicStates() const
{
	if (bNeedsNewReference)
	{
		return false;
	}

	USMGraph* Graph = IsStateMachineReference() ? FSMBlueprintEditorUtils::GetRootStateMachineGraph(ReferencedStateMachine, true) : Cast<USMGraph>(BoundGraph);
	if (Graph == nullptr)
	{
		return false;
	}

	return Graph->HasAnyLogicConnections();
}

void USMGraphNode_StateMachineStateNode::ConfigureInitialReferenceTemplate()
{
	if (bUseTemplate)
	{
		bReuseReference_DEPRECATED = false;
		InitStateMachineReferenceTemplate();
	}
	else
	{
		DestroyReferenceTemplate();
		SetNodeClass(nullptr);
	}
}

void USMGraphNode_StateMachineStateNode::SetNodeClassFromReferenceTemplate()
{
	if (!IsStateMachineReference())
	{
		return;
	}
	
	UClass* NewClass = ReferencedInstanceTemplate ? ReferencedInstanceTemplate->GetRootStateMachineNodeClass() : nullptr;
	if (NewClass == GetNodeClass())
	{
		return;
	}

	if (NewClass == nullptr)
	{
		NewClass = GetDefaultNodeClass();
	}

	StateMachineClass = NewClass;
	
	if (bUseTemplate && (!NodeInstanceTemplate || NodeInstanceTemplate->GetClass() != NewClass))
	{
		// Limit initializing a template unless required. Doing this with a default/null class on PreCompile
		// will throw nativization errors during packaging.
		InitTemplate();	
	}
}

#undef LOCTEXT_NAMESPACE
