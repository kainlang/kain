// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Compilers/SMKismetCompiler.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "FunctionNodes/SMGraphK2Node_FunctionNodes_StateInstance.h"
#include "FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionEvent.h"
#include "FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionInstance.h"
#include "FunctionNodes/SMGraphK2Node_StateWriteNodes.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Schema/SMTransitionGraphSchema.h"
#include "Graph/SMTransitionGraph.h"
#include "RootNodes/SMGraphK2Node_IntermediateRootStateMachineNodes.h"
#include "RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMTransition.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Engine/Texture2D.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "SMUnrealTypeDefs.h"
#include "UObject/UObjectThreadContext.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SMGraphTransition"

USMGraphNode_TransitionEdge::USMGraphNode_TransitionEdge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), DelegateOwnerInstance(SMDO_This), DelegateOwnerClass(nullptr),
	  bEventTriggersTargetedUpdate(true),
	  bEventTriggersFullUpdate(false),
	  bCanEvaluate_DEPRECATED(true),
	  bCanEvaluateFromEvent_DEPRECATED(true), bCanEvalWithStartState_DEPRECATED(true), bAutoFormatGraph(true),
	  bNOTPrimaryCondition(false), PriorityOrder_DEPRECATED(0), InitialOperatorNode(nullptr),
	  CachedHoveredTransitionStack(nullptr),
	  bWasEvaluating(false), TimeSinceHover(0), bIsHoveredByUser(false), bFromAnyState(false), bFromLinkState(false),
	  bChangingProperty(false), bUsingCustomName(false)
{
	bCanRenameNode = true;
	LastHoverTimeStamp = FDateTime::UtcNow();
}

void USMGraphNode_TransitionEdge::SetRuntimeDefaults(FSMTransition& Transition) const
{
	Transition.NodePosition = NodePosition;
	Transition.bHasInputEvents = FSMBlueprintEditorUtils::DoesGraphHaveInputEvents(GetBoundGraph());
	
	if (const USMTransitionInstance* Instance = Cast<USMTransitionInstance>(GetNodeTemplate()))
	{
		Transition.bAlwaysFalse = !PossibleToTransition();
		Transition.ConditionalEvaluationType = GetTransitionGraph()->GetConditionalEvaluationType();
		Transition.Priority = Instance->GetPriorityOrder();
		Transition.bCanEvaluate = Instance->bCanEvaluate;
		Transition.bCanEvaluateFromEvent = Instance->GetCanEvaluateFromEvent();
		Transition.bCanEvalWithStartState = Instance->GetCanEvalWithStartState();
		Transition.bRunParallel = Instance->GetRunParallel();
		Transition.bEvalIfNextStateActive = Instance->GetEvalIfNextStateActive();
		Transition.bFromAnyState = IsFromAnyState();
		Transition.bFromLinkState = IsFromLinkState();

		// Perform safety check in case this transition didn't have its bound graph renamed when updating to 2.8.
		{
			const FString GraphName = GetTransitionName();
			const FString DefaultName = GetDefaultTransitionName();
			if (!bUsingCustomName && GraphName != DefaultName)
			{
				if (!Transition.bFromLinkState && !Transition.bFromAnyState) // Link states and Any states may have a different name since they don't exist at run-time.
				{
					// If this is being hit as a user, then the asset probably needs to be recompiled and saved.
					// If this is being hit from extending the plugin, then RefreshConnectedTransitions() may need to be called on the state, or UpdatePrimaryTransition() on this transition.
					const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
					LDEDITOR_LOG_WARNING(TEXT("Transition '%s' has an incorrect graph name of '%s' in blueprint '%s'. The asset may need to be recompiled and saved."), *DefaultName, *GraphName, *Blueprint->GetName())
				}
				Transition.SetNodeName(DefaultName);
			}
			else
			{
				Transition.SetNodeName(GraphName);
			}
		}
	}
}

void USMGraphNode_TransitionEdge::CopyFrom(const USMGraphNode_TransitionEdge& Transition)
{
	TransitionClass = Transition.TransitionClass;
	DelegateOwnerInstance = Transition.DelegateOwnerInstance;
	DelegateOwnerClass = Transition.DelegateOwnerClass;
	DelegatePropertyName = Transition.DelegatePropertyName;
	bUsingCustomName = Transition.bUsingCustomName;

	NodeInstanceTemplate = Transition.NodeInstanceTemplate ? CastChecked<USMNodeInstance>(StaticDuplicateObject(Transition.NodeInstanceTemplate, this))
	: nullptr;

	TransitionStack = Transition.TransitionStack;

	for (int32 Idx = 0; Idx < TransitionStack.Num(); ++Idx)
	{
		FTransitionStackContainer& ThisStackContainer = TransitionStack[Idx];
		const FTransitionStackContainer& OtherStackContainer = Transition.TransitionStack[Idx];

		ThisStackContainer.NodeStackInstanceTemplate = OtherStackContainer.NodeStackInstanceTemplate ?
			CastChecked<USMNodeInstance>(StaticDuplicateObject(OtherStackContainer.NodeStackInstanceTemplate, this))
		: nullptr;
	}
}

FSMTransition* USMGraphNode_TransitionEdge::GetRuntimeNode() const
{
	if (const USMTransitionGraph* TransitionGraph = GetTransitionGraph())
	{
		return (FSMTransition*)TransitionGraph->GetRuntimeNode();
	}

	return nullptr;
}

void USMGraphNode_TransitionEdge::AllocateDefaultPins()
{
	UEdGraphPin* Inputs = CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
	Inputs->bHidden = true;
	UEdGraphPin* Outputs = CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
	Outputs->bHidden = true;
}

FText USMGraphNode_TransitionEdge::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(GetTransitionName());
}

void USMGraphNode_TransitionEdge::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	UpdatePrimaryTransition();
	if (Pin->LinkedTo.Num() == 0)
	{
		// Commit suicide; transitions must always have an input and output connection
		Modify();

		// Our parent graph will have our graph in SubGraphs so needs to be modified to record that.
		if (UEdGraph* ParentGraph = GetGraph())
		{
			ParentGraph->Modify();
		}

		DestroyNode();
	}
}

void USMGraphNode_TransitionEdge::PostPlacedNewNode()
{
	SetToCurrentVersion();
	const USMGraphNode_TransitionEdge* Transition = GetPrimaryReroutedTransition();
	if (Transition && Transition != this)
	{
		CopyFrom(*Transition);
		BoundGraph = Transition->GetBoundGraph();
		UpdatePrimaryTransition();
		return;
	}

	CreateBoundGraph();
	SetupDelegateDefaults();

	if (bGenerateTemplateOnNodePlacement)
	{
		InitTemplate();

		if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bDefaultNewTransitionsToTrue)
		{
			// Set default transition value to true if applicable.
			const USMTransitionGraph* TransitionGraph = GetTransitionGraph();
			if (TransitionGraph->ResultNode)
			{
				const UEdGraphSchema* Schema = TransitionGraph->GetSchema();
				check(Schema);
				Schema->TrySetDefaultValue(*TransitionGraph->ResultNode->GetTransitionEvaluationPin(), TEXT("True"));
			}
		}
	}
}

void USMGraphNode_TransitionEdge::PrepareForCopying()
{
	Super::PrepareForCopying();

	if (IsPrimaryReroutedTransition() && BoundGraph->GetOuter() != this)
	{
		BoundGraph->Rename(*BoundGraph->GetName(), this, REN_DoNotDirty | REN_DontCreateRedirectors);
	}
}

