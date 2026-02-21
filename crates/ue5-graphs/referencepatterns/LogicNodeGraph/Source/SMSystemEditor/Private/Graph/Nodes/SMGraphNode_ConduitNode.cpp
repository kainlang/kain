// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_ConduitNode.h"

#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "FunctionNodes/SMGraphK2Node_FunctionNodes_ConduitInstance.h"
#include "FunctionNodes/SMGraphK2Node_FunctionNodes_StateInstance.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_ConduitResultNode.h"
#include "Graph/Schema/SMConduitGraphSchema.h"
#include "Graph/SMConduitGraph.h"
#include "RootNodes/SMGraphK2Node_IntermediateRootStateMachineNodes.h"
#include "RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "SMUnrealTypeDefs.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "SMGraphConduitNode"

USMGraphNode_ConduitNode::USMGraphNode_ConduitNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bEvalWithTransitions_DEPRECATED(false), bWasEvaluating(false)
{
}

void USMGraphNode_ConduitNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
	CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

FText USMGraphNode_ConduitNode::GetTooltipText() const
{
	return LOCTEXT("ConduitNodeTooltip", "A conduit allows a single condition to be checked before selecting one of many transitions.");
}

void USMGraphNode_ConduitNode::PostPlacedNewNode()
{
	SetToCurrentVersion();
	
	// Create a new state machine graph
	check(BoundGraph == nullptr);
	BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
		this,
		NAME_None,
		USMConduitGraph::StaticClass(),
		USMConduitGraphSchema::StaticClass());
	check(BoundGraph);

	// Find an interesting name
	const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, *GetFriendlyNodeName().ToString());

	// Initialize the state machine graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();

	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}

	if (bGenerateTemplateOnNodePlacement)
	{
		InitTemplate();

		if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bDefaultNewConduitsToTrue)
		{
			// Set default conduit value to true if applicable.
			const USMConduitGraph* ConduitGraph = CastChecked<USMConduitGraph>(BoundGraph);
			if (ConduitGraph->ResultNode)
			{
				Schema->TrySetDefaultValue(*ConduitGraph->ResultNode->GetTransitionEvaluationPin(), TEXT("True"));
			}
		}
		
		if (USMConduitInstance* ConduitInstance = GetNodeTemplateAs<USMConduitInstance>())
		{
			ConduitInstance->SetEvalWithTransitions(FSMBlueprintEditorUtils::GetProjectEditorSettings()->bConfigureNewConduitsAsTransitions);
		}
	}
}

void USMGraphNode_ConduitNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Enable templates
	bool bStateChange = false;
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_ConduitNode, ConduitClass))
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

	Super::PostEditChangeProperty(PropertyChangedEvent);
	bCreatePropertyGraphsOnPropertyChange = true;
	bPostEditChangeConstructionRequiresFullRefresh = true;

	if (bStateChange && IsSafeToConditionallyCompile(PropertyChangedEvent.ChangeType))
	{
		FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this), false);
	}
}

FSlateIcon USMGraphNode_ConduitNode::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), TEXT("Graph.ConduitNode.Icon"));
}

void USMGraphNode_ConduitNode::ResetDebugState()
{
	Super::ResetDebugState();

	// Prevents a previous cycle from showing it as running.
	if (const FSMConduit* DebugNode = (FSMConduit*)GetDebugNode())
	{
		DebugNode->bWasEvaluating = bWasEvaluating = false;
	}
}

void USMGraphNode_ConduitNode::UpdateTime(float DeltaTime)
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	if (ShouldEvalWithTransitions() && Settings->bDisplayTransitionEvaluation)
	{
		if (const FSMConduit* DebugNode = (FSMConduit*)GetDebugNode())
		{
			if (WasEvaluating() && (DebugNode->IsActive() || DebugNode->bWasActive))
			{
				// Cancel evaluation display and let the super method reset.
				bWasEvaluating = false;
				bWasDebugActive = false;
			}
			else if (DebugNode->bIsEvaluating || DebugNode->bWasEvaluating)
			{
				// Not active but evaluating.
				bIsDebugActive = true;
				bWasEvaluating = true;
			}
			DebugNode->bWasEvaluating = false;
		}
	}

	Super::UpdateTime(DeltaTime);

	if (!WasDebugNodeActive())
	{
		bWasEvaluating = false;
	}
}

void USMGraphNode_ConduitNode::ImportDeprecatedProperties()
{
	Super::ImportDeprecatedProperties();

	if (USMConduitInstance* Instance = Cast<USMConduitInstance>(GetNodeTemplate()))
	{
		if (GetLoadedVersion() < TEMPLATE_PROPERTY_VERSION)
		{
			Instance->SetEvalWithTransitions(bEvalWithTransitions_DEPRECATED);
		}
	}
}

