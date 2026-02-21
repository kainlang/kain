// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Schema/SMGraphSchema.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Commands/SMEditorCommands.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Construction/ISMEditorConstructionManager.h"
#include "Graph/ConnectionDrawing/SMGraphConnectionDrawingPolicy.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SlateNodes/SGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineParentNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "ISMSystemEditorModule.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMNodeRuleUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "ContentBrowserModule.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorActions.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "SMGraphSchema"

template<typename T>
TSharedPtr<T> AddNewStateNodeAction(FGraphContextMenuBuilder& ContextMenuBuilder, const FText& Category, const FText& MenuDesc, const FText& Tooltip, const int32 Grouping = 0)
{
	TSharedPtr<T> NewStateNode(new T(Category, MenuDesc, Tooltip, Grouping));
	ContextMenuBuilder.AddAction(NewStateNode);
	return NewStateNode;
}

/** Action to reference a state machine */
struct FSMGraphSchemaAction_NewStateMachineReferenceNode : FSMGraphSchemaAction_NewNode
{
	FSMGraphSchemaAction_NewStateMachineReferenceNode()
	{
		bDontOverrideDefaultClass = true;	
	}

	FSMGraphSchemaAction_NewStateMachineReferenceNode(const FText& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, const int32 InGrouping)
		: FSMGraphSchemaAction_NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{
		bDontOverrideDefaultClass = true;
	}

	// FEdGraphSchemaAction
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// ~FEdGraphSchemaAction
};

/** Action to create new comment */
struct FSMGraphSchemaAction_NewComment : FEdGraphSchemaAction
{
	FSMGraphSchemaAction_NewComment()
	{}

	FSMGraphSchemaAction_NewComment(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{}

	// FEdGraphSchemaAction
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// ~FEdGraphSchemaAction
};


UEdGraphNode* FSMGraphSchemaAction_NewComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /*= true*/)
{
	// Add menu item for creating comment boxes
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	FVector2D SpawnLocation = Location;

	FSlateRect Bounds;
	if (Blueprint != nullptr && FKismetEditorUtilities::GetBoundsForSelectedNodes(Blueprint, Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation);
}

UEdGraphNode* FSMGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /*= true*/)
{
	UEdGraphNode* ResultNode = nullptr;

	if (GraphNodeTemplate != nullptr)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "AddNode", "Add Node"));
		ParentGraph->Modify();
		if (FromPin != nullptr)
		{
			FromPin->Modify();
		}

		// When called from a context menu an owner of temporaries is provided, but otherwise one generally isn't.
		// We need to be parented to a UEdGraph for proper handling through UE, especially on undo operations when
		// not having a graph outer can trigger an ensure.
		if (GraphNodeTemplate->GetOuter() == GetTransientPackage())
		{
			OwnerOfTemporaries = NewObject<UEdGraph>(GetTransientPackage());
			GraphNodeTemplate->Rename(nullptr, OwnerOfTemporaries, REN_DontCreateRedirectors);
		}

		// Setup transaction history early so undoing the creation of a transition will properly place the template
		// back on the owner of temporaries. Otherwise stale data will persist during a copy unless GC occurs first.
		GraphNodeTemplate->SetFlags(RF_Transactional);
		GraphNodeTemplate->Modify();

		GraphNodeTemplate->Rename(nullptr, ParentGraph, REN_DontCreateRedirectors);
		ParentGraph->AddNode(GraphNodeTemplate, true, bSelectNewNode);

		GraphNodeTemplate->CreateNewGuid();

		UClass* NodeClass = NodeSoftClassPath.LoadSynchronous();

		// Optimization to not double generate a template.
		if (USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(GraphNodeTemplate))
		{
			GraphNode->bGenerateTemplateOnNodePlacement = NodeClass == nullptr;
		}

		if (!bDontCallPostPlacedNode)
		{
			GraphNodeTemplate->PostPlacedNewNode();
		}

		GraphNodeTemplate->AllocateDefaultPins();
		
		GraphNodeTemplate->NodePosX = Location.X;
		GraphNodeTemplate->NodePosY = Location.Y;

		ResultNode = GraphNodeTemplate;

		if (NodeClass == nullptr && !bDontOverrideDefaultClass)
		{
			// Check for node defaults set under project settings.
			// Custom rules will still override these.
			
			const USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
			if (Cast<USMGraphNode_StateNode>(GraphNodeTemplate))
			{
				UClass* DefaultClass = Settings->DefaultStateClass.LoadSynchronous();
				if (!FSMNodeClassRule::IsBaseClass(DefaultClass))
				{
					NodeClass = DefaultClass;
				}
			}
			else if (USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GraphNodeTemplate))
			{
				if (!StateMachineNode->IsA<USMGraphNode_StateMachineParentNode>() /* bDontOverrideDefaultClass set for references */)
				{
					UClass* DefaultClass = Settings->DefaultStateMachineClass.LoadSynchronous();
					if (!FSMNodeClassRule::IsBaseClass(DefaultClass))
					{
						NodeClass = DefaultClass;
					}
				}
			}
			else if (Cast<USMGraphNode_ConduitNode>(GraphNodeTemplate))
			{
				UClass* DefaultClass = Settings->DefaultConduitClass.LoadSynchronous();
				if (!FSMNodeClassRule::IsBaseClass(DefaultClass))
				{
					NodeClass = DefaultClass;
				}
			}
			else if (Cast<USMGraphNode_TransitionEdge>(GraphNodeTemplate))
			{
				UClass* DefaultClass = Settings->DefaultTransitionClass.LoadSynchronous();
				if (!FSMNodeClassRule::IsBaseClass(DefaultClass))
				{
					NodeClass = DefaultClass;
				}
			}
		}
		
		// Set the actual node class if one is set.
		if (NodeClass)
		{
			if (USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(GraphNodeTemplate))
			{
				GraphNode->SetNodeClass(NodeClass);
				GraphNode->CreateGraphPropertyGraphs();

				// If the instance has a custom name supplied use that.
				USMGraphSchema::RenameNodeFromTemplateData(GraphNode);
			}
		}

		// Check for transition that needs to be set from the previous state to this one.
		if (FromPin != nullptr)
		{
			if (const USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(GraphNodeTemplate))
			{
				if (USMGraphNode_TransitionEdge* TransitionNode = StateNode->GetPreviousTransition())
				{
					if (const USMGraphNode_StateNodeBase* FromNode = TransitionNode->GetFromState())
					{
						const UClass* StateMachineClass = FSMBlueprintEditorUtils::GetStateMachineClassFromGraph(ParentGraph);
						USMGraphSchema::SetTransitionClassFromRules(TransitionNode, FromNode ? FromNode->GetNodeClass() : nullptr,
							NodeClass, StateMachineClass);
					}
				}
			}

			GraphNodeTemplate->AutowireNewNode(FromPin);
		}
		ParentGraph->NotifyGraphChanged();
		
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	return ResultNode;
}