void USMGraphNode_TransitionEdge::PostPasteNode()
{
	// This could potentially set the bound graph when using reroute nodes.
	UpdatePrimaryTransition();

	if (!BoundGraph)
	{
		CreateBoundGraph();

		// Make sure rerouted transitions correctly reference the new graph.
		UpdatePrimaryTransition();
	}

	TArray<UEdGraphNode*> ContainedNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UEdGraphNode>(BoundGraph, ContainedNodes);

	for (UEdGraphNode* GraphNode : ContainedNodes)
	{
		GraphNode->CreateNewGuid();
		GraphNode->PostPasteNode();
		// Required to correct context display issues.
		GraphNode->ReconstructNode();
	}

	Super::PostPasteNode();

	UEdGraphPin* InputPin = GetInputPin();
	UEdGraphPin* OutputPin = GetOutputPin();
	check(InputPin);
	check(OutputPin);

	if (InputPin->LinkedTo.Num() == 0 && OutputPin->LinkedTo.Num() == 0)
	{
		// If this transition is being copied & pasted by itself, look for nodes the user may want to link.
		
		TArray<USMGraphNode_StateNodeBase*> StateNodes;
		StateNodes.Reserve(2);
		if (const FSMBlueprintEditor* BlueprintEditor = FSMBlueprintEditorUtils::GetStateMachineEditor(this))
		{
			const TSet<TWeakObjectPtr<USMGraphNode_Base>>& Selection = BlueprintEditor->GetSelectedGraphNodesDuringPaste();
			if (Selection.Num() == 1)
			{
				if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Selection.CreateConstIterator()->Get()))
				{
					// For a single selected transition add the pasted transition to the stack.
					if (USMGraphNode_StateNodeBase* FromState = Transition->GetFromState())
					{
						StateNodes.Add(FromState);
					}
					if (USMGraphNode_StateNodeBase* ToState = Transition->GetToState())
					{
						StateNodes.Add(ToState);
					}
				}
				else if (USMGraphNode_StateNodeBase* State = Cast<USMGraphNode_StateNodeBase>(Selection.CreateConstIterator()->Get()))
				{
					// Single selected state - treat this as a self transition.
					StateNodes.Add(State);
					StateNodes.Add(State);
				}
			}
			else
			{
				// Check for multiple selected states.
				for (const TWeakObjectPtr<USMGraphNode_Base>& Object : Selection)
				{
					if (USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(Object.Get()))
					{
						if (StateNodes.Num() == 2)
						{
							// Only allow two selected state nodes.
							StateNodes.Empty();
							break;
						}
				
						StateNodes.Add(StateNode);
					}
				}
			}
		}

		if (StateNodes.Num() == 2)
		{
			check(StateNodes[0]);
			check(StateNodes[1]);

			const bool bIsSelfTransition = StateNodes[0] == StateNodes[1];
			bool bMakeConnection = true;
			
			// Don't allow pasting if going to an active reroute node or a to a reroute node as a self transition.
			{
				if (const USMGraphNode_RerouteNode* FromReroute = Cast<USMGraphNode_RerouteNode>(StateNodes[0]))
				{
					if (!FromReroute->IsRerouteEmpty() || bIsSelfTransition)
					{
						bMakeConnection = false;
					}
				}

				if (const USMGraphNode_RerouteNode* ToReroute = Cast<USMGraphNode_RerouteNode>(StateNodes[1]))
				{
					if (!ToReroute->IsRerouteEmpty() || bIsSelfTransition)
					{
						bMakeConnection = false;
					}
				}
			}

			// Prevent self transitions that already have a connection.
			if (bIsSelfTransition && StateNodes[0]->HasTransitionFromNode(StateNodes[0]))
			{
				bMakeConnection = false;
			}

			if (bMakeConnection)
			{
				InputPin->MakeLinkTo(StateNodes[0]->GetOutputPin());
				OutputPin->MakeLinkTo(StateNodes[1]->GetInputPin());
			}
		}
	}

	// Destroy this node if there are no valid connections to any states.
	for (const UEdGraphPin* Pin : Pins)
	{
		if (Pin->LinkedTo.Num() == 0)
		{
			DestroyNode();

			// The editor can maintain a reference to the destroyed object which may cause crashes if settings are edited.
			// Need to wait a tick because UE is still importing the pasted node and will try to select it.
			constexpr bool bNextTick = true;
			FSMBlueprintEditorUtils::ClearEditorSelection(this, bNextTick);
			break;
		}
	}
}

void USMGraphNode_TransitionEdge::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// Enable templates
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, TransitionClass))
	{
		InitTemplate();
	}
	else
	{
		bPostEditChangeConstructionRequiresFullRefresh = false;
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bPostEditChangeConstructionRequiresFullRefresh = true;
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegatePropertyName))
	{
		InitTransitionDelegate();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegateOwnerInstance))
	{
		DelegatePropertyName = NAME_None;
		DelegateOwnerClass = nullptr;
		InitTransitionDelegate();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegateOwnerClass))
	{
		DelegatePropertyName = NAME_None;
		InitTransitionDelegate();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, bEventTriggersTargetedUpdate) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, bEventTriggersFullUpdate))
	{
		UpdateResultNodeEventSettings();
	}
}

void USMGraphNode_TransitionEdge::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	bChangingProperty = true;

	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// ChainProperty primarily used for stack changes.
	
	if (const FProperty* HeadProperty = PropertyChangedEvent.PropertyChain.GetHead()->GetValue())
	{
		bool bCanReformatGraph = false;
		// The direct property that changed.
		const FName DirectPropertyName = HeadProperty->GetFName();
		if (DirectPropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, TransitionStack))
		{
			bCanReformatGraph = bAutoFormatGraph;

			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
			{
				// Array element duplication requires a new template generated.
				const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
				if (ArrayIndex >= 0 && ArrayIndex + 1 < TransitionStack.Num())
				{
					const FTransitionStackContainer& OriginalTransitionStack = TransitionStack[ArrayIndex];
					FTransitionStackContainer& NewTransitionStack = TransitionStack[ArrayIndex + 1];

					NewTransitionStack.TemplateGuid = FGuid::NewGuid();
					if (OriginalTransitionStack.NodeStackInstanceTemplate && OriginalTransitionStack.NodeStackInstanceTemplate->GetClass() != GetDefaultNodeClass())
					{
						if (NewTransitionStack.NodeStackInstanceTemplate != OriginalTransitionStack.NodeStackInstanceTemplate)
						{
							// This transition *shouldn't* exist because the object isn't deep copied, but who knows if USTRUCT UPROPERTY UObject handling changes?
							NewTransitionStack.DestroyTemplate();
						}
					
						NewTransitionStack.NodeStackInstanceTemplate = Cast<USMNodeInstance>(StaticDuplicateObject(OriginalTransitionStack.NodeStackInstanceTemplate, OriginalTransitionStack.NodeStackInstanceTemplate->GetOuter()));
						UEngine::CopyPropertiesForUnrelatedObjects(OriginalTransitionStack.NodeStackInstanceTemplate, NewTransitionStack.NodeStackInstanceTemplate);
						NewTransitionStack.NodeStackInstanceTemplate->SetTemplateGuid(NewTransitionStack.TemplateGuid);
					}
				}
			}
			
			//  Check if it's a property we care about.
			FEditPropertyChain::TDoubleLinkedListNode* MemberNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
			if (MemberNode && MemberNode->GetNextNode() && MemberNode->GetValue())
			{
				const FName Name = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetNextNode()->GetValue()->GetFName();
				
				// Template has been changed.
				if (Name == GET_MEMBER_NAME_CHECKED(FTransitionStackContainer, TransitionStackClass))
				{
					InitTransitionStack();
				}
				else if (Name == GET_MEMBER_NAME_CHECKED(FTransitionStackContainer, NodeStackInstanceTemplate))
				{
					// User defined setting of the transition, reformat not needed.
					bCanReformatGraph = false;
				}
			}
		}
		else if (DirectPropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, TransitionClass) ||
			DirectPropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, bAutoFormatGraph) ||
			DirectPropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, bNOTPrimaryCondition))
		{
			bCanReformatGraph = bAutoFormatGraph;
		}

		// Always reformat when any stack property has changed.
		if (bCanReformatGraph)
		{
			FormatGraphForStackNodes();
		}
		else
		{
			// Always remove unused stack instance nodes or they'll cause a compiler error.
			RemoveUnusedStackInstanceNodes();
			AddNewStackInstanceNodes();
		}
	}

	CopyToRoutedTransitions();
	UpdatePrimaryTransition(false);

	bChangingProperty = false;
}

void USMGraphNode_TransitionEdge::PostEditUndo()
{
	Super::PostEditUndo();
	UpdatePrimaryTransition();
}

void USMGraphNode_TransitionEdge::DestroyNode()
{
	Modify();

	const bool bIsPrimaryTransition = IsPrimaryReroutedTransition();
	if (BoundGraph && bIsPrimaryTransition)
	{
		BoundGraph->Modify();
		MovePrimaryTransitionToNextAvailable();
	}
	
	UEdGraph* GraphToRemove = BoundGraph;

	BoundGraph = nullptr;
	Super::DestroyNode();

	DestroyTransitionStack();

	if (GraphToRemove && bIsPrimaryTransition)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

void USMGraphNode_TransitionEdge::ReconstructNode()
{
	Super::ReconstructNode();
	RefreshTransitionDelegate();

	if (!bChangingProperty)
	{
		// Certain actions like reinitializing the template will cause this to trigger before other transitions
		// are updated from user changes and cause changes to be lost.
		UpdatePrimaryTransition();
	}
}

UObject* USMGraphNode_TransitionEdge::GetJumpTargetForDoubleClick() const
{
	if (FSMBlueprintEditorUtils::GetEditorSettings()->TransitionDoubleClickBehavior == ESMJumpToGraphBehavior::PreferExternalGraph)
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

FSlateIcon USMGraphNode_TransitionEdge::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), TEXT("Graph.TransitionNode.Icon"));
}

void USMGraphNode_TransitionEdge::ResetDebugState()
{
	Super::ResetDebugState();

	// Prevents a previous cycle from showing it as running.
	if (const FSMTransition* DebugNode = (FSMTransition*)GetDebugNode())
	{
		DebugNode->bWasEvaluating = bWasEvaluating = false;
	}
}

