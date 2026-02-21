// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Graph/Nodes/SMGraphNode_StateNode.h"

#include "Compilers/SMKismetCompiler.h"
#include "Configuration/SMEditorSettings.h"
#include "FunctionNodes/SMGraphK2Node_FunctionNodes_StateInstance.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEntryNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "RootNodes/SMGraphK2Node_IntermediateRootStateMachineNodes.h"
#include "RootNodes/SMGraphK2Node_StateEndNode.h"
#include "RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "SMUnrealTypeDefs.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"

#include "SMUtils.h"

#include "Engine/Engine.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectThreadContext.h"

#define LOCTEXT_NAMESPACE "SMGraphStateNode"

USMGraphNode_StateNode::USMGraphNode_StateNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphNode_StateNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Enable templates
	bool bStateChange = false;
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNode, StateClass))
	{
		InitTemplate();
		// Disable property graph refresh because InitTemplate handles it.
		bCreatePropertyGraphsOnPropertyChange = false;

		bStateChange = true;
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNode, StateStack) || PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FStateStackContainer, StateStackClass))
	{
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
		{
			// Array element duplication requires a new template generated.
			const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
			if (ArrayIndex >= 0 && ArrayIndex + 1 < StateStack.Num())
			{
				const FStateStackContainer& OriginalStateStack = StateStack[ArrayIndex];
				FStateStackContainer& NewStateStack = StateStack[ArrayIndex + 1];

				NewStateStack.TemplateGuid = FGuid::NewGuid();
				if (OriginalStateStack.NodeStackInstanceTemplate && OriginalStateStack.NodeStackInstanceTemplate->GetClass() != GetDefaultNodeClass())
				{
					if (NewStateStack.NodeStackInstanceTemplate != OriginalStateStack.NodeStackInstanceTemplate)
					{
						// This state *shouldn't* exist because the object isn't deep copied, but who knows if USTRUCT UPROPERTY UObject handling changes?
						NewStateStack.DestroyTemplate();
					}
					
					NewStateStack.NodeStackInstanceTemplate = Cast<USMNodeInstance>(StaticDuplicateObject(OriginalStateStack.NodeStackInstanceTemplate, OriginalStateStack.NodeStackInstanceTemplate->GetOuter()));
					UEngine::CopyPropertiesForUnrelatedObjects(OriginalStateStack.NodeStackInstanceTemplate, NewStateStack.NodeStackInstanceTemplate);
					NewStateStack.NodeStackInstanceTemplate->SetTemplateGuid(NewStateStack.TemplateGuid);

					for (TFieldIterator<FProperty> It(NewStateStack.NodeStackInstanceTemplate->GetClass()); It; ++It)
					{
						// Look for real graph properties (not auto generated from a variable), they won't have had their guids cleared.
						
						FProperty* Property = *It;

						FName VarName = Property->GetFName();
						if (VarName == GET_MEMBER_NAME_CHECKED(USMNodeInstance, ExposedPropertyOverrides))
						{
							continue;
						}

						if (FStructProperty* StructProperty = LD::PropertyUtils::GetGraphPropertyFromProperty(Property))
						{
							TArray<FSMGraphProperty_Base*> GraphProperties;
							USMUtils::BlueprintPropertyToNativeProperty(Property, NewStateStack.NodeStackInstanceTemplate, GraphProperties);
							for (FSMGraphProperty_Base* GraphProperty : GraphProperties)
							{
								GraphProperty->InvalidateGuid();
							}
						}
					}

					FSMBlueprintEditorUtils::DuplicateStackTemplatePropertyGraphs(this, this, NewStateStack, OriginalStateStack.TemplateGuid);
				}
			}
		}
		
		InitStateStack();
	}
	else
	{
		// Template and state stack require full generation, otherwise quick generation is fine.
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

UObject* USMGraphNode_StateNode::GetJumpTargetForDoubleClick() const
{
	if (FSMBlueprintEditorUtils::GetEditorSettings()->StateDoubleClickBehavior == ESMJumpToGraphBehavior::PreferExternalGraph)
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

void USMGraphNode_StateNode::DestroyNode()
{
	Super::DestroyNode();
	DestroyStateStack();
}

FSlateIcon USMGraphNode_StateNode::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), TEXT("Graph.StateNode.Icon"));
}