void FSMGraphSchemaAction_NewNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(GraphNodeTemplate);
	if (OwnerOfTemporaries)
	{
		Collector.AddReferencedObject(OwnerOfTemporaries);
	}
}

UEdGraphNode* FSMGraphSchemaAction_NewStateMachineReferenceNode::PerformAction(UEdGraph* ParentGraph,
	UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FOpenAssetDialogConfig SelectAssetConfig;
	SelectAssetConfig.DialogTitleOverride = LOCTEXT("ChooseStateMachinePath", "Choose a state machine");
	SelectAssetConfig.bAllowMultipleSelection = false;
	SelectAssetConfig.AssetClassNames.Add(USMBlueprint::StaticClass()->GetClassPathName());

	// Set the path to the current folder.
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph))
	{
		UObject* AssetOuter = Blueprint->GetOuter();
		UPackage* AssetPackage = AssetOuter->GetOutermost();

		// Remove the file name and go directly to the folder.
		FString AssetPath = AssetPackage->GetName();
		const int LastSlashPos = AssetPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		SelectAssetConfig.DefaultPath = AssetPath.Left(LastSlashPos);
	}

	TArray<FAssetData> AssetData = ContentBrowserModule.Get().CreateModalOpenAssetDialog(SelectAssetConfig);
	if (AssetData.Num() == 1)
	{
		if (USMBlueprint *ReferencedBlueprint = Cast<USMBlueprint>(AssetData[0].GetAsset()))
		{
			if (!ReferencedBlueprint->HasAnyFlags(RF_Transient) && IsValid(ReferencedBlueprint))
			{
				// Create the new node.
				if (USMGraphNode_StateMachineStateNode* NewStateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(FSMGraphSchemaAction_NewNode::PerformAction(ParentGraph, FromPin, Location, bSelectNewNode)))
				{
					// Rename the graph to match reference.
					TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(NewStateMachineNode);
					FBlueprintEditorUtils::RenameGraphWithSuggestion(NewStateMachineNode->GetBoundGraph(), NameValidator, ReferencedBlueprint->GetFName().ToString());

					// Convert to a reference only if valid, otherwise abort out.
					if (!NewStateMachineNode->ReferenceStateMachine(ReferencedBlueprint))
					{
						UBlueprint* ThisBlueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(NewStateMachineNode);
						FBlueprintEditorUtils::RemoveNode(ThisBlueprint, NewStateMachineNode);

						return nullptr;
					}

					return NewStateMachineNode;
				}
			}
		}
	}

	return nullptr;
}

USMGraphSchema::USMGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the result node
	FGraphNodeCreator<USMGraphNode_StateMachineEntryNode> NodeCreator(Graph);
	USMGraphNode_StateMachineEntryNode* EntryNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	if (USMGraph* StateMachineGraph = CastChecked<USMGraph>(&Graph))
	{
		StateMachineGraph->EntryNode = EntryNode;
	}
}

EGraphType USMGraphSchema::GetGraphType(const UEdGraph* TestEdGraph) const
{
	return GT_StateMachine;
}

void USMGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	Super::GetGraphContextActions(ContextMenuBuilder);

	// Vertical order for which groups show up in the context menu.
	const int32 BaseGrouping = 2;
	const int32 SpecialGrouping = 1;
	const int32 UserGrouping = 0;

	UClass* StateMachineClass = FSMBlueprintEditorUtils::GetStateMachineClassFromGraph(ContextMenuBuilder.CurrentGraph);
	const USMStateMachineInstance* StateMachineDefault = StateMachineClass ? Cast<USMStateMachineInstance>(StateMachineClass->GetDefaultObject()) : nullptr;
	
	// Validate which nodes can be placed.
	bool bBaseStatesAllowed = true;
	bool bBaseStateMachinesAllowed = true;
	bool bReferencesAllowed = true;
	bool bParentsAllowed = true;
	bool bConduitsAllowed = true;
	
	TSoftClassPtr<USMStateMachineInstance> BaseStateMachineClass = nullptr;
	if (StateMachineDefault)
	{
		const FSMStateMachineNodePlacementValidator& Rules = StateMachineDefault->GetAllowedNodes();
		bBaseStatesAllowed = Rules.IsStateAllowed(USMStateInstance::StaticClass());
		bBaseStateMachinesAllowed = Rules.bAllowSubStateMachines && Rules.IsStateAllowed(USMStateMachineInstance::StaticClass());
		BaseStateMachineClass = Rules.bAllowSubStateMachines ? Rules.DefaultSubStateMachineClass : nullptr;
		bReferencesAllowed = Rules.bAllowReferences;
		bParentsAllowed = Rules.bAllowParents;
		bConduitsAllowed = Rules.IsStateAllowed(USMConduitInstance::StaticClass());
	}
	
	// Add new state node
	if (bBaseStatesAllowed)
	{
		const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction =
			AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddState", "Add State..."), LOCTEXT("AddStateTooltip", "A new state which contains entry points for logic execution."), BaseGrouping);
		NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_StateNode>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	// Add new conduit node
	if (bConduitsAllowed)
	{
		const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction =
			AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddConduit", "Add Conduit..."), LOCTEXT("AddConduitTooltip", "A new conduit for branching to different states."), BaseGrouping);
		NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_ConduitNode>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	// Add new state machine node
	if (bBaseStateMachinesAllowed)
	{
		const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction =
			AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddStateMachine", "Add State Machine..."), LOCTEXT("AddStateMachineTooltip", "A new state machine."), BaseGrouping);
		NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_StateMachineStateNode>(ContextMenuBuilder.OwnerOfTemporaries);
		NewNodeAction->NodeSoftClassPath = BaseStateMachineClass;
	}

	// Add new parent state machine node
	if (bParentsAllowed)
	{
		UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ContextMenuBuilder.CurrentGraph);

		TArray<USMBlueprintGeneratedClass*> ParentClasses;
		if (FSMBlueprintEditorUtils::TryGetParentClasses(OwnerBlueprint, ParentClasses))
		{
			const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction =
				AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddParentStateMachine", "Add State Machine Parent..."), LOCTEXT("AddParentStateMachineTooltip", "A new state machine from the parent graph."), BaseGrouping);
			NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_StateMachineParentNode>(ContextMenuBuilder.OwnerOfTemporaries);
		}
	}

	// Add new state machine reference node
	if (bReferencesAllowed)
	{
		const TSharedPtr<FSMGraphSchemaAction_NewStateMachineReferenceNode> NewNodeAction =
			AddNewStateNodeAction<FSMGraphSchemaAction_NewStateMachineReferenceNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddStateMachineReference", "Add State Machine Reference..."), LOCTEXT("AddStateMachineReferenceTooltip", "Link to an existing state machine blueprint."), BaseGrouping);
		NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_StateMachineStateNode>(ContextMenuBuilder.OwnerOfTemporaries);
	}
	
	// Entry point (only if doesn't already exist) Shouldn't need this since the entry point can't be removed.
	{
		bool bHasEntry = false;
		for (auto NodeIt = ContextMenuBuilder.CurrentGraph->Nodes.CreateConstIterator(); NodeIt; ++NodeIt)
		{
			UEdGraphNode* Node = *NodeIt;
			if (const USMGraphNode_StateMachineEntryNode* StateNode = Cast<USMGraphNode_StateMachineEntryNode>(Node))
			{
				bHasEntry = true;
				break;
			}
		}

		if (!bHasEntry)
		{
			const TSharedPtr<FSMGraphSchemaAction_NewNode> Action =
				AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddEntryPoint", "Add Entry Point..."), LOCTEXT("AddEntryPointTooltip", "Define State Machine's Entry Point."), BaseGrouping);
			Action->GraphNodeTemplate = NewObject<USMGraphNode_StateMachineEntryNode>(ContextMenuBuilder.OwnerOfTemporaries);
		}
	}

	if (!ContextMenuBuilder.FromPin)
	{
		// Add comment
		{
			const UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ContextMenuBuilder.CurrentGraph);
			const bool bIsManyNodesSelected = (FKismetEditorUtilities::GetNumberOfSelectedNodes(OwnerBlueprint) > 0);
			const FText MenuDescription = bIsManyNodesSelected ? LOCTEXT("CreateCommentSelection", "Create Comment from Selection...") : LOCTEXT("AddComment", "Create Comment...");
			const FText ToolTip = LOCTEXT("CreateCommentSelectionTooltip", "Create a resizeable comment box around selected nodes.");

			const TSharedPtr<FSMGraphSchemaAction_NewComment> NewComment(new FSMGraphSchemaAction_NewComment(FText::GetEmpty(), MenuDescription, ToolTip, SpecialGrouping));
			ContextMenuBuilder.AddAction(NewComment);
		}
		
		// Add any state node
		{
			const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction =
				AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddAnyState", "Add Any State..."), LOCTEXT("AddAnyStateTooltip", "A special state node that represents any other state within this FSM."), SpecialGrouping);
			NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_AnyStateNode>(ContextMenuBuilder.OwnerOfTemporaries);
		}
	}

	// Add link state node
	{
		const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction =
			AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddLinkState", "Add Link State..."), LOCTEXT("AddLinkStateTooltip", "A special state node that represents another state within this FSM."), SpecialGrouping);
		NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_LinkStateNode>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	// Add reroute node
	if (!ContextMenuBuilder.FromPin || !ContextMenuBuilder.FromPin->GetOwningNode()->IsA<USMGraphNode_StateMachineEntryNode>())
	{
		const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction =
			AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddRerouteNode", "Add Reroute Node..."), LOCTEXT("AddRerouteNodeTooltip", "Reroute the transition connection to a different direction. For cosmetic use only."), SpecialGrouping);
		NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_RerouteNode>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	// Custom node actions
	{
		TArray<TSoftClassPtr<USMStateInstance_Base>> NodeClasses;
		FSMNodeInstanceUtils::GetAllNodeSubClasses(USMStateInstance_Base::StaticClass(), NodeClasses);

		UClass* FromClass = nullptr;
		if (ContextMenuBuilder.FromPin)
		{
			// Find the from state.
			if (const USMGraphNode_Base* BaseNode = Cast<USMGraphNode_Base>(ContextMenuBuilder.FromPin->GetOwningNode()))
			{
				FromClass = BaseNode->GetNodeClass();
				if (const USMGraphNode_RerouteNode* RerouteNode = Cast<USMGraphNode_RerouteNode>(BaseNode))
				{
					if (USMGraphNode_TransitionEdge* PrimaryTransition = RerouteNode->GetPrimaryTransition())
					{
						if (USMGraphNode_StateNodeBase* FromState = PrimaryTransition->GetFromState())
						{
							FromClass = FromState->GetNodeClass();
						}
					}
				}
			}
		}

		for (const TSoftClassPtr<USMStateInstance_Base>& NodeClass : NodeClasses)
		{
			if (FSMNodeClassRule::IsBaseClass(NodeClass))
			{
				continue;
			}
			
			if (!FSMNodeInstanceUtils::IsStateRegisteredWithContextMenu(NodeClass))
			{
				continue;
			}

			// Validate allowed placement in state machine.
			if (StateMachineDefault && !StateMachineDefault->GetAllowedNodes().IsStateAllowed(NodeClass))
			{
				continue;
			}

			const LD::NodeRuleUtils::FExtractedNodeConnectionValidator NodeConnectionValidator = LD::NodeRuleUtils::GetConnectionValidators(NodeClass);
			if (!ensure(NodeConnectionValidator.IsValid()))
			{
				continue;
			}

			// Validate connection.
			if (!NodeConnectionValidator.AsStateConnectionValidator().IsInboundConnectionValid(FromClass, StateMachineClass)
				&& FSMNodeInstanceUtils::ShouldHideFromContextMenuIfRulesFail(NodeClass))
			{
				continue;
			}

			FText MenuDescription = FText::FromString(FString::Printf(TEXT("Add %s..."), *FSMNodeInstanceUtils::GetNodeDisplayName(NodeClass)));

			const TSharedPtr<FSMGraphSchemaAction_NewNode> NewNodeAction = AddNewStateNodeAction<FSMGraphSchemaAction_NewNode>(
				ContextMenuBuilder,
				FSMNodeInstanceUtils::GetNodeCategory(NodeClass),
				MenuDescription,
				FSMNodeInstanceUtils::GetNodeDescriptionText(NodeClass),
				UserGrouping);
			if (NodeConnectionValidator.NativeClass->IsChildOf(USMStateMachineInstance::StaticClass()))
			{
				NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_StateMachineStateNode>(ContextMenuBuilder.OwnerOfTemporaries);
			}
			else if (NodeConnectionValidator.NativeClass->IsChildOf(USMConduitInstance::StaticClass()))
			{
				NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_ConduitNode>(ContextMenuBuilder.OwnerOfTemporaries);
			}
			else
			{
				NewNodeAction->GraphNodeTemplate = NewObject<USMGraphNode_StateNode>(ContextMenuBuilder.OwnerOfTemporaries);
			}
			
			NewNodeAction->NodeSoftClassPath = NodeClass;
		}
	}
}

void USMGraphSchema::GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	const UEdGraph* CurrentGraph = Context->Graph;
	const UEdGraphNode* InGraphNode = Context->Node;
	const UEdGraphPin* InGraphPin = Context->Pin;
	const bool bIsDebugging = Context->bIsDebugging;
	
	if (InGraphNode)
	{
		if (FSMBlueprintEditor* Editor = FSMBlueprintEditorUtils::GetStateMachineEditor(InGraphNode))
		{
			Editor->SelectedNodeForContext = MakeWeakObjectPtr<UEdGraphNode>(const_cast<UEdGraphNode*>(InGraphNode));
		}
		
		FToolMenuSection& Section = Menu->AddSection("SMGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		{
			Section.AddMenuEntry(FGenericCommands::Get().Delete);
			Section.AddMenuEntry(FGenericCommands::Get().Cut);
			Section.AddMenuEntry(FGenericCommands::Get().Copy);
			Section.AddMenuEntry(FGenericCommands::Get().Duplicate);

			if (bool bCanRename = InGraphNode->bCanRenameNode)
			{
				if (const USMGraphNode_Base* Node = Cast<USMGraphNode_Base>(InGraphNode))
				{
					if (const USMStateInstance_Base* NodeInstance = Cast<USMStateInstance_Base>(Node->GetNodeTemplate()))
					{
						bCanRename = NodeInstance->ShouldDisplayNameWidget() && !NodeInstance->ShouldUseDisplayNameOnly();
					}
					else if (Node->IsA<USMGraphNode_TransitionEdge>() || Node->IsA<USMGraphNode_RerouteNode>())
					{
						// These nodes technically can be renamed but we don't want to do so anywhere other than the details panel.
						bCanRename = false;
					}
				}

				if (bCanRename)
				{
					Section.AddMenuEntry(FGenericCommands::Get().Rename);
				}
			}

			if (!bIsDebugging)
			{
				FToolMenuSection& StateSection = Menu->AddSection("SMGraphSchemaStateActions", LOCTEXT("StateActionsMenuHeader", "State Actions"));
				
				if (const USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(InGraphNode))
				{
					StateSection.AddMenuEntry(FSMEditorCommands::Get().CutAndMergeStates);
					StateSection.AddMenuEntry(FSMEditorCommands::Get().CopyAndMergeStates);
				}
				
				StateSection.AddMenuEntry(FSMEditorCommands::Get().CollapseToStateMachine);

				if (CanReplaceNode(InGraphNode))
				{
					StateSection.AddSubMenu(
						NAME_None,
						LOCTEXT("NodeActionsReplaceWith", "Replace With..."),
						LOCTEXT("NodeActionsReplaceWithToolTip", "Perform a destructive replacement of the selected node"),
						FNewMenuDelegate::CreateUObject(this, &USMGraphSchema::GetReplaceWithMenuActions, InGraphNode));
				}

				if (const USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(InGraphNode))
				{
					FToolMenuSection& StateMachineSection = Menu->AddSection("SMGraphSchemaReferenceActions", LOCTEXT("ReferenceActionsMenuHeader", "Reference Actions"));
					{
						if (StateMachineNode->IsStateMachineReference())
						{
							StateMachineSection.AddMenuEntry(FSMEditorCommands::Get().JumpToStateMachineReference);
							StateMachineSection.AddMenuEntry(FSMEditorCommands::Get().ChangeStateMachineReference);

							if (StateMachineNode->ShouldUseIntermediateGraph())
							{
								StateMachineSection.AddMenuEntry(FSMEditorCommands::Get().DisableIntermediateGraph);
							}
							else
							{
								StateMachineSection.AddMenuEntry(FSMEditorCommands::Get().EnableIntermediateGraph);
							}
						}
						else
						{
							StateMachineSection.AddMenuEntry(FSMEditorCommands::Get().ConvertToStateMachineReference);
						}
					}
				}
			}
			else
			{
				// Allow some state machine actions while debugging
				if (const USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(InGraphNode))
				{
					FToolMenuSection& ReferenceSection = Menu->AddSection("SMGraphSchemaReferenceActions", LOCTEXT("ReferenceActionsMenuHeader", "Reference Actions"));
					{
						if (StateMachineNode->IsStateMachineReference())
						{
							ReferenceSection.AddMenuEntry(FSMEditorCommands::Get().JumpToStateMachineReference);
						}
					}
				}
			}
		}

		FToolMenuSection& GraphSection = Menu->AddSection("SMGraphSchemaGraphActions", LOCTEXT("GraphActionsMenuHeader", "Graph Actions"));
		{
			GraphSection.AddMenuEntry(FSMEditorCommands::Get().GoToGraph);
			GraphSection.AddMenuEntry(FSMEditorCommands::Get().GoToNodeBlueprint);
			if (const USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(InGraphNode))
			{
				GraphSection.AddMenuEntry(FSMEditorCommands::Get().GoToPropertyBlueprint);
			}
			if (const USMGraphNode_TransitionEdge* TransitionEdge = Cast<USMGraphNode_TransitionEdge>(InGraphNode))
			{
				const USMGraphNode_TransitionEdge* TransitionEdgeNonConst = const_cast<USMGraphNode_TransitionEdge*>(TransitionEdge);
				TransitionEdgeNonConst->ClearCachedHoveredStackTemplate();
				if (TransitionEdge->GetHoveredStackTemplate() != nullptr)
				{
					GraphSection.AddMenuEntry(FSMEditorCommands::Get().GoToTransitionStackBlueprint);
				}
			}
		}

		FToolMenuSection& LinkSection = Menu->AddSection("SMGraphSchemaLinkActions", LOCTEXT("LinkActionsMenuHeader", "Link Actions"));
		{
			LinkSection.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
			if (!bIsDebugging && InGraphNode->IsA<USMGraphNode_StateNodeBase>() && !InGraphNode->IsA<USMGraphNode_RerouteNode>())
			{
				LinkSection.AddMenuEntry(FSMEditorCommands::Get().CreateSelfTransition);
			}
		}

		if (const USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(InGraphNode))
		{
			// Check for custom graph property context menus.
			if (const USMGraphK2Node_PropertyNode_Base* PropertyNode = GraphNode->GetPropertyNodeUnderMouse())
			{
				PropertyNode->GetContextMenuActionsForOwningNode(CurrentGraph, InGraphNode, InGraphPin, Menu, bIsDebugging);
			}
		}

		FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME)
		.GetExtendGraphNodeContextMenu().Broadcast(Menu, Context);
	}

	Super::GetContextMenuActions(Menu, Context);
}