void USMGraphNode_TransitionEdge::UpdateTime(float DeltaTime)
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	if (Settings->bDisplayTransitionEvaluation)
	{
		if (const FSMTransition* DebugNode = (FSMTransition*)GetDebugNode())
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

void USMGraphNode_TransitionEdge::ImportDeprecatedProperties()
{
	Super::ImportDeprecatedProperties();

	if (USMTransitionInstance* Instance = Cast<USMTransitionInstance>(GetNodeTemplate()))
	{
		Instance->bCanEvaluate = bCanEvaluate_DEPRECATED;
		Instance->SetCanEvaluateFromEvent(bCanEvaluateFromEvent_DEPRECATED);
		Instance->SetCanEvalWithStartState(bCanEvalWithStartState_DEPRECATED);
		Instance->SetPriorityOrder(PriorityOrder_DEPRECATED);
	}
}

void USMGraphNode_TransitionEdge::PlaceDefaultInstanceNodes()
{
	Super::PlaceDefaultInstanceNodes();

	USMGraphK2Node_TransitionResultNode* ResultNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphK2Node_TransitionResultNode>(BoundGraph);
	USMGraphK2Node_TransitionInstance_CanEnterTransition* InstanceCanEnterTransition = nullptr;
	if (FSMBlueprintEditorUtils::PlaceNodeIfNotSet<USMGraphK2Node_TransitionInstance_CanEnterTransition>(BoundGraph, ResultNode,
		&InstanceCanEnterTransition, EGPD_Input, HasValidTransitionStack() ? -750 : -550))
	{
		// Pin names won't match correctly so manually wire.
		InstanceCanEnterTransition->GetSchema()->TryCreateConnection(InstanceCanEnterTransition->FindPin(UEdGraphSchema_K2::PN_ReturnValue), ResultNode->GetInputPin());
	}

	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionEnteredNode, USMGraphK2Node_TransitionInstance_OnTransitionTaken>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionInitializedNode, USMGraphK2Node_TransitionInstance_OnTransitionInitialized>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionShutdownNode, USMGraphK2Node_TransitionInstance_OnTransitionShutdown>(BoundGraph);

	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_IntermediateStateMachineStartNode, USMGraphK2Node_StateInstance_StateMachineStart>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_IntermediateStateMachineStopNode, USMGraphK2Node_StateInstance_StateMachineStop>(BoundGraph);
}

FLinearColor USMGraphNode_TransitionEdge::GetBackgroundColor() const
{
	if (const USMGraphNode_RerouteNode* PrevReroute = GetPreviousRerouteNode())
	{
		if (const USMGraphNode_TransitionEdge* PrevTransition = PrevReroute->GetPreviousTransition())
		{
			return PrevTransition->GetBackgroundColor();
		}
	}

	const FLinearColor BaseColor = Super::GetBackgroundColor();
	
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	if (Settings->bDisplayTransitionEvaluation)
	{
		if (const FSMTransition* DebugNode = (FSMTransition*)GetDebugNode())
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

	return BaseColor;
}

FLinearColor USMGraphNode_TransitionEdge::GetActiveBackgroundColor() const
{
	return FSMBlueprintEditorUtils::GetEditorSettings()->ActiveTransitionColor;
}

void USMGraphNode_TransitionEdge::SetNodeClass(UClass* Class)
{
	TransitionClass = Class;
	Super::SetNodeClass(Class);
}

float USMGraphNode_TransitionEdge::GetMaxDebugTime() const
{
	return FSMBlueprintEditorUtils::GetEditorSettings()->TimeToFadeLastActiveTransition;
}

bool USMGraphNode_TransitionEdge::IsDebugNodeActive() const
{
	if (const USMGraphNode_RerouteNode* PrevReroute = GetPreviousRerouteNode())
	{
		if (const USMGraphNode_TransitionEdge* PrevTransition = PrevReroute->GetPreviousTransition())
		{
			return PrevTransition->IsDebugNodeActive();
		}
	}
	return Super::IsDebugNodeActive();
}

bool USMGraphNode_TransitionEdge::WasDebugNodeActive() const
{
	if (const USMGraphNode_RerouteNode* PrevReroute = GetPreviousRerouteNode())
	{
		if (const USMGraphNode_TransitionEdge* PrevTransition = PrevReroute->GetPreviousTransition())
		{
			return PrevTransition->WasDebugNodeActive();
		}
	}

	return Super::WasDebugNodeActive();
}

void USMGraphNode_TransitionEdge::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::PreCompile(CompilerContext);

	const USMGraphNode_StateNodeBase* NextState = GetToState();
	if (!NextState || NextState->IsA<USMGraphNode_RerouteNode>())
	{
		CompilerContext.MessageLog.Error(TEXT("Transition @@ has no Next State. This could be due to a disconnected reroute node."), this);
	}

	const USMGraphNode_StateNodeBase* FromState = GetFromState();
	if (!FromState || FromState->IsA<USMGraphNode_RerouteNode>())
	{
		CompilerContext.MessageLog.Error(TEXT("Transition @@ has no Previous State. This could be due to a disconnected reroute node."), this);
	}

	if (!DelegatePropertyName.IsNone())
	{
		if (UClass* DelegateClass = GetSelectedDelegateOwnerClass())
		{
			if (DelegateClass->FindPropertyByName(DelegatePropertyName) == nullptr)
			{
				// The delegate cannot be found, check to see if it was renamed.
				
				if (USMGraphK2Node_FunctionNode_TransitionEvent* TransitionEvent = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(BoundGraph))
				{
					UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
					bool bRequiresDelegateRefresh = false;
					
					FString NewDelegateName;
					if (FProperty* RemappedProperty = FSMBlueprintEditorUtils::GetPropertyByName(Blueprint, DelegatePropertyName))
					{
						NewDelegateName = RemappedProperty->GetName();
					}
					else if (UFunction* Function = TransitionEvent->GetDelegateFunction())
					{
						NewDelegateName = Function->GetName();
						NewDelegateName.RemoveFromEnd(TEXT("__DelegateSignature"));
					}
					else if (DelegatePropertyGuid.IsValid())
					{
						// Attempt a guid lookup if there is one saved. This can happen if the variable was renamed once,
						// but this owning blueprint wasn't saved, and the variable was renamed again.
						
						FBPVariableDescription VariableDescription;
						if (FSMBlueprintEditorUtils::TryGetVariableByGuid(Blueprint, DelegatePropertyGuid, VariableDescription))
						{
							NewDelegateName = VariableDescription.VarName.ToString();
							bRequiresDelegateRefresh = true;
						}
					}
					
					if (!NewDelegateName.IsEmpty())
					{
						const FString OldDelegateName = DelegatePropertyName.ToString();

						DelegatePropertyName = *NewDelegateName;
						TransitionEvent->DelegatePropertyName = *NewDelegateName;

						if (OldDelegateName != NewDelegateName)
						{
							// NewDelegateName cannot be refreshed on first compile in some situations, only display the message when it's been updated.
							
							FText const InfoFormat = LOCTEXT("EventDelegateRename", "Event delegate '{0}' has been renamed to '{1}' on transition @@.");
							CompilerContext.MessageLog.Note(*FText::Format(InfoFormat, FText::FromString(OldDelegateName), FText::FromString(NewDelegateName)).ToString(), this);
						}
						
						if (bRequiresDelegateRefresh)
						{
							RefreshTransitionDelegate();
						}
						
						return;
					}
				}
				
				CompilerContext.MessageLog.Error(TEXT("Delegate property not found for transition @@."), this);
			}
			else
			{
				RefreshTransitionDelegate();
			}
		}
	}
}

void USMGraphNode_TransitionEdge::PreCompileNodeInstanceValidation(FCompilerResultsLog& CompilerContext,
	USMCompilerLog* CompilerLog, USMGraphNode_Base* OwningNode)
{
	if (IsPrimaryReroutedTransition())
	{
		Super::PreCompileNodeInstanceValidation(CompilerContext, CompilerLog, OwningNode);

		const TArray<FTransitionStackContainer>& Templates = GetAllNodeStackTemplates();
		for (const FTransitionStackContainer& Template : Templates)
		{
			RunPreCompileValidateForNodeInstance(Template.NodeStackInstanceTemplate, CompilerLog);
		}
	}
}

void USMGraphNode_TransitionEdge::OnCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::OnCompile(CompilerContext);

	const TArray<FTransitionStackContainer>& Templates = GetAllNodeStackTemplates();

	if (Templates.Num() > 0)
	{
		const FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(BoundGraph);
		check(RuntimeNode);

		for (const FTransitionStackContainer& Template : Templates)
		{
			if (Template.NodeStackInstanceTemplate && GetDefaultNodeClass() != Template.TransitionStackClass)
			{
				CompilerContext.AddDefaultObjectTemplate(RuntimeNode->GetNodeGuid(), Template.NodeStackInstanceTemplate, FTemplateContainer::StackTemplate, Template.TemplateGuid);
			}
		}
	}
}

bool USMGraphNode_TransitionEdge::AreTemplatesFullyLoaded() const
{
	if (!Super::AreTemplatesFullyLoaded())
	{
		return false;
	}

	for (const FTransitionStackContainer& Stack : TransitionStack)
	{
		if (!Stack.NodeStackInstanceTemplate || Stack.NodeStackInstanceTemplate->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
		{
			return false;
		}
	}

	return true;
}

bool USMGraphNode_TransitionEdge::CanRunConstructionScripts() const
{
	return IsPrimaryReroutedTransition();
}

bool USMGraphNode_TransitionEdge::DoesNodePossiblyHaveConstructionScripts() const
{
	if (Super::DoesNodePossiblyHaveConstructionScripts())
	{
		return true;
	}

	for (const FTransitionStackContainer& Stack : TransitionStack)
	{
		if (Stack.NodeStackInstanceTemplate && Stack.NodeStackInstanceTemplate->bHasEditorConstructionScripts)
		{
			return true;
		}
	}

	return false;
}

void USMGraphNode_TransitionEdge::RunAllConstructionScripts_Internal()
{
	Super::RunAllConstructionScripts_Internal();

	if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		for (const FTransitionStackContainer& Stack : TransitionStack)
		{
			if (Stack.NodeStackInstanceTemplate)
			{
				Stack.NodeStackInstanceTemplate->RunConstructionScript();
			}
		}
	}
}