void USMGraphNode_StateNode::PlaceDefaultInstanceNodes()
{
	Super::PlaceDefaultInstanceNodes();

	USMGraphK2Node_StateEntryNode* EntryNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphK2Node_StateEntryNode>(BoundGraph);
	FSMBlueprintEditorUtils::PlaceNodeIfNotSet<USMGraphK2Node_StateInstance_Begin>(BoundGraph, EntryNode);

	USMGraphK2Node_StateUpdateNode* UpdateNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphK2Node_StateUpdateNode>(BoundGraph);
	FSMBlueprintEditorUtils::PlaceNodeIfNotSet<USMGraphK2Node_StateInstance_Update>(BoundGraph, UpdateNode);

	USMGraphK2Node_StateEndNode* EndNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<USMGraphK2Node_StateEndNode>(BoundGraph);
	FSMBlueprintEditorUtils::PlaceNodeIfNotSet<USMGraphK2Node_StateInstance_End>(BoundGraph, EndNode);

	// Optional nodes.
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_IntermediateStateMachineStartNode, USMGraphK2Node_StateInstance_StateMachineStart>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_IntermediateStateMachineStopNode, USMGraphK2Node_StateInstance_StateMachineStop>(BoundGraph);

	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionInitializedNode, USMGraphK2Node_StateInstance_OnStateInitialized>(BoundGraph);
	FSMBlueprintEditorUtils::SetupDefaultPassthroughNodes<USMGraphK2Node_TransitionShutdownNode, USMGraphK2Node_StateInstance_OnStateShutdown>(BoundGraph);
}

void USMGraphNode_StateNode::SetNodeClass(UClass* Class)
{
	StateClass = Class;
	Super::SetNodeClass(Class);
}

const FSlateBrush* USMGraphNode_StateNode::GetNodeIcon() const
{
	if (const FSlateBrush* Icon = Super::GetNodeIcon())
	{
		return Icon;
	}

	return FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Icon"));
}

void USMGraphNode_StateNode::InitTemplate()
{
	Super::InitTemplate();
}

bool USMGraphNode_StateNode::AreTemplatesFullyLoaded() const
{
	if (!Super::AreTemplatesFullyLoaded())
	{
		return false;
	}

	for (const FStateStackContainer& Stack : StateStack)
	{
		if (!Stack.NodeStackInstanceTemplate || Stack.NodeStackInstanceTemplate->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
		{
			return false;
		}
	}

	return true;
}

bool USMGraphNode_StateNode::DoesNodePossiblyHaveConstructionScripts() const
{
	if (Super::DoesNodePossiblyHaveConstructionScripts())
	{
		return true;
	}

	for (const FStateStackContainer& Stack : StateStack)
	{
		if (Stack.NodeStackInstanceTemplate && Stack.NodeStackInstanceTemplate->bHasEditorConstructionScripts)
		{
			return true;
		}
	}

	return false;
}

void USMGraphNode_StateNode::RunAllConstructionScripts_Internal()
{
	Super::RunAllConstructionScripts_Internal();

	if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		// Use an iterator in case the stack is modified during construction.
		for (auto It = StateStack.CreateIterator(); It; ++It)
		{
			if (It->NodeStackInstanceTemplate)
			{
				It->NodeStackInstanceTemplate->RunConstructionScript();
			}
		}
	}
}

void USMGraphNode_StateNode::RestoreArchetypeValuesPriorToConstruction()
{
	Super::RestoreArchetypeValuesPriorToConstruction();
	for (const FStateStackContainer& Stack : StateStack)
	{
		if (Stack.NodeStackInstanceTemplate)
		{
			Stack.NodeStackInstanceTemplate->RestoreArchetypeValuesPriorToConstruction();
		}
	}
}