const FPinConnectionResponse USMGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		USMGraphNode_StateNodeBase* State = Cast<USMGraphNode_StateNodeBase>(PinA->GetOwningNode());

		// Only connect to same states when using context menu.
		if (!State || !State->bCanTransitionToSelf)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorSameNode", "Use the context menu to create self-transitions."));
		}
	}

	if (PinB->GetOwningNode()->IsA<USMGraphNode_AnyStateNode>())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorAnyStateNode", "Cannot connect to an AnyState Node."));
	}

	if (const USMGraphNode_RerouteNode* RerouteNodeB = Cast<USMGraphNode_RerouteNode>(PinB->GetOwningNode()))
	{
		// Don't allow state -> reroute if reroute already has an inbound connection.
		if (RerouteNodeB->GetInputPin()->LinkedTo.Num() > 0)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorStateToRerouteNode", "Cannot connect a state to an active reroute node."));
		}

		const USMGraphNode_RerouteNode* RerouteNodeA = Cast<USMGraphNode_RerouteNode>(PinA->GetOwningNode());
		if (RerouteNodeA && RerouteNodeA->GetPrimaryTransition() != nullptr && RerouteNodeB->GetPrimaryTransition() != nullptr)
		{
			// Reroute of a transition type to another reroute of a different transition.
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorMismatchedReroute", "Cannot connect different transition types."));
		}
	}
	
	const bool bPinAIsEntry = PinA->GetOwningNode()->IsA(USMGraphNode_StateMachineEntryNode::StaticClass());
	const bool bPinBIsEntry = PinB->GetOwningNode()->IsA(USMGraphNode_StateMachineEntryNode::StaticClass());
	USMGraphNode_StateNodeBase* StateNodeA = Cast<USMGraphNode_StateNodeBase>(PinA->GetOwningNode());
	USMGraphNode_StateNodeBase* StateNodeB = Cast<USMGraphNode_StateNodeBase>(PinB->GetOwningNode());

	if (bPinAIsEntry || bPinBIsEntry)
	{
		if (bPinAIsEntry && StateNodeB)
		{
			if (StateNodeB->IsA<USMGraphNode_RerouteNode>())
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorEntryNodeToReroute", "Cannot connect an entry node to a reroute node."));
			}

			// Check for user defined rules.
			FPinConnectionResponse UserResponse;
			if (!DoesUserAllowPlacement(PinA->GetOwningNode(), PinB->GetOwningNode(), UserResponse))
			{
				return UserResponse;
			}

			USMGraphNode_StateMachineEntryNode* EntryNode = CastChecked<USMGraphNode_StateMachineEntryNode>(PinA->GetOwningNode());
			if (EntryNode->bAllowParallelEntryStates)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
			}
			
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT(""));
		}

		if (bPinBIsEntry && StateNodeA)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorEntryNode", "Cannot connect a state to an entry node."));
		}

		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorNotStateNode", "Entry must connect to a state node."));
	}

	const bool bPinAIsTransition = PinA->GetOwningNode()->IsA(USMGraphNode_TransitionEdge::StaticClass());
	const bool bPinBIsTransition = PinB->GetOwningNode()->IsA(USMGraphNode_TransitionEdge::StaticClass());

	if (bPinAIsTransition && bPinBIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorTransition", "Cannot wire a transition to a transition."));
	}
	if (bPinAIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT(""));
	}
	if (bPinBIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT(""));
	}

	// Check for user defined rules.
	FPinConnectionResponse UserResponse;
	if (!DoesUserAllowPlacement(PinA->GetOwningNode(), PinB->GetOwningNode(), UserResponse))
	{
		return UserResponse;
	}
	
	if (!bPinAIsTransition && !bPinBIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, TEXT("Create a transition."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
}

bool USMGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	if (CanCreateConnection(PinA, PinB).Response == CONNECT_RESPONSE_DISALLOW)
	{
		return false;
	}

	if (PinB->Direction == PinA->Direction)
	{
		if (USMGraphNode_StateNodeBase* Node = Cast<USMGraphNode_StateNodeBase>(PinB->GetOwningNode()))
		{
			if (PinA->Direction == EGPD_Input)
			{
				PinB = Node->GetOutputPin();
			}
			else
			{
				PinB = Node->GetInputPin();
			}
		}
	}

	const bool bModified = (PinA && PinB) ? UEdGraphSchema::TryCreateConnection(PinA, PinB) : false;

	if (bModified)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(PinA->GetOwningNode());
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

		const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
		if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Standard)
		{
			ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprint(Cast<USMBlueprint>(Blueprint));
		}
	}

	return bModified;
}