void USMGraphNode_TransitionEdge::RestoreArchetypeValuesPriorToConstruction()
{
	Super::RestoreArchetypeValuesPriorToConstruction();
	for (const FTransitionStackContainer& Stack : TransitionStack)
	{
		if (Stack.NodeStackInstanceTemplate)
		{
			Stack.NodeStackInstanceTemplate->RestoreArchetypeValuesPriorToConstruction();
		}
	}
}

const FSlateBrush* USMGraphNode_TransitionEdge::GetNodeIcon() const
{
	if (const FSlateBrush* Icon = Super::GetNodeIcon())
	{
		return Icon;
	}

	return FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.TransitionNode.Icon"));
}

void USMGraphNode_TransitionEdge::OnBoundGraphRenamed(UObject* OldOuter, const FName OldName)
{
	Super::OnBoundGraphRenamed(OldOuter, OldName);

	Modify();
	bUsingCustomName = GetTransitionName() != GetDefaultTransitionName();
}

TArray<TSubclassOf<USMGraphNode_Base>> USMGraphNode_TransitionEdge::GetAllowedDuplicateNodeNameTypes() const
{
	return { USMGraphNode_TransitionEdge::StaticClass(), USMGraphNode_RerouteNode::StaticClass() };
}

void USMGraphNode_TransitionEdge::OnConvertToCurrentVersion(bool bOnlyOnLoad)
{
	Super::OnConvertToCurrentVersion(bOnlyOnLoad);

	if ((!IsTemplate() && GetLinker() && GetLinker()->IsPersistent() && GetLinker()->IsLoading()) || !bOnlyOnLoad)
	{
		if (GetLoadedVersion() < TRANSITION_NAMES_VERSION && !bUsingCustomName && GetTransitionName() != GetDefaultTransitionName())
		{
			// 2.8 -- The bound graph should be renamed to the transition default name.
			// This can't go under PostLoad /w CustomVersion because the linker will become detached on related sub-objects during a rename.
			UpdatePrimaryTransition();
		}
	}
}

void USMGraphNode_TransitionEdge::ResetNodeName()
{
	if (USMGraphNode_TransitionEdge* PrimaryTransition = GetPrimaryReroutedTransition())
	{
		FScopedTransaction Transaction(LOCTEXT("ResetNodeName", "Reset Node Name"));
	
		PrimaryTransition->Modify();
		PrimaryTransition->bUsingCustomName = false;
		PrimaryTransition->UpdatePrimaryTransition();

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(PrimaryTransition);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// Don't call super and read node template values -- transitions consider any name other than the system generated to be customized.
}

void USMGraphNode_TransitionEdge::CreateBoundGraph()
{
	// Create a new transition graph
	check(BoundGraph == nullptr);

	BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
		this,
		NAME_None,
		USMTransitionGraph::StaticClass(),
		USMTransitionGraphSchema::StaticClass());
	check(BoundGraph);

	// Use the default transition name initially
	FEdGraphUtilities::RenameGraphToNameOrCloseToName(BoundGraph, GetDefaultTransitionName());

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
}

void USMGraphNode_TransitionEdge::DuplicateBoundGraph()
{
	// Create a new transition graph
	check(BoundGraph != nullptr);

	BoundGraph = CastChecked<UEdGraph>(StaticDuplicateObject(BoundGraph, this));

	// Find an interesting name
	FEdGraphUtilities::RenameGraphToNameOrCloseToName(BoundGraph, GetTransitionName());

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();

	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}
}

void USMGraphNode_TransitionEdge::SetBoundGraph(UEdGraph* Graph)
{
	BoundGraph = Graph;
}

FLinearColor USMGraphNode_TransitionEdge::GetTransitionColor(bool bIsHovered) const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();

	const FLinearColor HoverColor = Settings->TransitionHoverColor;
	const FLinearColor BaseColor = GetBackgroundColor();

	return bIsHovered ? BaseColor * HoverColor : BaseColor;
}

const FSlateBrush* USMGraphNode_TransitionEdge::GetTransitionIcon(int32 InIndex)
{
	// Base node.
	if (InIndex < 0)
	{
		return GetNodeIcon();
	}

	// Transition stack.
	if (InIndex >= 0 && InIndex < TransitionStack.Num())
	{
		FTransitionStackContainer& StackElement = TransitionStack[InIndex];
		if (StackElement.NodeStackInstanceTemplate)
		{
			if (StackElement.NodeStackInstanceTemplate->HasCustomIcon())
			{
				UTexture2D* Texture = StackElement.NodeStackInstanceTemplate->GetNodeIcon();
				const FString TextureName = Texture ? Texture->GetFullName() : FString();
				const FVector2D Size = StackElement.NodeStackInstanceTemplate->GetNodeIconSize();
				const FLinearColor TintColor = StackElement.NodeStackInstanceTemplate->GetNodeIconTintColor();
				if (StackElement.CachedTexture != TextureName || StackElement.CachedTextureSize != Size || StackElement.CachedNodeTintColor != TintColor)
				{
					StackElement.CachedTexture = TextureName;
					StackElement.CachedTextureSize = Size;
					StackElement.CachedNodeTintColor = TintColor;
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
				
					StackElement.CachedBrush = Brush;
				}

				return &StackElement.CachedBrush;
			}
		}
	}

	return nullptr;
}

UClass* USMGraphNode_TransitionEdge::GetSelectedDelegateOwnerClass() const
{
	if (DelegateOwnerInstance == SMDO_This)
	{
		return FBlueprintEditorUtils::FindBlueprintForNodeChecked(this)->SkeletonGeneratedClass;
	}
	
	if (DelegateOwnerInstance == SMDO_PreviousState)
	{
		if (USMGraphNode_StateNodeBase* PreviousState = GetFromState())
		{
			if (UClass* NodeClass = PreviousState->GetNodeClass())
			{
				if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(NodeClass))
				{
					return Blueprint->SkeletonGeneratedClass;
				}

				return NodeClass;
			}
		}
	}
	
	if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(DelegateOwnerClass))
	{
		if (UBlueprint* BP = Cast<UBlueprint>(BPGC->ClassGeneratedBy))
		{
			return BP->SkeletonGeneratedClass;
		}
	}

	return DelegateOwnerClass;
}

void USMGraphNode_TransitionEdge::GoToTransitionEventNode()
{
	if (USMGraphK2Node_FunctionNode_TransitionEvent* PreviousEventNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(BoundGraph))
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(PreviousEventNode);
	}
}

void USMGraphNode_TransitionEdge::InitTransitionDelegate()
{
	if (!BoundGraph)
	{
		return;
	}

	// Backup existing.
	FVector2D PreviousEntryPosition;
	bool bHadPreviousNodes = false;
	UEdGraphPin* PreviousThenPin = nullptr;
	TArray<USMGraphK2Node_FunctionNode_TransitionEvent*> PreviousEventNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(BoundGraph, PreviousEventNodes);
	for (USMGraphK2Node_FunctionNode_TransitionEvent* EventNode : PreviousEventNodes)
	{
		bHadPreviousNodes = true;
		PreviousEntryPosition = FVector2D(EventNode->NodePosX, EventNode->NodePosY);
		PreviousThenPin = EventNode->GetThenPin();
	}

	// Create new.
	if (FMulticastDelegateProperty* DelegateProperty = FSMBlueprintEditorUtils::GetDelegateProperty(DelegatePropertyName, GetSelectedDelegateOwnerClass()))
	{
		// Event entry node.
		BoundGraph->Modify();
		FGraphNodeCreator<USMGraphK2Node_FunctionNode_TransitionEvent> NodeCreator(*BoundGraph);
		USMGraphK2Node_FunctionNode_TransitionEvent* OurEventNode = NodeCreator.CreateNode();
		const FVector2D Position = bHadPreviousNodes ? PreviousEntryPosition : BoundGraph->GetGoodPlaceForNewNode();
		OurEventNode->NodePosX = Position.X;
		OurEventNode->NodePosY = Position.Y;
		OurEventNode->SetEventReferenceFromDelegate(DelegateProperty, DelegateOwnerInstance);
		OurEventNode->TransitionClass = TransitionClass;
		NodeCreator.Finalize();
		if (PreviousThenPin)
		{
			OurEventNode->GetThenPin()->CopyPersistentDataFromOldPin(*PreviousThenPin);
		}

		// Create return node if it doesn't exist.
		TArray<USMGraphK2Node_StateWriteNode_TransitionEventReturn*> ResultNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_StateWriteNode_TransitionEventReturn>(BoundGraph, ResultNodes);

		if (!bHadPreviousNodes || ResultNodes.Num() == 0)
		{
			FGraphNodeCreator<USMGraphK2Node_StateWriteNode_TransitionEventReturn> NodeReturnCreator(*BoundGraph);
			USMGraphK2Node_StateWriteNode_TransitionEventReturn* OurReturnNode = NodeReturnCreator.CreateNode();
			OurReturnNode->NodePosX = OurEventNode->NodePosX + OurEventNode->NodeWidth + 450;
			OurReturnNode->NodePosY = OurEventNode->NodePosY;
			NodeReturnCreator.Finalize();

			OurReturnNode->GetSchema()->TryCreateConnection(OurEventNode->GetOutputPin(), OurReturnNode->GetExecPin());
		}
	}

	// Clear existing.
	for (USMGraphK2Node_FunctionNode_TransitionEvent* EventNode : PreviousEventNodes)
	{
		FSMBlueprintEditorUtils::RemoveNode(FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this), EventNode);
	}

	UpdateTransitionDelegateGuid();
}