void USMGraphNode_StateNode::PreCompileNodeInstanceValidation(FCompilerResultsLog& CompilerContext,
                                                              USMCompilerLog* CompilerLog, USMGraphNode_Base* OwningNode)
{
	Super::PreCompileNodeInstanceValidation(CompilerContext, CompilerLog, OwningNode);

	const TArray<FStateStackContainer>& Templates = GetAllNodeStackTemplates();
	for (const FStateStackContainer& Template : Templates)
	{
		RunPreCompileValidateForNodeInstance(Template.NodeStackInstanceTemplate, CompilerLog);
	}
}

void USMGraphNode_StateNode::OnCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::OnCompile(CompilerContext);

	const TArray<FStateStackContainer>& Templates = GetAllNodeStackTemplates();

	if (Templates.Num() > 0)
	{
		FSMNode_Base* RuntimeNode = FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(BoundGraph);
		check(RuntimeNode);

		for (const FStateStackContainer& Template : Templates)
		{
			if (Template.NodeStackInstanceTemplate && GetDefaultNodeClass() != Template.StateStackClass)
			{
				CompilerContext.AddDefaultObjectTemplate(RuntimeNode->GetNodeGuid(), Template.NodeStackInstanceTemplate, FTemplateContainer::StackTemplate, Template.TemplateGuid);
			}
		}
	}
}

int32 USMGraphNode_StateNode::GetIndexOfTemplate(const FGuid& TemplateGuid) const
{
	for (int32 Idx = 0; Idx < StateStack.Num(); ++Idx)
	{
		if (StateStack[Idx].TemplateGuid == TemplateGuid)
		{
			return Idx;
		}
	}

	return INDEX_NONE;
}

void USMGraphNode_StateNode::GetAllNodeTemplates(TArray<USMNodeInstance*>& OutNodeInstances) const
{
	Super::GetAllNodeTemplates(OutNodeInstances);
	for (const FStateStackContainer& Stack : GetAllNodeStackTemplates())
	{
		OutNodeInstances.Add(Stack.NodeStackInstanceTemplate);
	}
}

USMNodeInstance* USMGraphNode_StateNode::AddStackNode(TSubclassOf<USMNodeInstance> NodeClass, int32 StackIndex)
{
	Modify();

	const UClass* DefaultNodeClass = GetDefaultNodeClass();
	if (!ensureMsgf(NodeClass && NodeClass->IsChildOf(DefaultNodeClass) && NodeClass != DefaultNodeClass, TEXT("AddStackNode only accepts a valid child of a State Instance Class.")))
	{
		return nullptr;
	}

	const FStateStackContainer NewStateStack(static_cast<TSubclassOf<USMStateInstance>>(NodeClass));

	const int32 IndexUsed = USMUtils::InsertOrAddToArray(StateStack, NewStateStack, StackIndex);
	check(IndexUsed >= 0);

	InitStateStack();
	CreateGraphPropertyGraphs();

	USMStateInstance* StackInstance = Cast<USMStateInstance>(
		GetTemplateFromIndex(IndexUsed));

	ensure(StackInstance);
	return StackInstance;
}

void USMGraphNode_StateNode::RemoveStackNode(int32 StackIndex)
{
	Modify();
	USMUtils::RemoveAtOrPopFromArray(StateStack, StackIndex);
}

void USMGraphNode_StateNode::ClearStackNodes()
{
	Modify();
	StateStack.Empty();
}

const TArray<FStateStackContainer>& USMGraphNode_StateNode::GetAllNodeStackTemplates() const
{
	return StateStack;
}

USMNodeInstance* USMGraphNode_StateNode::GetTemplateFromIndex(int32 Index) const
{
	if (Index >= 0 && Index < StateStack.Num())
	{
		return StateStack[Index].NodeStackInstanceTemplate;
	}

	return nullptr;
}

void USMGraphNode_StateNode::InitStateStack()
{
	for (FStateStackContainer& StateContainer : StateStack)
	{
		StateContainer.InitTemplate(this);
	}
}

void USMGraphNode_StateNode::DestroyStateStack()
{
	for (FStateStackContainer& StateContainer : StateStack)
	{
		StateContainer.DestroyTemplate();
	}

	StateStack.Reset();
}

#undef LOCTEXT_NAMESPACE