bool USMGraphSchema::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* A, UEdGraphPin* B) const
{
	USMGraphNode_StateNodeBase* NodeA = Cast<USMGraphNode_StateNodeBase>(A->GetOwningNode());
	USMGraphNode_StateNodeBase* NodeB = Cast<USMGraphNode_StateNodeBase>(B->GetOwningNode());

	if (NodeA == nullptr || NodeB == nullptr)
	{
		return false;
	}

	if (NodeA->GetOutputPin() == nullptr || NodeB->GetInputPin() == nullptr)
	{
		return false;
	}

	const bool bIsForRerouteNode = NodeA->IsA<USMGraphNode_RerouteNode>() || NodeB->IsA<USMGraphNode_RerouteNode>();

	const FVector2D InitPos((NodeA->NodePosX + NodeB->NodePosX) / 2, (NodeA->NodePosY + NodeB->NodePosY) / 2);

	FSMGraphSchemaAction_NewNode Action;
	Action.GraphNodeTemplate = NewObject<USMGraphNode_TransitionEdge>(NodeA->GetGraph());
	Action.bDontCallPostPlacedNode = bIsForRerouteNode;

	USMGraphNode_TransitionEdge* EdgeNode = CastChecked<USMGraphNode_TransitionEdge>(Action.PerformAction(NodeA->GetGraph(), nullptr, InitPos, false));

	if (A->Direction == EGPD_Output)
	{
		if (USMGraphNode_RerouteNode* RerouteNodeA = Cast<USMGraphNode_RerouteNode>(NodeA))
		{
			TArray<USMGraphNode_TransitionEdge*> NodeBOutTransitions;
			NodeB->GetOutputTransitions(NodeBOutTransitions);
			for (USMGraphNode_TransitionEdge* Transition : NodeBOutTransitions)
			{
				if (Transition->IsConnectedToRerouteNode(RerouteNodeA))
				{
					// This is now a self link.
					UEdGraphSchema::BreakPinLinks(*NodeA->GetInputPin(), true);
					break;
				}
			}

			RerouteNodeA->BreakAllOutgoingReroutedConnections();
		}

		EdgeNode->CreateConnections(NodeA, NodeB);
	}
	else
	{
		if (NodeB->IsA<USMGraphNode_RerouteNode>())
		{
			UEdGraphSchema::BreakPinLinks(*NodeB->GetOutputPin(), true);
		}

		EdgeNode->CreateConnections(NodeB, NodeA);
	}

	if (bIsForRerouteNode)
	{
		EdgeNode->PostPlacedNewNode();
	}

	// If this is a transition being placed as part of a new state node then the state node will handle this.
	// This only matters if this transition is being connected after a state has been placed.
	const UClass* StateMachineClass = FSMBlueprintEditorUtils::GetStateMachineClassFromGraph(NodeA->GetOwningStateMachineGraph());

	const UClass* PreviousStateClass = NodeA->GetNodeClass();
	if (const USMGraphNode_RerouteNode* RerouteNodeA = Cast<USMGraphNode_RerouteNode>(NodeA))
	{
		if (const USMGraphNode_TransitionEdge* TransitionEdge = RerouteNodeA->GetPrimaryTransition())
		{
			if (const USMGraphNode_StateNodeBase* FromState = TransitionEdge->GetFromState())
			{
				PreviousStateClass = FromState->GetNodeClass();
			}
		}
	}

	const UClass* ToStateClass = NodeB->GetNodeClass();
	if (const USMGraphNode_RerouteNode* RerouteNodeB = Cast<USMGraphNode_RerouteNode>(NodeB))
	{
		if (const USMGraphNode_TransitionEdge* TransitionEdge = RerouteNodeB->GetPrimaryTransition())
		{
			if (const USMGraphNode_StateNodeBase* ToState = TransitionEdge->GetToState())
			{
				ToStateClass = ToState->GetNodeClass();
			}
		}
	}
	
	SetTransitionClassFromRules(EdgeNode, PreviousStateClass, ToStateClass, StateMachineClass);

	// Self transition.
	if (NodeA == NodeB)
	{
		if (USMTransitionInstance* TransitionInstance = EdgeNode->GetNodeTemplateAs<USMTransitionInstance>())
		{
			TransitionInstance->SetCanEvalWithStartState(false);
		}
	}

	EdgeNode->UpdatePrimaryTransition();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(EdgeNode);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	return true;
}