void USMGraphNode_TransitionEdge::SetupDelegateDefaults()
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
	DelegateOwnerClass = Blueprint->SkeletonGeneratedClass;
}

void USMGraphNode_TransitionEdge::RefreshTransitionDelegate()
{
	DelegatePropertyGuid.Invalidate();
	
	if (DelegatePropertyName.IsNone() || BoundGraph == nullptr)
	{
		// BoundGraph can be nullptr on copy paste without both states connected.
		return;
	}
	
	if (FMulticastDelegateProperty* DelegateProperty = FSMBlueprintEditorUtils::GetDelegateProperty(DelegatePropertyName, GetSelectedDelegateOwnerClass()))
	{
		UpdateTransitionDelegateGuid();
		
		TArray<USMGraphK2Node_FunctionNode_TransitionEvent*> PreviousEventNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(BoundGraph, PreviousEventNodes);

		for (USMGraphK2Node_FunctionNode_TransitionEvent* EventNode : PreviousEventNodes)
		{
			EventNode->TransitionClass = TransitionClass;
			EventNode->SetEventReferenceFromDelegate(DelegateProperty, DelegateOwnerInstance);
		}
	}
}

void USMGraphNode_TransitionEdge::UpdateTransitionDelegateGuid()
{
	DelegatePropertyGuid.Invalidate();
	
	UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
	FBPVariableDescription VariableOut;
	if (FSMBlueprintEditorUtils::TryGetVariableByName(Blueprint, DelegatePropertyName, VariableOut))
	{
		DelegatePropertyGuid = VariableOut.VarGuid;
	}
}

void USMGraphNode_TransitionEdge::UpdateResultNodeEventSettings()
{
	TArray<USMGraphK2Node_StateWriteNode_TransitionEventReturn*> ResultNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_StateWriteNode_TransitionEventReturn>(BoundGraph, ResultNodes);

	for (USMGraphK2Node_StateWriteNode_TransitionEventReturn* ResultNode : ResultNodes)
	{
		if (ResultNode->bUseOwningTransitionSettings)
		{
			ResultNode->bEventTriggersTargetedUpdate = bEventTriggersTargetedUpdate;
			ResultNode->bEventTriggersFullUpdate = bEventTriggersFullUpdate;
		}
	}
}

FString USMGraphNode_TransitionEdge::GetTransitionName() const
{
	return GetNodeName();
}

FString USMGraphNode_TransitionEdge::GetDefaultTransitionName() const
{
	const USMGraphNode_StateNodeBase* State1 = GetFromState();
	const USMGraphNode_StateNodeBase* State2 = GetToState();

	const FString State1Name = State1 && !State1->IsA<USMGraphNode_RerouteNode>() ? State1->GetStateName() : TEXT("Null");
	FString State2Name = State2 && !State2->IsA<USMGraphNode_RerouteNode>() ? State2->GetStateName() : TEXT("Null");
	
	if (const USMGraphNode_LinkStateNode* LinkStateNode = Cast<USMGraphNode_LinkStateNode>(State2))
	{
		// This is the final name that is used at run-time, so just use this now so it's not confusing for the user.
		State2Name = LinkStateNode->GetLinkedState() != nullptr ? LinkStateNode->LinkedStateName : TEXT("Null");
	}

	return FString::Printf(TEXT("%s to %s"), *State1Name, *State2Name);
}

void USMGraphNode_TransitionEdge::CreateConnections(USMGraphNode_StateNodeBase* Start, USMGraphNode_StateNodeBase* End)
{
	Pins[0]->Modify();
	Pins[0]->LinkedTo.Empty();

	Start->GetOutputPin()->Modify();
	Pins[0]->MakeLinkTo(Start->GetOutputPin());

	// This to next
	Pins[1]->Modify();
	Pins[1]->LinkedTo.Empty();

	End->GetInputPin()->Modify();
	Pins[1]->MakeLinkTo(End->GetInputPin());

	SetDefaultsWhenPlaced();
}

bool USMGraphNode_TransitionEdge::PossibleToTransition() const
{
	if (USMTransitionGraph* Graph = Cast<USMTransitionGraph>(BoundGraph))
	{
		return Graph->HasAnyLogicConnections();
	}

	return false;
}

USMTransitionGraph* USMGraphNode_TransitionEdge::GetTransitionGraph() const
{
	return CastChecked<USMTransitionGraph>(BoundGraph);
}

USMGraphNode_StateNodeBase* USMGraphNode_TransitionEdge::GetFromState(bool bIncludeReroute) const
{
	if (!bIncludeReroute)
	{
		if (const USMGraphNode_RerouteNode* PrevRerouteNode = GetPreviousRerouteNode())
		{
			if (const USMGraphNode_TransitionEdge* PrevTransition = PrevRerouteNode->GetPreviousTransition())
			{
				return PrevTransition->GetFromState(bIncludeReroute);
			}
		}
	}

	if (Pins.Num() && Pins[0]->LinkedTo.Num() > 0)
	{
		USMGraphNode_StateNodeBase* PreviousNode = Cast<USMGraphNode_StateNodeBase>(Pins[0]->LinkedTo[0]->GetOwningNode());
		return PreviousNode;
	}

	return nullptr;
}

USMGraphNode_StateNodeBase* USMGraphNode_TransitionEdge::GetToState(bool bIncludeReroute) const
{
	if (!bIncludeReroute)
	{
		if (const USMGraphNode_RerouteNode* NextRerouteNode = GetNextRerouteNode())
		{
			if (const USMGraphNode_TransitionEdge* NextTransition = NextRerouteNode->GetNextTransition())
			{
				return NextTransition->GetToState(bIncludeReroute);
			}
		}
	}

	if (Pins.Num() > 1 && Pins[1]->LinkedTo.Num() > 0)
	{
		USMGraphNode_StateNodeBase* NextNode = Cast<USMGraphNode_StateNodeBase>(Pins[1]->LinkedTo[0]->GetOwningNode());
		return NextNode;
	}

	return nullptr;
}

USMGraphNode_RerouteNode* USMGraphNode_TransitionEdge::GetPreviousRerouteNode() const
{
	if (Pins.Num() && Pins[0]->LinkedTo.Num() > 0)
	{
		return Cast<USMGraphNode_RerouteNode>(Pins[0]->LinkedTo[0]->GetOwningNode());
	}

	return nullptr;
}

USMGraphNode_RerouteNode* USMGraphNode_TransitionEdge::GetNextRerouteNode() const
{
	if (Pins.Num() > 1 && Pins[1]->LinkedTo.Num() > 0)
	{
		return Cast<USMGraphNode_RerouteNode>(Pins[1]->LinkedTo[0]->GetOwningNode());
	}

	return nullptr;
}

USMGraphNode_TransitionEdge* USMGraphNode_TransitionEdge::GetPrimaryReroutedTransition()
{
	if (IsPrimaryReroutedTransition())
	{
		return this;
	}

	const USMGraphNode_RerouteNode* PrevRerouteNode = GetPreviousRerouteNode();
	USMGraphNode_TransitionEdge* PrimaryTransition = nullptr;
	while (PrevRerouteNode)
	{
		if (USMGraphNode_TransitionEdge* PrevTransition = PrevRerouteNode->GetPreviousTransition())
		{
			PrevRerouteNode = PrevTransition->GetPreviousRerouteNode();
			if (PrevTransition->IsPrimaryReroutedTransition())
			{
				PrimaryTransition = PrevTransition;
				break;
			}
		}
		else
		{
			break;
		}
	}

	if (PrimaryTransition == nullptr)
	{
		const USMGraphNode_RerouteNode* NextRerouteNode = GetNextRerouteNode();
		while (NextRerouteNode)
		{
			if (USMGraphNode_TransitionEdge* NextTransition = NextRerouteNode->GetNextTransition())
			{
				NextRerouteNode = NextTransition->GetNextRerouteNode();
				if (NextTransition->IsPrimaryReroutedTransition())
				{
					PrimaryTransition = NextTransition;
				}
			}
			else
			{
				break;
			}
		}
	}

	return PrimaryTransition;
}

USMGraphNode_TransitionEdge* USMGraphNode_TransitionEdge::GetFirstReroutedTransition()
{
	USMGraphNode_TransitionEdge* FirstTransition = this;

	const USMGraphNode_RerouteNode* PrevRerouteNode = GetPreviousRerouteNode();
	while (PrevRerouteNode)
	{
		if (USMGraphNode_TransitionEdge* PrevTransition = PrevRerouteNode->GetPreviousTransition())
		{
			PrevRerouteNode = PrevTransition->GetPreviousRerouteNode();
			FirstTransition = PrevTransition;
		}
		else
		{
			break;
		}
	}

	return FirstTransition;
}

USMGraphNode_TransitionEdge* USMGraphNode_TransitionEdge::GetLastReroutedTransition()
{
	USMGraphNode_TransitionEdge* LastTransition = this;

	const USMGraphNode_RerouteNode* NextRerouteNode = GetNextRerouteNode();
	while (NextRerouteNode)
	{
		if (USMGraphNode_TransitionEdge* NextTransition = NextRerouteNode->GetNextTransition())
		{
			NextRerouteNode = NextTransition->GetNextRerouteNode();
			LastTransition = NextTransition;
		}
		else
		{
			break;
		}
	}

	return LastTransition;
}

bool USMGraphNode_TransitionEdge::IsPrimaryReroutedTransition() const
{
	// Bound graph can be invalid if this is a rerouted transition in the process of being destroyed.
	return BoundGraph && BoundGraph->GetOuter() == this;
}

bool USMGraphNode_TransitionEdge::IsConnectedToRerouteNode(const USMGraphNode_RerouteNode* RerouteNode)
{
	TArray<USMGraphNode_TransitionEdge*> Transitions;
	GetAllReroutedTransitions(Transitions);

	for (const USMGraphNode_TransitionEdge* Transition : Transitions)
	{
		if (Transition->GetToState(true) == RerouteNode)
		{
			return true;
		}
	}

	return false;
}

void USMGraphNode_TransitionEdge::GetAllReroutedTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions,
	TArray<USMGraphNode_RerouteNode*>& OutRerouteNodes)
{
	OutTransitions.Reset();
	OutRerouteNodes.Reset();

	USMGraphNode_RerouteNode* PrevRerouteNode = GetPreviousRerouteNode();
	while (PrevRerouteNode)
	{
		OutRerouteNodes.Insert(PrevRerouteNode, 0);
		if (USMGraphNode_TransitionEdge* PrevTransition = PrevRerouteNode->GetPreviousTransition())
		{
			PrevRerouteNode = PrevTransition->GetPreviousRerouteNode();
			OutTransitions.Insert(PrevTransition, 0);
		}
		else
		{
			break;
		}
	}

	OutTransitions.Add(this);

	USMGraphNode_RerouteNode* NextRerouteNode = GetNextRerouteNode();
	while (NextRerouteNode)
	{
		OutRerouteNodes.Add(NextRerouteNode);

		if (USMGraphNode_TransitionEdge* NextTransition = NextRerouteNode->GetNextTransition())
		{
			NextRerouteNode = NextTransition->GetNextRerouteNode();
			OutTransitions.Add(NextTransition);
		}
		else
		{
			break;
		}
	}
}

void USMGraphNode_TransitionEdge::GetAllReroutedTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions)
{
	TArray<USMGraphNode_RerouteNode*> RerouteNodes;
	GetAllReroutedTransitions(OutTransitions, RerouteNodes);
}

void USMGraphNode_TransitionEdge::UpdatePrimaryTransition(bool bCopySettingsFromPrimary)
{
	USMGraphNode_TransitionEdge* PrimaryTransition = GetPrimaryReroutedTransition();
	TArray<USMGraphNode_TransitionEdge*> ReroutedTransitions;
	GetAllReroutedTransitions(ReroutedTransitions);

	TObjectPtr<UEdGraph> PrimaryGraph = nullptr;

	if (PrimaryTransition)
	{
		if (ReroutedTransitions.Num() > 0)
		{
			PrimaryGraph = PrimaryTransition->GetBoundGraph();

			if (ReroutedTransitions[0] != PrimaryTransition)
			{
				PrimaryGraph->Rename(nullptr, ReroutedTransitions[0], REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			}
		}
	}
	else if (BoundGraph)
	{
		// There isn't a primary transition but we're pointing to a graph. Perhaps this transition was moved from the chain
		// such as through a collapse or copy paste.

		PrimaryTransition = this;
		DuplicateBoundGraph();
		PrimaryGraph = BoundGraph;
	}

	if (PrimaryTransition && !PrimaryTransition->bUsingCustomName)
	{
		FEdGraphUtilities::RenameGraphToNameOrCloseToName(PrimaryGraph ? PrimaryGraph : BoundGraph, GetDefaultTransitionName());
	}

	if (bCopySettingsFromPrimary && PrimaryTransition)
	{
		if (PrimaryGraph)
		{
			for (USMGraphNode_TransitionEdge* Transition : ReroutedTransitions)
			{
				// Not handled under CopyToRoutedTransitions.
				Transition->SetBoundGraph(PrimaryGraph);
			}
		}

		PrimaryTransition->CopyToRoutedTransitions();
	}
}

void USMGraphNode_TransitionEdge::DestroyReroutedTransitions()
{
	if (!IsRerouted())
	{
		return;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);

	TArray<USMGraphNode_TransitionEdge*> ReroutedTransitions;
	TArray<USMGraphNode_RerouteNode*> RerouteNodes;
	GetAllReroutedTransitions(ReroutedTransitions, RerouteNodes);

	for (USMGraphNode_TransitionEdge* ReroutedTransition : ReroutedTransitions)
	{
		if (IsValid(ReroutedTransition) && ReroutedTransition != this)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, ReroutedTransition, true);
		}
	}
	for (USMGraphNode_RerouteNode* RerouteNode : RerouteNodes)
	{
		if (IsValid(RerouteNode))
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, RerouteNode, true);
		}
	}
}

bool USMGraphNode_TransitionEdge::ShouldRunParallel() const
{
	if (USMTransitionInstance* Instance = GetNodeTemplateAs<USMTransitionInstance>())
	{
		return Instance->GetRunParallel();
	}

	return false;
}

bool USMGraphNode_TransitionEdge::WasEvaluating() const
{
	if (const USMGraphNode_RerouteNode* PrevReroute = GetPreviousRerouteNode())
	{
		if (const USMGraphNode_TransitionEdge* PrevTransition = PrevReroute->GetPreviousTransition())
		{
			return PrevTransition->WasEvaluating();
		}
	}

	return bWasEvaluating;
}

bool USMGraphNode_TransitionEdge::IsFromAnyState() const
{
	if (USMGraphNode_AnyStateNode* AnyState = Cast<USMGraphNode_AnyStateNode>(GetFromState()))
	{
		return true;
	}

	return bFromAnyState;
}

bool USMGraphNode_TransitionEdge::IsFromLinkState() const
{
	if (USMGraphNode_LinkStateNode* LinkState = Cast<USMGraphNode_LinkStateNode>(GetFromState()))
	{
		return true;
	}

	return bFromLinkState;
}

bool USMGraphNode_TransitionEdge::IsFromRerouteNode() const
{
	return GetPreviousRerouteNode() != nullptr;
}

void USMGraphNode_TransitionEdge::CopyToRoutedTransitions()
{
	if (IsRerouted())
	{
		TArray<USMGraphNode_TransitionEdge*> ReroutedTransitions;
		GetAllReroutedTransitions(ReroutedTransitions);

		for (USMGraphNode_TransitionEdge* Transition : ReroutedTransitions)
		{
			if (Transition != this)
			{
				Transition->CopyFrom(*this);
			}
		}
	}
}

bool USMGraphNode_TransitionEdge::MovePrimaryTransitionToNextAvailable()
{
	if (IsRerouted() && IsPrimaryReroutedTransition())
	{
		TArray<USMGraphNode_TransitionEdge*> ReroutedTransitions;
		GetAllReroutedTransitions(ReroutedTransitions);

		for (USMGraphNode_TransitionEdge* Transition : ReroutedTransitions)
		{
			if (Transition == this)
			{
				continue;
			}

			if (!ensure(!Transition->IsPrimaryReroutedTransition()))
			{
				break;
			}

			BoundGraph->Rename(nullptr, Transition, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			Transition->SetBoundGraph(BoundGraph);
			BoundGraph = nullptr;
			return true;
		}
	}

	return false;
}

UEdGraphPin* USMGraphNode_TransitionEdge::GetLinearExpressionPin() const
{
	if (const USMTransitionGraph* TransitionGraph = GetTransitionGraph())
	{
		return TransitionGraph->ResultNode->GetTransitionEvaluationPin();
	}

	return nullptr;
}

const TArray<FTransitionStackContainer>& USMGraphNode_TransitionEdge::GetAllNodeStackTemplates() const
{
	return TransitionStack;
}

int32 USMGraphNode_TransitionEdge::GetIndexOfTemplate(const FGuid& TemplateGuid) const
{
	for (int32 Idx = 0; Idx < TransitionStack.Num(); ++Idx)
	{
		if (TransitionStack[Idx].TemplateGuid == TemplateGuid)
		{
			return Idx;
		}
	}

	return INDEX_NONE;
}

void USMGraphNode_TransitionEdge::GetAllNodeTemplates(TArray<USMNodeInstance*>& OutNodeInstances) const
{
	Super::GetAllNodeTemplates(OutNodeInstances);
	for (const FTransitionStackContainer& Stack : GetAllNodeStackTemplates())
	{
		OutNodeInstances.Add(Stack.NodeStackInstanceTemplate);
	}
}

USMNodeInstance* USMGraphNode_TransitionEdge::GetTemplateFromIndex(int32 Index) const
{
	if (Index >= 0 && Index < TransitionStack.Num())
	{
		return TransitionStack[Index].NodeStackInstanceTemplate;
	}

	return nullptr;
}

USMNodeInstance* USMGraphNode_TransitionEdge::GetTemplateFromGuid(const FGuid& TemplateGuid) const
{
	const int32 Index = GetIndexOfTemplate(TemplateGuid);
	return GetTemplateFromIndex(Index);
}

USMNodeInstance* USMGraphNode_TransitionEdge::GetHoveredStackTemplate() const
{
	if (CachedHoveredTransitionStack)
	{
		return CachedHoveredTransitionStack;
	}
	
	for (const FTransitionStackContainer& StackContainer : TransitionStack)
	{
		if (StackContainer.IconImage.IsValid() && StackContainer.IconImage->IsDirectlyHovered())
		{
			CachedHoveredTransitionStack = StackContainer.NodeStackInstanceTemplate;
			return StackContainer.NodeStackInstanceTemplate;
		}
	}
	
	return nullptr;
}

void USMGraphNode_TransitionEdge::InitTransitionStack()
{
	for (FTransitionStackContainer& TransitionContainer : TransitionStack)
	{
		TransitionContainer.InitTemplate(this);
	}
}

void USMGraphNode_TransitionEdge::DestroyTransitionStack()
{
	for (FTransitionStackContainer& TransitionContainer : TransitionStack)
	{
		TransitionContainer.DestroyTemplate();
	}

	TransitionStack.Reset();
}

bool USMGraphNode_TransitionEdge::HasValidTransitionStack() const
{
	for (const FTransitionStackContainer& StackElement : TransitionStack)
	{
		if (StackElement.NodeStackInstanceTemplate != nullptr)
		{
			return true;
		}
	}

	return false;
}

void USMGraphNode_TransitionEdge::FormatGraphForStackNodes()
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(BoundGraph);

	UEdGraphPin* EvalPin = GetLinearExpressionPin();
	check(EvalPin);
	
	// Locate any pin the user has previously configured manually, such as a variable. We handle only the initial pin
	// of an operator node as this is initially setup from any logic connected directly to the evaluation pin.
	UEdGraphPin* UserDefinedPin = nullptr;
	{
		UEdGraphPin* PreviousInitialOperatorPin = IsValid(InitialOperatorNode) ? InitialOperatorNode->GetInputPin(0) : EvalPin;
		if (PreviousInitialOperatorPin && PreviousInitialOperatorPin->LinkedTo.Num() > 0 &&
			PreviousInitialOperatorPin->LinkedTo[0] != nullptr && PreviousInitialOperatorPin->LinkedTo[0]->GetOwningNodeUnchecked() != nullptr &&
			IsValid(PreviousInitialOperatorPin->LinkedTo[0]->GetOwningNode()))
		{
			UserDefinedPin = PreviousInitialOperatorPin->LinkedTo[0];
		}
	}

	InitialOperatorNode = nullptr;
	
	// Cleanup any previously auto generated nodes.
	for (UEdGraphNode* Node : AutoGeneratedStackNodes)
	{
		if (UserDefinedPin && Node == UserDefinedPin->GetOwningNode())
		{
			UserDefinedPin = nullptr;
			if (UK2Node_CommutativeAssociativeBinaryOperator* PossibleNOTNode = Cast<UK2Node_CommutativeAssociativeBinaryOperator>(Node))
			{
				// Check if the node being removed was an auto generated primary NOT.
				if (PossibleNOTNode->GetFunctionName() == GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool))
				{
					if (UEdGraphPin* NOTInPin = PossibleNOTNode->GetInputPin(0))
					{
						if (NOTInPin->LinkedTo.Num() > 0 && IsValid(NOTInPin->LinkedTo[0]->GetOwningNode()))
						{
							UserDefinedPin = NOTInPin->LinkedTo[0];
						}
					}
				}
			}
		}
		FSMBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
	}

	// Verify all can enter transition nodes are removed (stack versions should have been removed from above).
	{
		TArray<USMGraphK2Node_TransitionInstance_CanEnterTransition*> StackNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested(BoundGraph, StackNodes);

		for (UEdGraphNode* Node : StackNodes)
		{
			if (UserDefinedPin && Node == UserDefinedPin->GetOwningNode())
			{
				UserDefinedPin = nullptr;
			}

			// If the user has custom logic and this is the default GetNodeInstance the don't remove it. User may
			// have other custom logic hooked up to it.
			if (UserDefinedPin && Node->GetClass() == USMGraphK2Node_TransitionInstance_CanEnterTransition::StaticClass())
			{
				continue;
			}
			
			FSMBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}

	// Place main instance node if valid.
	if (!UserDefinedPin && !IsUsingDefaultNodeClass())
	{
		PlaceDefaultInstanceNodes();
	}

	// For checking index iteration for a single operator.
	int32 OperatorIndex = 0;

	auto CreateNOTNode = [this](UEdGraphPin* InFromPin, UEdGraphPin* InOperatorInputPin, int32 InNodePosX, int32 InNodePosY) -> UK2Node_CallFunction*
	{
		check(InFromPin && InOperatorInputPin);
		InOperatorInputPin->BreakAllPinLinks();
				
		FGraphNodeCreator<UK2Node_CommutativeAssociativeBinaryOperator> NOTNodeCreator(*BoundGraph);
		UK2Node_CallFunction* NOTFunctionNode = NOTNodeCreator.CreateNode();
		UFunction* NewFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool));
		check(NewFunction);
					
		NOTFunctionNode->SetFromFunction(NewFunction);
		NOTFunctionNode->NodePosX = InNodePosX;
		NOTFunctionNode->NodePosY = InNodePosY;
		NOTNodeCreator.Finalize();

		// Connect stack pin to NOT pin.
		UEdGraphPin* NOTInputPin = NOTFunctionNode->FindPin(TEXT("A"), EGPD_Input);
		check(NOTInputPin);
		InFromPin->MakeLinkTo(NOTInputPin);

		// Connect NOT pin to operator pin.
		UEdGraphPin* NOTOutputPin = NOTFunctionNode->GetReturnValuePin();
		check(NOTOutputPin);
		NOTOutputPin->MakeLinkTo(InOperatorInputPin);
					
		AutoGeneratedStackNodes.Add(NOTFunctionNode);
		return NOTFunctionNode;
	};
	
	auto GetOrCreateBinaryOperator = [this, EvalPin, &OperatorIndex](const FTransitionStackContainer& InTransitionStack,
	UK2Node_CommutativeAssociativeBinaryOperator* InPreviousOperator)
	{
		UK2Node_CommutativeAssociativeBinaryOperator* ReturnOperator = nullptr;
		bool bNeedsNewOperator = InPreviousOperator == nullptr;
		if (InPreviousOperator)
		{
			const FName PreviousFunctionName = InPreviousOperator->GetFunctionName();
			if (InTransitionStack.Mode == ESMExpressionMode::OR)
			{
				bNeedsNewOperator = PreviousFunctionName != GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR);
			}
			else if (InTransitionStack.Mode == ESMExpressionMode::AND)
			{
				bNeedsNewOperator = PreviousFunctionName != GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND);
			}
		}

		if (bNeedsNewOperator)
		{
			const int32 PreviousOperatorIndex = OperatorIndex;
			OperatorIndex = 1;
			
			FGraphNodeCreator<UK2Node_CommutativeAssociativeBinaryOperator> NodeCreator(*BoundGraph);
			UK2Node_CommutativeAssociativeBinaryOperator* NewOperator = NodeCreator.CreateNode();
			UFunction* NewFunction = nullptr;
			if (InTransitionStack.Mode == ESMExpressionMode::OR)
			{
				NewFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR));
			}
			else if (InTransitionStack.Mode == ESMExpressionMode::AND)
			{
				NewFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND));
			}

			check(NewFunction);
			NewOperator->SetFromFunction(NewFunction);

			NewOperator->NodePosX = InPreviousOperator ? InPreviousOperator->NodePosX : EvalPin->GetOwningNode()->NodePosX - EvalPin->GetOwningNode()->NodeWidth - 200;
			NewOperator->NodePosY = InPreviousOperator ? (InPreviousOperator->NodePosY + 32 + (32 * PreviousOperatorIndex)) : EvalPin->GetOwningNode()->NodePosY;
			NodeCreator.Finalize();

			ReturnOperator = NewOperator;
		}
		else
		{
			// Existing operator
			check(InPreviousOperator);
			if (++OperatorIndex > 1) // Operators have 2 pins by default.
			{
				InPreviousOperator->AddInputPin();
			}
			
			ReturnOperator = InPreviousOperator;
		}

		return ReturnOperator;
	};

	auto CreatePrimaryNOT = [&](UEdGraphPin* ToPin) -> UK2Node_CallFunction*
	{
		// Generate the NOT node for the initial condition.
		UEdGraphPin* PinToNOT = ToPin->LinkedTo.Num() > 0 ? ToPin->LinkedTo[0] : UserDefinedPin;
		if (PinToNOT)
		{
			UEdGraphPin* FromPin = PinToNOT;
			return CreateNOTNode(FromPin, ToPin,
				FromPin->GetOwningNode()->NodePosX + 175, FromPin->GetOwningNode()->NodePosY);
		}
		return nullptr;
	};
	
	if (bNOTPrimaryCondition)
	{
		if (!HasValidTransitionStack()) // Else created in stack loop.
		{
			CreatePrimaryNOT(EvalPin);
		}
	}
	else if (TransitionStack.Num() == 0 && UserDefinedPin && IsValid(UserDefinedPin->GetOwningNode()) && EvalPin->LinkedTo.Num() == 0)
	{
		// User has cleared NOT status of only the primary condition and is using no other stacks. Reconnect the user pin.
		UserDefinedPin->MakeLinkTo(EvalPin);
	}

	UK2Node_CommutativeAssociativeBinaryOperator* OperatorNode = nullptr;
	for (auto StackIt = TransitionStack.CreateIterator(); StackIt; ++StackIt)
	{ 
		if (StackIt->NodeStackInstanceTemplate)
		{
			FGraphNodeCreator<USMGraphK2Node_TransitionStackInstance_CanEnterTransition> StackCanEnterTransitionNodeCreator(*BoundGraph);
			USMGraphK2Node_TransitionStackInstance_CanEnterTransition* NewStackNode = StackCanEnterTransitionNodeCreator.CreateNode();
			NewStackNode->NodePosX = EvalPin->GetOwningNode()->NodePosX - EvalPin->GetOwningNode()->NodeWidth - 800;
			NewStackNode->NodePosY = (StackIt.GetIndex() + 1) * 64;
			StackCanEnterTransitionNodeCreator.Finalize();

			USMGraphK2Node_TransitionStackInstance_CanEnterTransition* NewGraphNode = CastChecked<USMGraphK2Node_TransitionStackInstance_CanEnterTransition>(NewStackNode);
			NewGraphNode->SetNodeStackGuid(StackIt->TemplateGuid);
			
			UK2Node_CommutativeAssociativeBinaryOperator* PreviousOperator = OperatorNode;

			OperatorNode = GetOrCreateBinaryOperator(*StackIt, OperatorNode);
			check(OperatorNode);
			
			if (StackIt.GetIndex() == 0)
			{
				// The first iteration handles the user provided condition. This could be the default CanInstanceEnterTransition
				// or custom user logic.
				UEdGraphPin* FirstInputPin = OperatorNode->GetInputPin(0);
				check(FirstInputPin);

				// Either setup with previously entered user data into the operator node, or start over with the evaluation pin.
				if (UserDefinedPin)
				{
					UserDefinedPin->MakeLinkTo(FirstInputPin);
				}
				else
				{
					FirstInputPin->CopyPersistentDataFromOldPin(*EvalPin);
				}
				
				if (StackIt->Mode == ESMExpressionMode::AND && !FirstInputPin->HasAnyConnections() && FirstInputPin->GetDefaultAsString() == TEXT("false"))
				{
					// If nothing was entered originally just default it to true otherwise this AND will never be true
					// unless the user alters it manually.
					FirstInputPin->DefaultValue = TEXT("true");
				}

				if (bNOTPrimaryCondition)
				{
					if (UK2Node_CallFunction* PrimaryNOTNode = CreatePrimaryNOT(FirstInputPin))
					{
						// Adjust x position for consistency.
						PrimaryNOTNode->NodePosX = OperatorNode->NodePosX - 175;
					}
				}
				
				InitialOperatorNode = OperatorNode;
			}
			else if (PreviousOperator && PreviousOperator != OperatorNode)
			{
				// Connect output of previous operator to first input of new operator.
				if (UEdGraphPin* PreviousOutputPin = PreviousOperator->FindOutPin())
				{
					UEdGraphPin* FirstInputPin = OperatorNode->GetInputPin(0);
					check(FirstInputPin);

					PreviousOutputPin->MakeLinkTo(FirstInputPin);
				}
			}

			// Connect the pins.
			{
				UEdGraphPin* OperatorInputPin = OperatorNode->GetInputPin(OperatorIndex);
				check(OperatorInputPin);

				if (StackIt->bNOT)
				{
					CreateNOTNode(NewGraphNode->GetReturnValuePinChecked(), OperatorInputPin,
						OperatorNode->NodePosX - 175, NewStackNode->NodePosY);
				}
				else
				{
					// Stack pin directly to operator pin.
					NewGraphNode->GetReturnValuePinChecked()->MakeLinkTo(OperatorInputPin);
				}
			}
			
			AutoGeneratedStackNodes.Add(NewStackNode);
			AutoGeneratedStackNodes.Add(OperatorNode);
		}
	}

	if (OperatorNode)
	{
		EvalPin->BreakAllPinLinks();

		if (UEdGraphPin* OperatorOutputPin = OperatorNode->FindOutPin())
		{
			OperatorOutputPin->MakeLinkTo(EvalPin);
		}
	}
}

void USMGraphNode_TransitionEdge::AddNewStackInstanceNodes()
{
	TArray<USMGraphK2Node_TransitionStackInstance_CanEnterTransition*> StackNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(BoundGraph, StackNodes);
	
	for (auto StackIt = TransitionStack.CreateIterator(); StackIt; ++StackIt)
	{
		if (StackIt->NodeStackInstanceTemplate == nullptr ||
			StackNodes.ContainsByPredicate([StackIt](const USMGraphK2Node_TransitionStackInstance_CanEnterTransition* ExistingGraphNode)
		{
			return StackIt->NodeStackInstanceTemplate->GetTemplateGuid() == ExistingGraphNode->GetNodeStackGuid();
		}))
		{
			continue;
		}
		
		FGraphNodeCreator<USMGraphK2Node_TransitionStackInstance_CanEnterTransition> StackCanEnterTransitionNodeCreator(*BoundGraph);
		USMGraphK2Node_TransitionStackInstance_CanEnterTransition* NewStackNode = StackCanEnterTransitionNodeCreator.CreateNode();
		NewStackNode->NodePosX = -750;
		NewStackNode->NodePosY = (StackIt.GetIndex() + 1) * 64;
		StackCanEnterTransitionNodeCreator.Finalize();

		USMGraphK2Node_TransitionStackInstance_CanEnterTransition* NewGraphNode = CastChecked<USMGraphK2Node_TransitionStackInstance_CanEnterTransition>(NewStackNode);
		NewGraphNode->SetNodeStackGuid(StackIt->TemplateGuid);
	}
}

void USMGraphNode_TransitionEdge::RemoveUnusedStackInstanceNodes()
{
	TArray<USMGraphK2Node_TransitionStackInstance_CanEnterTransition*> StackNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested(BoundGraph, StackNodes);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(BoundGraph);

	for (USMGraphK2Node_TransitionStackInstance_CanEnterTransition* Node : StackNodes)
	{
		const FGuid& StackGuid = Node->GetNodeStackGuid();
		if (!GetTemplateFromGuid(StackGuid))
		{
			FSMBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}
}

FLinearColor USMGraphNode_TransitionEdge::Internal_GetBackgroundColor() const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	const FLinearColor ColorModifier = GetCustomBackgroundColor() ? *GetCustomBackgroundColor() : FLinearColor(1.f, 1.f, 1.f, 1.f);
	const FLinearColor DefaultColor = Settings->TransitionEmptyColor * ColorModifier;
	
	USMTransitionGraph* Graph = Cast<USMTransitionGraph>(BoundGraph);

	if (!Graph)
	{
		return DefaultColor;
	}

	const bool bHasResultLogic = Graph->HasAnyLogicConnections();
	// This transition will never be taken.
	if (!bHasResultLogic)
	{
		return DefaultColor;
	}

	if (!Settings->bEnableTransitionWithEntryLogicColor)
	{
		return Settings->TransitionValidColor * ColorModifier;
	}

	// Transition with execution logic.
	const bool bHasTransitionEnteredLogic = Graph->HasTransitionEnteredLogic();
	if (bHasTransitionEnteredLogic)
	{
		return Settings->TransitionWithEntryLogicColor * ColorModifier;
	}
	
	// Regular transition.
	return Settings->TransitionValidColor * ColorModifier;
}

void USMGraphNode_TransitionEdge::SetDefaultsWhenPlaced()
{
	// Auto set parallel mode based on previous state.
	if (USMGraphNode_StateNodeBase* PreviousState = GetFromState())
	{
		if (USMTransitionInstance* Instance = GetNodeTemplateAs<USMTransitionInstance>())
		{
			Instance->SetRunParallel(PreviousState->ShouldDefaultTransitionsToParallel());
		}
	}
}

#undef LOCTEXT_NAMESPACE