void USMGraphNode_ConduitNode::PlaceDefaultInstanceNodes()
{
	Super::PlaceDefaultInstanceNodes();

	USMGraphK2Node_ConduitResultNode* ResultNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphK2Node_ConduitResultNode>(BoundGraph);
	USMGraphK2Node_ConduitInstance_CanEnterTransition* InstanceCanEnterTransition = nullptr;
	if (FSMBlueprintEditorUtils::PlaceNodeIfNotSet<USMGraphK2Node_ConduitInstance_CanEnterTransition>(BoundGraph, ResultNode, &InstanceCanEnterTransition, EGPD_Input, -550))
	{
		// Pin names won't match correctly so manually wire.
		InstanceCanEnterTransition->GetSchema()->TryCreateConnection(InstanceCanEnterTransition->FindPin(UEdGraphSchema_K2::PN_ReturnValue), ResultNode->GetInputPin());
	}

	// Optional nodes.
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_IntermediateStateMachineStartNode, USMGraphK2Node_StateInstance_StateMachineStart>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_IntermediateStateMachineStopNode, USMGraphK2Node_StateInstance_StateMachineStop>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionEnteredNode, USMGraphK2Node_ConduitInstance_OnConduitEntered>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionInitializedNode, USMGraphK2Node_ConduitInstance_OnConduitInitialized>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionShutdownNode, USMGraphK2Node_ConduitInstance_OnConduitShutdown>(BoundGraph);
}

void USMGraphNode_ConduitNode::SetNodeClass(UClass* Class)
{
	ConduitClass = Class;
	Super::SetNodeClass(Class);
}

const FSlateBrush* USMGraphNode_ConduitNode::GetNodeIcon() const
{
	if (const FSlateBrush* Icon = Super::GetNodeIcon())
	{
		return Icon;
	}

	return FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.ConduitNode.Icon"));
}

void USMGraphNode_ConduitNode::SetRuntimeDefaults(FSMState_Base& State) const
{
	Super::SetRuntimeDefaults(State);
	FSMConduit& Conduit = static_cast<FSMConduit&>(State);
	Conduit.bEvalWithTransitions = ShouldEvalWithTransitions();
	if (const USMConduitInstance* Instance = Cast<USMConduitInstance>(GetNodeTemplate()))
	{
		Conduit.bCanEvaluate = Instance->bCanEvaluate;
		Conduit.ConditionalEvaluationType = CastChecked<USMConduitGraph>(BoundGraph)->GetConditionalEvaluationType();
	}
}

FLinearColor USMGraphNode_ConduitNode::GetActiveBackgroundColor() const
{
	const FLinearColor BaseColor = Super::GetActiveBackgroundColor();
	
	if (ShouldEvalWithTransitions())
	{
		const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
		if (Settings->bDisplayTransitionEvaluation)
		{
			if (const FSMConduit* DebugNode = (FSMConduit*)GetDebugNode())
			{
				if (DebugNode->bIsEvaluating || bWasEvaluating)
				{
					const float TimeToFade = 0.7f;
					const float DebugTime = GetDebugTime();
					if (DebugTime < TimeToFade)
					{
						return FLinearColor::LerpUsingHSV(Settings->EvaluatingTransitionColor, BaseColor, DebugTime / TimeToFade);
					}
				}
			}
		}
	}

	return BaseColor;
}

UObject* USMGraphNode_ConduitNode::GetJumpTargetForDoubleClick() const
{
	if (FSMBlueprintEditorUtils::GetEditorSettings()->ConduitDoubleClickBehavior == ESMJumpToGraphBehavior::PreferExternalGraph)
	{
		if (const UClass* Class = GetNodeClass())
		{
			if (UBlueprint* NodeBlueprint = UBlueprint::GetBlueprintFromClass(Class))
			{
				return NodeBlueprint;
			}
		}
	}
	
	return Super::GetJumpTargetForDoubleClick();
}

bool USMGraphNode_ConduitNode::ShouldEvalWithTransitions() const
{
	if (USMConduitInstance* Instance = Cast<USMConduitInstance>(GetNodeTemplate()))
	{
		return Instance->GetEvalWithTransitions();
	}

	return false;
}

FLinearColor USMGraphNode_ConduitNode::Internal_GetBackgroundColor() const
{
	const FLinearColor* CustomColor = GetCustomBackgroundColor();
	if (ShouldEvalWithTransitions() && !CustomColor && !IsEndState())
	{
		// Draw colors as though this conduit is part of a transition.
		const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
		const FLinearColor ColorModifier = FLinearColor(0.7f, 0.7f, 0.7f, 0.7f);
		const FLinearColor DefaultColor = Settings->TransitionEmptyColor * ColorModifier;
	
		const USMConduitGraph* Graph = Cast<USMConduitGraph>(BoundGraph);
		if (!Graph)
		{
			return DefaultColor;
		}

		const bool bHasResultLogic = Graph->HasAnyLogicConnections();
		// This conduit will never be taken.
		if (!bHasResultLogic)
		{
			return DefaultColor;
		}
	
		// Regular transition conduit.
		return Settings->TransitionValidColor * ColorModifier;
	}

	// Draw colors as though this conduit is a state.
	return Super::Internal_GetBackgroundColor();
}

#undef LOCTEXT_NAMESPACE