FConnectionDrawingPolicy* USMGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID,
	float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj) const
{
	return new FSMGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

FLinearColor USMGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);
}

void USMGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);

	if (const USMGraphNode_StateMachineStateNode* StateNode = Cast<const USMGraphNode_StateMachineStateNode>(Graph.GetOuter()))
	{
		FString NodeType = "state machine";
		if (StateNode->IsA<USMGraphNode_StateMachineParentNode>())
		{
			NodeType = "parent";
		}
		else if (StateNode->IsStateMachineReference())
		{
			NodeType = "reference";
		}

		DisplayInfo.PlainName = FText::Format(LOCTEXT("StateNameGraphTitle", "{0} ({1})"), FText::FromString(StateNode->GetStateName()), FText::FromString(NodeType));
	}
	DisplayInfo.DisplayName = DisplayInfo.PlainName;
	DisplayInfo.DocExcerptName = nullptr;
	DisplayInfo.Tooltip = DisplayInfo.PlainName;
}

void USMGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakNodeLinks", "Break Node Links"));

	// Most nodes work fine without this. StateMachineEntry node does not.
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(&TargetNode);
	Super::BreakNodeLinks(TargetNode);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void USMGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links"));

	USMGraphNode_RerouteNode* Reroute = nullptr;

	if (USMGraphNode_TransitionEdge* TransitionEdge = Cast<USMGraphNode_TransitionEdge>(TargetPin.GetOwningNode()))
	{
		TransitionEdge->UpdatePrimaryTransition();
		if (TransitionEdge->GetPreviousRerouteNode() != nullptr)
		{
			// Don't set the reroute so it won't be deleted when it's the first one.
			// This way a user can delete the first rerouted transition and connect another state to it.
			Reroute = TransitionEdge->GetNextRerouteNode();
		}
	}
	else
	{
		Reroute = Cast<USMGraphNode_RerouteNode>(TargetPin.GetOwningNode());
	}

	if (Reroute)
	{
		Reroute->BreakAllOutgoingReroutedConnections();
	}

	// Most nodes work fine without this. StateMachineEntry node does not.
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin.GetOwningNode());
	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Standard)
	{
		ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprint(Cast<USMBlueprint>(Blueprint));
	}
}

void USMGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link"));

	USMGraphNode_RerouteNode* Reroute = nullptr;

	if (USMGraphNode_TransitionEdge* TransitionEdge = Cast<USMGraphNode_TransitionEdge>(SourcePin->GetOwningNode()))
    {
    	TransitionEdge->UpdatePrimaryTransition();
		if (TransitionEdge->GetPreviousRerouteNode() != nullptr)
		{
			// Don't set the reroute so it won't be deleted when it's the first one.
			// This way a user can delete the first rerouted transition and connect another state to it.
			Reroute = TransitionEdge->GetNextRerouteNode();
		}
    }
	else
	{
		Reroute = Cast<USMGraphNode_RerouteNode>(SourcePin->GetOwningNode());
	}

	if (Reroute)
	{
		Reroute->BreakAllOutgoingReroutedConnections();
	}

	// Most nodes work fine without this. StateMachineEntry node does not.
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin->GetOwningNode());
	Super::BreakSinglePinLink(SourcePin, TargetPin);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

bool USMGraphSchema::SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const
{
	return Cast<USMGraphNode_StateNode>(InTargetNode) != nullptr;
}

void USMGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		if (USMGraph* StateMachineGraph = Cast<USMGraph>(&GraphBeingRemoved))
		{
			bool bHasBoundGraph = false;

			UEdGraphNode* StateMachineNode = nullptr;
			if (USMGraphK2Node_StateMachineNode* StateMachineK2Node = StateMachineGraph->GetOwningStateMachineK2Node())
			{
				StateMachineNode = StateMachineK2Node;
				bHasBoundGraph = StateMachineK2Node->GetStateMachineGraph() != nullptr;
			}
			else if (USMGraphNode_StateMachineStateNode* StateMachineStateNode = StateMachineGraph->GetOwningStateMachineNodeWhenNested())
			{
				StateMachineNode = StateMachineStateNode;
				bHasBoundGraph = !StateMachineStateNode->IsSwitchingGraphTypes() && StateMachineStateNode->GetBoundGraph() != nullptr;
			}
			else
			{
				// No entry node.
				checkNoEntry();
			}

			// Let the node delete first-- it will trigger graph removal. Helps with undo buffer transaction.
			if (bHasBoundGraph)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, StateMachineNode, true);
				return;
			}

			// Remove this graph from the parent graph.
			UEdGraph* ParentGraph = StateMachineNode->GetGraph();
			ParentGraph->SubGraphs.Remove(StateMachineGraph);

			// Remove all contained states and transitions.
			TArray<UEdGraphNode*> AllNodes;
			StateMachineGraph->GetNodesOfClass<UEdGraphNode>(AllNodes);

			// Remove all sub nodes.
			for (UEdGraphNode* Node : AllNodes)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	Super::HandleGraphBeingDeleted(GraphBeingRemoved);
}

void USMGraphSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB,
	const FVector2D& GraphPosition) const
{
	const FScopedTransaction Transaction(LOCTEXT("CreateRerouteNodeOnWire", "Create Reroute Node"));

	const FVector2D NodeSpacerSize = SGraphNode_TransitionEdge::GetTotalRerouteSpacerSize();
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize / 2.f);

	// Create a new reroute node.
	UEdGraph* ParentGraph = PinA->GetOwningNode()->GetGraph();
	if (!FBlueprintEditorUtils::IsGraphReadOnly(ParentGraph))
	{
		if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(PinB->GetOwningNode()))
		{
			const USMGraphNode_StateNodeBase* NextState = Transition->GetToState(true);
			const USMGraphNode_StateNodeBase* PrevState = Transition->GetFromState();

			if (NextState != PrevState)
			{
				USMGraphNode_RerouteNode* NewReroute =
					FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate(ParentGraph,
						NewObject<USMGraphNode_RerouteNode>(), KnotTopLeft);

				Transition->GetOutputPin()->BreakLinkTo(NextState->GetInputPin());
				Transition->GetOutputPin()->MakeLinkTo(NewReroute->GetInputPin());

				TryCreateConnection(NewReroute->GetOutputPin(), NextState->GetInputPin());

				NewReroute->ReconstructNode();

				UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			}
		}
	}
}

bool USMGraphSchema::DoesUserAllowPlacement(const UEdGraphNode* A, const UEdGraphNode* B, FPinConnectionResponse& ResponseOut)
{
	const USMGraphNode_Base* StateNodeA = Cast<USMGraphNode_Base>(A);
	const USMGraphNode_Base* StateNodeB = Cast<USMGraphNode_Base>(B);

	ensure(!StateNodeA || !StateNodeA->IsA<USMGraphNode_TransitionEdge>());
	ensure(!StateNodeB || !StateNodeB->IsA<USMGraphNode_TransitionEdge>());
	
	UClass* StateClassA = StateNodeA ? StateNodeA->GetNodeClass() : nullptr;
	UClass* StateClassB = StateNodeB ? StateNodeB->GetNodeClass() : nullptr;

	if (const USMGraphNode_RerouteNode* RerouteA = Cast<USMGraphNode_RerouteNode>(StateNodeA))
	{
		if (const USMGraphNode_StateNodeBase* PrevStateA = RerouteA->GetPreviousState())
		{
			StateClassA = PrevStateA->GetNodeClass();
		}
	}

	if (const USMGraphNode_RerouteNode* RerouteB = Cast<USMGraphNode_RerouteNode>(StateNodeB))
	{
		if (const USMGraphNode_StateNodeBase* PrevStateB = RerouteB->GetPreviousState())
		{
			StateClassB = PrevStateB->GetNodeClass();
		}
	}

	if (const USMGraph* StateMachineGraph = StateNodeA ? StateNodeA->GetOwningStateMachineGraph() : StateNodeB ? StateNodeB->GetOwningStateMachineGraph() : nullptr)
	{
		UClass* StateMachineClass = FSMBlueprintEditorUtils::GetStateMachineClassFromGraph(StateMachineGraph);

		if (StateClassA)
		{
			if (const USMStateInstance_Base* DefaultObject = Cast<USMStateInstance_Base>(StateClassA->GetDefaultObject()))
			{
				const FSMStateConnectionValidator& Filter = DefaultObject->GetAllowedConnections();
				if (!Filter.IsOutboundConnectionValid(StateClassB, StateMachineClass))
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("FromClass"), FText::FromString(GetNameSafe(StateClassA)));
					Args.Add(TEXT("ToClass"), FText::FromString(GetNameSafe(StateClassB)));
					
					ResponseOut = FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, FText::Format(LOCTEXT("PinRuleViolation",
						"A user defined rule in {FromClass} prevents a connection to state class: {ToClass}."), Args));
					return false;
				}
			}
		}
		if (StateClassB)
		{
			if (const USMStateInstance_Base* DefaultObject = Cast<USMStateInstance_Base>(StateClassB->GetDefaultObject()))
			{
				const FSMStateConnectionValidator& Filter = DefaultObject->GetAllowedConnections();
				if (!Filter.IsInboundConnectionValid(StateClassA, StateMachineClass))
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("FromClass"), FText::FromString(GetNameSafe(StateClassA)));
					Args.Add(TEXT("ToClass"), FText::FromString(GetNameSafe(StateClassB)));
					
					ResponseOut = FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, FText::Format(LOCTEXT("PinRuleViolation",
						"A user defined rule in {ToClass} prevents a connection from state class: {FromClass}."), Args));
					return false;
				}
			}
		}
	}

	return true;
}

bool USMGraphSchema::CanReplaceNode(const UEdGraphNode* InGraphNode)
{
	bool bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddParent;
	return CanReplaceNodeWith(InGraphNode, bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddParent);
}

bool USMGraphSchema::CanReplaceNodeWith(const UEdGraphNode* InGraphNode, bool& bStateMachine, bool& bStateMachineRef,
	bool& bState, bool& bConduit, bool& bStateMachineParent)
{
	if (!InGraphNode->IsA<USMGraphNode_StateNodeBase>() || InGraphNode->IsA<USMGraphNode_RerouteNode>())
	{
		return false;
	}

	bool bCanAddStateMachine = !InGraphNode->IsA<USMGraphNode_StateMachineStateNode>() || InGraphNode->IsA<USMGraphNode_StateMachineParentNode>();
	bool bCanAddStateMachineRef = bCanAddStateMachine;
	bool bCanAddStateMachineParent = !InGraphNode->IsA<USMGraphNode_StateMachineParentNode>();
	bool bCanAddState = !InGraphNode->IsA<USMGraphNode_StateNode>();
	bool bCanAddConduit = !InGraphNode->IsA<USMGraphNode_ConduitNode>();

	if (USMGraphNode_StateMachineStateNode const* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(InGraphNode))
	{
		if (!StateMachineNode->IsA<USMGraphNode_StateMachineParentNode>())
		{
			if (StateMachineNode->IsStateMachineReference())
			{
				bCanAddStateMachine = true;
				bCanAddStateMachineRef = false;
			}
			else
			{
				bCanAddStateMachine = false;
				bCanAddStateMachineRef = true;
			}
		}
	}

	// Only allow parent to be set if the blueprint is a child.
	if (bCanAddStateMachineParent)
	{
		TArray<USMBlueprintGeneratedClass*> ParentClasses;
		UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(InGraphNode);
		bCanAddStateMachineParent = FSMBlueprintEditorUtils::TryGetParentClasses(OwnerBlueprint, ParentClasses);
	}

	bStateMachine = bCanAddStateMachine;
	bStateMachineRef = bCanAddStateMachineRef;
	bState = bCanAddState;
	bConduit = bCanAddConduit;
	bStateMachineParent = bCanAddStateMachineParent;

	return bStateMachine || bStateMachineRef || bState || bConduit || bStateMachineParent;
}

bool USMGraphSchema::SetTransitionClassFromRules(USMGraphNode_TransitionEdge* InTransitionEdge,
												 const UClass* InFromStateClass, const UClass* InToStateClass,
												 const UClass* InStateMachineClass,
												 TSubclassOf<USMTransitionInstance> InBaseClass)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("USMGraphSchema::SetTransitionClassFromRules"), STAT_SetTransitionClassFromRules, STATGROUP_LogicDriverEditor);

	// The goal is to find the furthest transition child class that passes rules. For most cases there should be only one
	// class that passes rules, but in the event a project is overloading a plugin's class that has rules defined, the
	// project version should supersede the plugin class.

	// This has no specific handling for sibling classes that pass. The first one loaded into memory/iterated on would
	// be used.

	if (InBaseClass == nullptr)
	{
		InBaseClass = USMTransitionInstance::StaticClass();
	}

	TArray<TSoftClassPtr<USMTransitionInstance>> TransitionClasses;
	FSMNodeInstanceUtils::GetAllNodeSubClasses(InBaseClass, TransitionClasses);

	auto DoesTransitionClassPassRules = [&](const TSoftClassPtr<USMTransitionInstance>& InTransitionClass) -> bool
	{
		if (!FSMNodeInstanceUtils::CanTransitionBeAutoPlaced(InTransitionClass))
		{
			return false;
		}
		
		return LD::NodeRuleUtils::DoesTransitionClassPassRules(InTransitionClass,
			InFromStateClass, InToStateClass, InStateMachineClass, false);
	};

	// Find the first transition class that passes rules. Go start to end for consistency.
	for (const TSoftClassPtr<USMTransitionInstance>& TransitionClass : TransitionClasses)
	{
		if (TransitionClass.Get() == InBaseClass)
		{
			continue;
		}

		// Validate allowed placement in state machine.
		if (InStateMachineClass
			&& !CastChecked<USMStateMachineInstance>(
				InStateMachineClass->GetDefaultObject())->GetAllowedNodes().IsTransitionAllowed(TransitionClass))
		{
			continue;
		}

		if (DoesTransitionClassPassRules(TransitionClass))
		{
			TSoftClassPtr<USMTransitionInstance> TargetTransitionClass = TransitionClass;
			
			// Now gather children classes and reverse the search to find the furthest most child that passes.
			// Just load the base class into memory now since it would be anyway upon loading any children.

			TArray<TSoftClassPtr<USMTransitionInstance>> ChildrenClasses;
			FSMNodeInstanceUtils::GetAllNodeSubClasses(TransitionClass.LoadSynchronous(), ChildrenClasses);

			for (int32 ChildIdx = ChildrenClasses.Num() - 1; ChildIdx >= 0; --ChildIdx)
			{
				const TSoftClassPtr<USMTransitionInstance>& ChildTransitionClass = ChildrenClasses[ChildIdx];
				if (DoesTransitionClassPassRules(ChildTransitionClass))
				{
					TargetTransitionClass = ChildTransitionClass;
					break;
				}
			}

			UClass* LoadedClass = TargetTransitionClass.LoadSynchronous();
			
			ensure(LoadedClass != nullptr
				&& LoadedClass->IsChildOf(USMTransitionInstance::StaticClass())
					&& LoadedClass->IsChildOf(TransitionClass.Get()));

			InTransitionEdge->SetNodeClass(LoadedClass);
			InTransitionEdge->CreateGraphPropertyGraphs();

			// If the instance has a custom name supplied use that.
			RenameNodeFromTemplateData(InTransitionEdge);
			
			return true;
		}
	}

	return false;
}

void USMGraphSchema::RenameNodeFromTemplateData(USMGraphNode_Base* InGraphNode)
{
	check(InGraphNode);
	
	if (const USMNodeInstance* NodeInstance = Cast<USMNodeInstance>(InGraphNode->GetNodeTemplate()))
	{
		if (!FSMNodeClassRule::IsBaseClass(NodeInstance->GetClass()))
		{
			if (const USMTransitionInstance* TransitionInstance = Cast<USMTransitionInstance>(NodeInstance))
			{
				// Transitions should only rename themselves if a custom name was set deliberately.
				if (TransitionInstance->GetNodeDescription().Name.IsNone())
				{
					return;
				}
			}
		
			const FString DefaultNodeName = FSMNodeInstanceUtils::GetNodeDisplayName(NodeInstance);
			if (!DefaultNodeName.IsEmpty())
			{
				const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(InGraphNode);
				FBlueprintEditorUtils::RenameGraphWithSuggestion(InGraphNode->GetBoundGraph(), NameValidator, DefaultNodeName);
			}
		}
	}
}

void USMGraphSchema::GetReplaceWithMenuActions(FMenuBuilder& MenuBuilder, const UEdGraphNode* InGraphNode) const
{
	bool bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddParent;

	if (!CanReplaceNodeWith(InGraphNode, bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddParent))
	{
		return;
	}

	MenuBuilder.BeginSection("SMGraphSchemaNodeReplacementActions", LOCTEXT("NodeActionsReplacementMenuHeader", "Replacement"));

	if (bCanAddStateMachine)
	{
		MenuBuilder.AddMenuEntry(FSMEditorCommands::Get().ReplaceWithStateMachine);
	}

	if (bCanAddStateMachineRef)
	{
		MenuBuilder.AddMenuEntry(FSMEditorCommands::Get().ReplaceWithStateMachineReference);
	}

	if (bCanAddParent)
	{
		MenuBuilder.AddMenuEntry(FSMEditorCommands::Get().ReplaceWithStateMachineParent);
	}

	if (bCanAddState)
	{
		MenuBuilder.AddMenuEntry(FSMEditorCommands::Get().ReplaceWithState);
	}

	if (bCanAddConduit)
	{
		MenuBuilder.AddMenuEntry(FSMEditorCommands::Get().ReplaceWithConduit);
	}

	MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE
