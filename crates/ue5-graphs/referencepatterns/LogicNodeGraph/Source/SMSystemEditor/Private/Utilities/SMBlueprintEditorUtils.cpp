// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Blueprints/SMBlueprintFactory.h"
#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Blueprints/UI/SSMConvertToReferenceDialog.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Construction/ISMEditorConstructionManager.h"
#include "Graph/Nodes/FunctionNodes/SMGraphK2Node_FunctionNodes_TransitionEvent.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateMachineSelectNode.h"
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
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/Schema/SMGraphSchema.h"
#include "Graph/SMGraph.h"
#include "Graph/SMPropertyGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMNodeInstanceUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMConduit.h"
#include "SMUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "EdGraphUtilities.h"
#include "EditorStyleSet.h"
#include "Factories/Factory.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IMainFrameModule.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Composite.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_InputAction.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/App.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SMBlueprintEditorUtils"

FSMBlueprintEditor* FSMBlueprintEditorUtils::GetStateMachineEditor(UObject const* Object)
{
	if (!Object || !GEditor)
	{
		return nullptr;
	}

	UBlueprint const* Blueprint = nullptr;

	if (Object->IsA<USMBlueprint>())
	{
		Blueprint = Cast<USMBlueprint>(Object);
	}
	else if (Object->IsA<UEdGraph>())
	{
		Blueprint = FindBlueprintForGraph(Cast<UEdGraph>(Object));
	}
	else if (Object->IsA<UEdGraphNode>())
	{
		Blueprint = FindBlueprintForNode(Cast<UEdGraphNode>(Object));
	}

	if (Blueprint == nullptr)
	{
		return nullptr;
	}

	return (FSMBlueprintEditor*)GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset((UObject*)Blueprint, false);
}

USMBlueprint* FSMBlueprintEditorUtils::FindBlueprintFromObject(UObject* Object)
{
	for (UObject* Owner = Object; Owner; Owner = Owner->GetOuter())
	{
		if (USMBlueprint* Blueprint = Cast<USMBlueprint>(Owner))
		{
			return Blueprint;
		}
		if (USMBlueprintGeneratedClass* GeneratedClass = Cast<USMBlueprintGeneratedClass>(Owner))
		{
			if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(GeneratedClass))
			{
				return Cast<USMBlueprint>(Blueprint);
			}
		}
	}

	return nullptr;
}

const USMEditorSettings* FSMBlueprintEditorUtils::GetEditorSettings()
{
	static const USMEditorSettings* Settings = GetDefault<USMEditorSettings>();
	check(Settings);
	return Settings;
}

USMEditorSettings* FSMBlueprintEditorUtils::GetMutableEditorSettings()
{
	static USMEditorSettings* Settings = GetMutableDefault<USMEditorSettings>();
	check(Settings);
	return Settings;
}

const USMProjectEditorSettings* FSMBlueprintEditorUtils::GetProjectEditorSettings()
{
	static const USMProjectEditorSettings* Settings = GetDefault<USMProjectEditorSettings>();
	check(Settings);
	return Settings;
}

USMProjectEditorSettings* FSMBlueprintEditorUtils::GetMutableProjectEditorSettings()
{
	static USMProjectEditorSettings* Settings = GetMutableDefault<USMProjectEditorSettings>();
	check(Settings);
	return Settings;
}

bool FSMBlueprintEditorUtils::ContainsNodeOfAnyClassNested(const UEdGraph* InGraph, const TArray<UClass*>& InClasses)
{
	check(InGraph);

	for (const TObjectPtr<UEdGraphNode>& Node : InGraph->Nodes)
	{
		if (Node && InClasses.ContainsByPredicate([&](const UClass* Class)
		{
			return Node->GetClass()->IsChildOf(Class);
		}))
		{
			return true;
		}
	}

	TArray<UEdGraph*> ChildrenGraphs;
	InGraph->GetAllChildrenGraphs(ChildrenGraphs);

	for (const UEdGraph* NextGraph : ChildrenGraphs)
	{
		if (ContainsNodeOfAnyClassNested(NextGraph, InClasses))
		{
			return true;
		}
	}

	return false;
}

void FSMBlueprintEditorUtils::GetAllRuntimeEntryNodes(const UEdGraph* InGraph, TArray<USMGraphK2Node_RuntimeNode_Base*>& OutEntryNodes)
{
	TArray<USMGraphK2Node_RuntimeNode_Base*> AllNodes;
	GetAllNodesOfClassNested(InGraph, AllNodes);

	AllNodes.RemoveAll([](const USMGraphK2Node_RuntimeNode_Base* Node)
	{
		return !Node->IsConsideredForEntryConnection();
	});
	
	OutEntryNodes.Append(AllNodes);
}

void FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(UBlueprint* Blueprint, bool bUpdateDependencies, bool bRecreateGraphProperties)
{
	check(Blueprint);

	USMBlueprint* SMBlueprint = CastChecked<USMBlueprint>(Blueprint);
	FSMConditionalCompileConfiguration Configuration;
	{
		Configuration.bUpdateDependencies = bUpdateDependencies;
		Configuration.bRecreateGraphProperties = bRecreateGraphProperties;
	}
	ISMEditorConstructionManager::Get().QueueBlueprintForConditionalCompile(SMBlueprint, MoveTemp(Configuration));
}

FSMBlueprintEditorUtils::FOnBlueprintConditionallyCompiled FSMBlueprintEditorUtils::OnBlueprintPreConditionallyCompiledEvent;
FSMBlueprintEditorUtils::FOnBlueprintConditionallyCompiled FSMBlueprintEditorUtils::OnBlueprintPostConditionallyCompiledEvent;

void FSMBlueprintEditorUtils::GetValidDerivedClasses(const UClass* TargetClass, TArray<UClass*>& OutClasses)
{
	TArray<UClass*> Classes;
	GetDerivedClasses(TargetClass, Classes);

	for (UClass* Class : Classes)
	{
		if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) ||
			Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		OutClasses.Add(Class);
	}
}

UClass* FSMBlueprintEditorUtils::GetMostUpToDateClass(UClass* Class)
{
	if (Class && Class->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		UBlueprint* GeneratedByBP = Cast<UBlueprint>(Class->ClassGeneratedBy);
		if (GeneratedByBP != nullptr)
		{
			UClass* NewOutputClass = GeneratedByBP->GeneratedClass;
			if (NewOutputClass && !NewOutputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				return NewOutputClass;
			}
		}
	}

	return Class;
}

UClass* FSMBlueprintEditorUtils::TryGetFullyGeneratedClass(UClass* Class)
{
	if (!Class)
	{
		return nullptr;
	}

	if (UBlueprint* FoundBlueprint = UBlueprint::GetBlueprintFromClass(Class))
	{
		return FoundBlueprint->GeneratedClass ? Cast<UClass>(FoundBlueprint->GeneratedClass) : Class;
	}

	return Class;
}

void FSMBlueprintEditorUtils::GetAllNodeInstancesWithPropertyGraphs(UBlueprint* Blueprint,
                                                                    TSet<TSubclassOf<USMNodeInstance>>& NodeInstances)
{
	TArray<USMGraphK2Node_PropertyNode_Base*> PropertyNodes;
	GetAllNodesOfClassNested<USMGraphK2Node_PropertyNode_Base>(GetTopLevelStateMachineGraph(Blueprint), PropertyNodes);
	for (USMGraphK2Node_PropertyNode_Base* PropertyNode : PropertyNodes)
	{
		NodeInstances.Add(PropertyNode->GetOwningTemplate()->GetClass());
		
	}
}

void FSMBlueprintEditorUtils::HandleRefreshAllNodes(UBlueprint* InBlueprint)
{
	if (InBlueprint->IsA<USMBlueprint>())
	{
		TArray<USMGraphNode_Base*> AllNodes;
		GetAllNodesOfClass(InBlueprint, AllNodes);

		for (USMGraphNode_Base* Node : AllNodes)
		{
			Node->ReconstructNode();
		}
	}
}

void FSMBlueprintEditorUtils::HandleRenameVariableEvent(UBlueprint* InBlueprint, UClass* InVariableClass,
	const FName& InOldVarName, const FName& InNewVarName)
{
	if (InOldVarName == InNewVarName || !InBlueprint || !InBlueprint->GeneratedClass)
	{
		return;
	}
	
	if (const USMNodeBlueprint* NodeBlueprint = Cast<USMNodeBlueprint>(InBlueprint))
	{
		if (USMNodeInstance* DefaultNodeInstance = Cast<USMNodeInstance>(NodeBlueprint->GeneratedClass->ClassDefaultObject))
		{
			// Update a property override with the new name if one exists.
			if (DefaultNodeInstance->FindExposedPropertyOverrideByName(InOldVarName))
			{
				// Remove any existing new variable, we'll want to use the old one the user has selected.
				FSMNodeInstanceUtils::RemoveExposedPropertyOverrideByName(DefaultNodeInstance, InNewVarName);

				// Update the variable name.
				ensure(FSMNodeInstanceUtils::UpdateExposedPropertyOverrideName(DefaultNodeInstance, InOldVarName, InNewVarName));
			}
		}
	}
}

void FSMBlueprintEditorUtils::GetAllConnectedNodes(UEdGraphNode* StartNode, EEdGraphPinDirection Direction,
                                                   TSet<UEdGraphNode*>& FoundNodes)
{
	if (FoundNodes.Contains(StartNode))
	{
		return;
	}

	FoundNodes.Add(StartNode);
	
	for (UEdGraphPin* Pin : StartNode->GetAllPins())
	{
		if (Pin->Direction == Direction || Direction == EGPD_MAX)
		{
			for (UEdGraphPin* ConnectedPin : Pin->LinkedTo)
			{
				GetAllConnectedNodes(ConnectedPin->GetOwningNode(), Direction, FoundNodes);
			}
		}
	}
}

void FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(UEdGraph* GraphIn, UBlueprint* BlueprintIn, bool bModify, bool bSkipEntryNodes, bool bSilently)
{
	if (BlueprintIn == nullptr)
	{
		BlueprintIn = FindBlueprintForGraph(GraphIn);
	}
	TArray<UEdGraphNode*> Nodes = GraphIn->Nodes;
	for (UEdGraphNode* Node : Nodes)
	{
		if (bSkipEntryNodes && (Node->IsA<USMGraphNode_StateMachineEntryNode>() || Node->IsA<USMGraphK2Node_StateMachineEntryNode>()))
		{
			continue;
		}

		if (bSilently)
		{
			RemoveNodeSilently(BlueprintIn, Node);
		}
		else
		{
			RemoveNode(BlueprintIn, Node, true);
		}
	}

	if (bModify)
	{
		MarkBlueprintAsStructurallyModified(BlueprintIn);
	}
}

void FSMBlueprintEditorUtils::RemoveNodeSilently(UBlueprint* Blueprint, UEdGraphNode* Node)
{
	check(Node);

	const UEdGraphSchema* Schema = nullptr;

	// Ensure we mark parent graph modified
	if (UEdGraph* GraphObj = Node->GetGraph())
	{
		GraphObj->Modify();
		Schema = GraphObj->GetSchema();
	}

	if (Blueprint != nullptr)
	{
		// Remove any breakpoints set on the node
		FKismetDebugUtilities::RemoveBreakpointFromNode(Node, Blueprint);

		// Remove any watches set on the node's pins
		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
		{
			FKismetDebugUtilities::RemovePinWatch(Blueprint, Node->Pins[PinIndex]);
		}
	}

	Node->Modify();

	// Timelines will be removed from the blueprint if the node is a UK2Node_Timeline
	// Owning blueprint can't be null for this operation. It's possible it is if this is an
	// invalid graph we are cleaning up.
	if (Schema && FBlueprintEditorUtils::FindBlueprintForNode(Node) != nullptr)
	{
		Schema->BreakNodeLinks(*Node);
	}

	Node->DestroyNode();
}

bool FSMBlueprintEditorUtils::IsNodeSelected(UEdGraphNode* Node)
{
	if (FSMBlueprintEditor* Editor = GetStateMachineEditor(Node))
	{
		return Editor->GetSelectedNodes().Contains(Node);
	}

	return false;
}

void FSMBlueprintEditorUtils::SplitCategories(const FString& InCategoryString, TArray<FString>& OutCategories)
{
	OutCategories.Reset();
	InCategoryString.ParseIntoArray(OutCategories, TEXT("|"));
}

bool FSMBlueprintEditorUtils::IsNodeGraphDefault(const UEdGraphNode* Node)
{
	return Node && Node->GetOutermost()->GetMetaData()->HasValue(Node, FNodeMetadata::DefaultGraphNode);
}

FSMNode_Base* FSMBlueprintEditorUtils::GetRuntimeNodeFromGraph(const UEdGraph* Graph)
{
	Graph = FindTopLevelOwningGraph(Graph);

	if (!Graph)
	{
		return nullptr;
	}

	if (const USMGraphK2* K2Graph = Cast<USMGraphK2>(Graph))
	{
		return K2Graph->GetRuntimeNode();
	}

	if (const USMGraph* SMGraph = Cast<USMGraph>(Graph))
	{
		return SMGraph->GetRuntimeNode();
	}

	return nullptr;
}

FSMNode_Base* FSMBlueprintEditorUtils::GetRuntimeNodeFromExactNode(UEdGraphNode* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	if (USMGraphK2Node_RuntimeNodeContainer* Container = Cast<USMGraphK2Node_RuntimeNodeContainer>(Node))
	{
		return Container->GetRunTimeNode();
	}

	if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(Node))
	{
		return &EntryNode->StateMachineNode;
	}

	return nullptr;
}

FSMNode_Base* FSMBlueprintEditorUtils::GetRuntimeNodeFromExactNodeChecked(UEdGraphNode* Node)
{
	FSMNode_Base* RuntimeNode = GetRuntimeNodeFromExactNode(Node);
	check(RuntimeNode);
	return RuntimeNode;
}

UEdGraph* FSMBlueprintEditorUtils::FindGraphFromRuntimeNodeGuid(UEdGraph* InGraph, const FGuid& InNodeGuid, const FString& InGraphName)
{
	if (const FSMNode_Base* RuntimeNode = GetRuntimeNodeFromGraph(InGraph))
	{
		if (RuntimeNode->GetNodeGuid() == InNodeGuid && (InGraphName.IsEmpty() || InGraphName == InGraph->GetName()))
		{
			return InGraph;
		}
	}

	for (const TObjectPtr<UEdGraph>& ChildGraph : InGraph->SubGraphs)
	{
		if (UEdGraph* MatchingGraph = FindGraphFromRuntimeNodeGuid(ChildGraph, InNodeGuid, InGraphName))
		{
			return MatchingGraph;
		}
	}

	return nullptr;
}

USMNodeInstance* FSMBlueprintEditorUtils::GetNodeTemplate(const UEdGraph* ForGraph)
{
	if (const USMGraphNode_Base* NodeOwner = FindTopLevelOwningNode(ForGraph))
	{
		return NodeOwner->GetNodeTemplate();
	}

	return nullptr;
}

TSubclassOf<UObject> FSMBlueprintEditorUtils::GetNodeTemplateClass(const UEdGraph* ForGraph, bool bReturnDefaultIfNone, const FGuid& TemplateGuid)
{
	if (const USMGraphNode_Base* NodeOwner = FindTopLevelOwningNode(ForGraph))
	{
		if (TemplateGuid.IsValid())
		{
			if (const USMNodeInstance* Template = NodeOwner->GetNodeTemplateFromGuid(TemplateGuid))
			{
				return Template->GetClass();
			}
		}
		
		if (UClass* Class = NodeOwner->GetNodeClass())
		{
			return Class;
		}

		if (bReturnDefaultIfNone)
		{
			if (const FSMNode_Base* RuntimeNode = NodeOwner->FindRuntimeNode())
			{
				return RuntimeNode->GetDefaultNodeInstanceClass();
			}
		}
	}

	return nullptr;
}

UClass* FSMBlueprintEditorUtils::GetNodeClassFromPin(const UEdGraphPin* Pin)
{
	UClass* FromClass = nullptr;
	if (Pin)
	{
		if (const USMGraphNode_Base* BaseNode = Cast<USMGraphNode_Base>(Pin->GetOwningNode()))
		{
			FromClass = BaseNode->GetNodeClass();
		}
	}

	return FromClass;
}

UClass* FSMBlueprintEditorUtils::GetStateMachineClassFromGraph(const UEdGraph* Graph)
{
	if (const USMGraph* SMGraph = Cast<USMGraph>(Graph))
	{
		// Nested state machine.
		if (const USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(SMGraph->GetOwningStateMachineNodeWhenNested()))
		{
			return StateMachineNode->GetNodeClass();
		}

		// Root state machine.
		const UBlueprint* Blueprint = FindBlueprintForGraphChecked(Graph);
		if (const USMInstance* Instance = Cast<USMInstance>(Blueprint->GeneratedClass->GetDefaultObject(false)))
		{
			return Instance->GetRootStateMachineNodeClass();
		}
	}

	return nullptr;
}

void FSMBlueprintEditorUtils::GoToNodeBlueprint(const USMGraphNode_Base* InGraphNode)
{
	if (const UClass* Class = InGraphNode->GetNodeClass())
	{
		if (const UBlueprint* NodeBlueprint = GetNodeBlueprintFromClassAndSetDebugObject(Class, InGraphNode))
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NodeBlueprint);
		}
	}
}

USMNodeBlueprint* FSMBlueprintEditorUtils::GetNodeBlueprintFromClassAndSetDebugObject(const UClass* InClass,
                                                                                      const USMGraphNode_Base* InGraphNode, const FGuid* InTemplateGuid)
{
	if (USMNodeBlueprint* NodeBlueprint = Cast<USMNodeBlueprint>(UBlueprint::GetBlueprintFromClass(InClass)))
	{
		if (const USMGraphNode_Base* OwningNode = Cast<USMGraphNode_Base>(InGraphNode))
		{
			if (const FSMNode_Base* DebugNode = GetDebugNode(OwningNode))
			{
				if (InTemplateGuid)
				{
					const int32 TemplateIndex = InGraphNode->GetIndexOfTemplate(*InTemplateGuid);
					if (USMNodeInstance* NodeStackInstance = DebugNode->GetNodeInStack(TemplateIndex))
					{
						NodeBlueprint->SetObjectBeingDebugged(NodeStackInstance);
						return NodeBlueprint;
					}
				}
				
				if (USMNodeInstance* NodeInstance = DebugNode->GetNodeInstance())
				{
					// Verify the correct class is used in case this is a stack instance that wasn't found.
					if (InClass == NodeInstance->GetClass())
					{
						NodeBlueprint->SetObjectBeingDebugged(NodeInstance);
					}
				}
			}
		}

		return NodeBlueprint;
	}

	return nullptr;
}

const FSMNode_Base* FSMBlueprintEditorUtils::GetDebugNode(const USMGraphNode_Base* Node)
{
	check(Node);

	if (UBlueprint* ThisBlueprint = FindBlueprintForNode(Node))
	{
		if (USMInstance* CurrentDebugObject = Cast<USMInstance>(ThisBlueprint->GetObjectBeingDebugged()))
		{
			CurrentDebugObject = CurrentDebugObject->GetPrimaryReferenceOwner();
			if (FSMNode_Base* RuntimeNode = GetRuntimeNodeFromGraph(Node->GetBoundGraph()))
			{
				// Find the correct runtime instance mapping to this node.
				if (const FSMNode_Base* RealRuntimeNode = CurrentDebugObject->GetDebugStateMachineConst().GetRuntimeNode(RuntimeNode->GetNodeGuid()))
				{
					return RealRuntimeNode;
				}
			}
		}
	}

	return nullptr;
}

void FSMBlueprintEditorUtils::FindRuntimeNodeWithOwners(const UEdGraph* Graph, TArray<const FSMNode_Base*>& RuntimeNodesOrdered, TSet<const UObject*>* StopOnOuters)
{
	while (Graph)
	{
		FSMNode_Base* RuntimeNode = GetRuntimeNodeFromGraph(Graph);
		if (!RuntimeNode)
		{
			break;
		}
		RuntimeNodesOrdered.Add(RuntimeNode);

		UEdGraphNode* OwningNode = Cast<UEdGraphNode>(Graph->GetOuter());
		if (OwningNode)
		{
			if (StopOnOuters && StopOnOuters->Contains(OwningNode))
			{
				break;
			}

			Graph = OwningNode->GetGraph();
		}
		else
		{
			break;
		}
	}

	Algo::Reverse(RuntimeNodesOrdered);
}

FGuid FSMBlueprintEditorUtils::TryCreatePathGuid(const UEdGraph* Graph)
{
	TArray<const FSMNode_Base*> Nodes;
	FindRuntimeNodeWithOwners(Graph, Nodes);

	return USMUtils::PathToGuid(USMUtils::BuildGuidPathFromNodes(Nodes));
}

FString FSMBlueprintEditorUtils::GetSafeName(const FString& InName)
{
	return ObjectTools::SanitizeObjectName(InName);
}

FString FSMBlueprintEditorUtils::GetSafeStateName(const FString& InName)
{
	return ObjectTools::SanitizeInvalidChars(InName, LD_INVALID_STATENAME_CHARACTERS);
}

FString FSMBlueprintEditorUtils::FindUniqueName(const FString& InName, UEdGraph* Graph)
{
	check(Graph);

	FString NameToCheck = InName;
	FString Prefix = InName;

	bool bHasUnderscore = false;
	int32 NameCount = 0;
	const int32 UnderscoreIndex = NameToCheck.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (UnderscoreIndex >= 0)
	{
		const FString Number = NameToCheck.RightChop(UnderscoreIndex + 1);
		if (Number.Len() == 0)
		{
			// Ends in underscore.
			bHasUnderscore = true;
		}
		else if (Number.IsNumeric())
		{
			NameCount = FCString::Atoi(*Number);
			Prefix.RemoveAt(UnderscoreIndex, Prefix.Len() - UnderscoreIndex);
		}
	}

	while (Graph->SubGraphs.FindByPredicate([&NameToCheck](UEdGraph* SubGraph)
	{
		return NameToCheck == SubGraph->GetName();
	}) != nullptr)
	{
		NameToCheck = Prefix + (bHasUnderscore ? TEXT("") : TEXT("_")) + FString::FromInt(++NameCount);
	}

	return NameToCheck;
}

void FSMBlueprintEditorUtils::UpdateRuntimeNodeForGraph(FSMNode_Base* Node, const UEdGraph* Graph)
{
	FSMNode_Base* RuntimeNode = GetRuntimeNodeFromGraph(Graph);
	if (RuntimeNode)
	{
		*RuntimeNode = *Node;
	}

	TArray<USMGraphK2Node_RuntimeNodeReference*> References;
	GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeReference>(Graph, References);
	for (USMGraphK2Node_RuntimeNodeReference* Reference : References)
	{
		Reference->Modify();
		Reference->RuntimeNodeGuid = Node->GetNodeGuid();
	}
}

void FSMBlueprintEditorUtils::UpdateRuntimeNodeForNestedGraphs(const FGuid& CurrentGuid, FSMNode_Base* Node, const UEdGraph* Graph)
{
	TSet<UEdGraph*> NestedGraphs;
	GetAllGraphsOfClassNested<UEdGraph>(Graph, NestedGraphs);

	for (UEdGraph* NestedGraph : NestedGraphs)
	{
		// We want to make sure the node we're updating is correct since all sub graphs will be replaced with the given node.
		FSMNode_Base* GraphRuntimeNode = GetRuntimeNodeFromGraph(NestedGraph);
		if (!GraphRuntimeNode || GraphRuntimeNode->GetNodeGuid() != CurrentGuid)
		{
			continue;
		}

		UpdateRuntimeNodeForGraph(Node, NestedGraph);
	}
}

void FSMBlueprintEditorUtils::UpdateRuntimeNodeForBlueprint(const FGuid& CurrentGuid, FSMNode_Base* Node, UBlueprint* Blueprint)
{
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		UpdateRuntimeNodeForNestedGraphs(CurrentGuid, Node, Graph);
	}
}

FMulticastDelegateProperty* FSMBlueprintEditorUtils::GetDelegateProperty(const FName& DelegatePropertyName,
	UClass* DelegateOwnerClass, const UFunction* SignatureFunction)
{
	if (DelegatePropertyName == NAME_None)
	{
		return nullptr;
	}
	
	if (FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(DelegateOwnerClass, DelegatePropertyName))
	{
		return Property;
	}

	if (FMulticastDelegateProperty* Property = FMemberReference::FindRemappedField<FMulticastDelegateProperty>(DelegateOwnerClass, DelegatePropertyName))
	{
		return Property;
	}

	for (TFieldIterator<FMulticastDelegateProperty> It(DelegateOwnerClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if (FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
		{
			if (Delegate->GetFName() == DelegatePropertyName)
			{
				return Delegate;
			}
		}
	}

	if (SignatureFunction)
	{
		return FindDelegatePropertyByFunction(SignatureFunction);
	}
	
	return nullptr;
}

FMulticastDelegateProperty* FSMBlueprintEditorUtils::FindDelegatePropertyByFunction(const UFunction* SignatureFunction)
{
	check(SignatureFunction);
	if (const UClass* OuterUClass = SignatureFunction->GetOuterUClass())
	{
		const bool bIsPackage = Cast<UPackage>(OuterUClass) != nullptr;
		if (!bIsPackage)
		{
			// Field iterator on packages will crash and also aren't necessary. If bIsPackage is true likely the delegate doesn't exist,
			// such as a native delegate that was removed.
			for (TFieldIterator<FMulticastDelegateProperty> It(OuterUClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				if (FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
				{
					if (Delegate->SignatureFunction->GetFName() == SignatureFunction->GetFName())
					{
						return Delegate;
					}
				}
			}
		}
	}

	return nullptr;
}

bool FSMBlueprintEditorUtils::DoesGraphHaveInputEvents(const UEdGraph* InGraph)
{
	if (!InGraph)
	{
		return false;
	}

	return ContainsNodeOfAnyClassNested(InGraph,
		{
			UK2Node_InputKey::StaticClass(),
			UK2Node_InputAction::StaticClass(),
			UK2Node_InputAxisEvent::StaticClass(),
			UK2Node_InputAxisKeyEvent::StaticClass(),
			UK2Node_EnhancedInputAction::StaticClass()
		});
}

bool FSMBlueprintEditorUtils::GraphHasAnyLogicConnections(const UEdGraph* Graph)
{
	if (!Graph)
	{
		return false;
	}

	if (const USMGraph* EdGraph = Cast<USMGraph>(Graph))
	{
		return EdGraph->HasAnyLogicConnections();
	}
	if (const USMGraphK2* K2Graph = Cast<USMGraphK2>(Graph))
	{
		return K2Graph->HasAnyLogicConnections();
	}

	return false;
}

USMGraphNode_Base* FSMBlueprintEditorUtils::FindTopLevelOwningNode(const UEdGraph* InGraph)
{
	if (!InGraph)
	{
		return nullptr;
	}

	for (UObject* Outer = InGraph->GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (USMGraphNode_Base* OwningNode = Cast<USMGraphNode_Base>(Outer))
		{
			return OwningNode;
		}
	}

	return nullptr;
}

const UEdGraph* FSMBlueprintEditorUtils::FindTopLevelOwningGraph(const UEdGraph* InGraph)
{
	USMGraphNode_Base* OwningNode = FindTopLevelOwningNode(InGraph);
	if (!OwningNode)
	{
		return InGraph;
	}

	return OwningNode->GetBoundGraph();
}

USMGraphK2* FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(UBlueprint* Blueprint)
{
	if (Blueprint == nullptr)
	{
		return nullptr;
	}

	const TObjectPtr<UEdGraph>* FoundGraph = Blueprint->UbergraphPages.FindByPredicate([&](const UEdGraph* Graph)
	{
		return Graph->GetFName() == USMGraphK2Schema::GN_StateMachineDefinitionGraph;
	});

	if (FoundGraph)
	{
		return Cast<USMGraphK2>(*FoundGraph);
	}

	return nullptr;
}

USMGraphK2Node_StateMachineNode* FSMBlueprintEditorUtils::GetRootStateMachineNode(UBlueprint* Blueprint, bool bUseParent)
{
	if (Blueprint == nullptr)
	{
		return nullptr;
	}

	USMGraphK2* TopLevelGraph = GetTopLevelStateMachineGraph(Blueprint);
	if (!TopLevelGraph)
	{
		return nullptr;
	}

	USMGraphK2Node_StateMachineSelectNode* SelectNode = GetFirstNodeOfClassNested<USMGraphK2Node_StateMachineSelectNode>(TopLevelGraph);
	if (SelectNode == nullptr)
	{
		return nullptr;
	}

	// This isn't connected to a state machine. We need to look up and hope there's a parent class that has one.
	if (SelectNode->GetInputPin()->LinkedTo.Num() == 0 && bUseParent)
	{
		if (const UClass* ParentClass = Blueprint->ParentClass)
		{
			if (UBlueprint* ParentBP = Cast<UBlueprint>(ParentClass->ClassGeneratedBy))
			{
				return GetRootStateMachineNode(ParentBP, bUseParent);
			}
		}
	}

	// No valid state machine graph exists. It's possible we aren't checking the parent or the parent doesn't have a valid graph either.
	if (SelectNode->GetInputPin()->LinkedTo.Num() == 0)
	{
		return nullptr;
	}

	return Cast<USMGraphK2Node_StateMachineNode>(SelectNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
}

USMGraph* FSMBlueprintEditorUtils::GetRootStateMachineGraph(UBlueprint* Blueprint, bool bUseParent)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMBlueprintEditorUtils::GetRootStateMachineGraph"), STAT_GetRootStateMachineGraph, STATGROUP_LogicDriverEditor);
	
	USMGraphK2Node_StateMachineNode* TopLevelNode = GetRootStateMachineNode(Blueprint, bUseParent);

	if (TopLevelNode == nullptr)
	{
		return nullptr;
	}

	return TopLevelNode->GetStateMachineGraph();
}

USMGraphK2Node_RuntimeNodeContainer* FSMBlueprintEditorUtils::GetRuntimeContainerFromGraph(const UEdGraph* Graph)
{
	const UEdGraph* OwningGraph = FindTopLevelOwningGraph(Graph);

	if (const USMGraph* StateMachineGraph = Cast<USMGraph>(OwningGraph))
	{
		/* State machine states have a special container which should already be generated by this point, but can be null. */
		return StateMachineGraph->GeneratedContainerNode;
	}

	TArray<USMGraphK2Node_RuntimeNodeContainer*> ResultNodes;
	GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeContainer>(OwningGraph, ResultNodes);

	if (!ensure(ResultNodes.Num() == 1))
	{
		// This was reported being hit on 2.5.2 / 4.27.2 with collapsing nodes to nested state machines, and
		// copy and pasting them between super/sub graphs. Cannot recreate, but this can't be a check or
		// projects may not load.

		// This has been confirmed hit on 2.7 / 4.27.2 when undoing / redoing node deletion that impacts
		// construction scripts of nodes, such as a property array that is generated based on connected nodes.
		if (ResultNodes.Num() == 0)
		{
			return nullptr;
		}
	}
	return ResultNodes[0];
}

bool FSMBlueprintEditorUtils::IsNodeImpactedFromAnyStateNode(const USMGraphNode_StateNodeBase* StateNode, TArray<USMGraphNode_AnyStateNode*>* OutAllAnyStates)
{
	check(StateNode)
	if (OutAllAnyStates)
	{
		OutAllAnyStates->Reset();
	}
	
	TArray<USMGraphNode_AnyStateNode*> AnyStates;
	if (TryGetAnyStateNodesForGraph(StateNode->GetOwningStateMachineGraph(), AnyStates))
	{
		for (USMGraphNode_AnyStateNode* AnyState : AnyStates)
		{
			if (DoesAnyStateImpactOtherNode(AnyState, StateNode))
			{
				if (OutAllAnyStates)
				{
					OutAllAnyStates->Add(AnyState);
					continue;
				}
				return true;
			}
		}
	}

	return OutAllAnyStates ? OutAllAnyStates->Num() > 0 : false;
}

bool FSMBlueprintEditorUtils::TryGetAnyStateNodesForGraph(USMGraph* Graph, TArray<USMGraphNode_AnyStateNode*>& OutNodes)
{
	check(Graph)
	OutNodes.Reset();
	
	for (const TObjectPtr<UEdGraphNode>& Node : Graph->Nodes)
	{
		if (USMGraphNode_AnyStateNode* AnyState = Cast<USMGraphNode_AnyStateNode>(Node))
		{
			OutNodes.Add(AnyState);
		}
	}

	return OutNodes.Num() > 0;
}

bool FSMBlueprintEditorUtils::DoesAnyStateImpactOtherNode(const USMGraphNode_AnyStateNode* AnyStateNode,
                                                          const USMGraphNode_StateNodeBase* OtherNode)
{
	check(AnyStateNode)
	check(OtherNode)

	if (const USMGraphNode_LinkStateNode* LinkState = Cast<USMGraphNode_LinkStateNode>(OtherNode))
	{
		OtherNode = LinkState->GetLinkedState();
		if (OtherNode == nullptr)
		{
			return false;
		}
	}

	if (AnyStateNode == OtherNode
		 || AnyStateNode->GetGraph() != OtherNode->GetGraph()
		 || !AnyStateNode->HasOutputConnections()
		 || OtherNode->ShouldExcludeFromAnyState()
		 || !OtherNode->CanExistAtRuntime())
	{
		return false;
	}

	if (!AnyStateNode->AnyStateTagQuery.IsEmpty())
	{
		if (!AnyStateNode->AnyStateTagQuery.Matches(OtherNode->AnyStateTags))
		{
			return false;
		}
	}

	bool bResult = false;
	for (int32 Idx = 0; Idx < AnyStateNode->GetOutputPin()->LinkedTo.Num(); ++Idx)
	{
		if (USMGraphNode_TransitionEdge* Transition = AnyStateNode->GetNextTransition(Idx))
		{
			USMGraphNode_StateNodeBase* NextState = AnyStateNode->GetNextState(Idx);
			if (!AnyStateNode->bAllowInitialReentry && (OtherNode == NextState ||
				(NextState && NextState->IsA<USMGraphNode_LinkStateNode>() &&
					CastChecked<USMGraphNode_LinkStateNode>(NextState)->GetLinkedState() == OtherNode)))
			{
				// Any state only impacts if other state isn't connected directly to it.
				return false;
			}
			
			bResult = true;
		}
	}

	return bResult;
}

bool FSMBlueprintEditorUtils::TryGetParentClasses(UBlueprint* Blueprint, TArray<USMBlueprintGeneratedClass*>& OutClassesOrdered)
{
	USMBlueprintGeneratedClass* Parent = Blueprint->ParentClass ? Cast<USMBlueprintGeneratedClass>(Blueprint->ParentClass.Get()) : nullptr;
	while (Parent)
	{
		OutClassesOrdered.Add(Parent);
		Parent = Cast<USMBlueprintGeneratedClass>(Parent->GetSuperClass());
	}

	return OutClassesOrdered.Num() > 0;
}

bool FSMBlueprintEditorUtils::IsStateMachineInstanceGraph(UEdGraph* GraphIn)
{
	return GraphIn->IsA<USMGraphK2>();
}

bool FSMBlueprintEditorUtils::IsGraphConfiguredForTransitionEvents(const UEdGraph* Graph)
{
	if (!Graph || !Graph->IsA<USMTransitionGraph>())
	{
		return false;
	}

	if (GetFirstNodeOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(Graph))
	{
		return true;
	}

	if (const UBlueprint* Blueprint = FindBlueprintForGraph(Graph))
	{
		if (Blueprint->SupportsInputEvents())
		{
			return true;
		}
	}

	// Input events also allow transition events.
	return DoesGraphHaveInputEvents(Graph);
}

bool FSMBlueprintEditorUtils::TryGetVariableByName(UBlueprint* Blueprint, const FName& Name, FBPVariableDescription& VariableOut)
{
	while (Blueprint)
	{
		const int32 VarIndex = FindNewVariableIndex(Blueprint, Name);
		if (VarIndex != INDEX_NONE)
		{
			VariableOut = Blueprint->NewVariables[VarIndex];
			return true;
		}

		Blueprint = Blueprint->ParentClass ? Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy) : nullptr;
	}

	return false;
}

bool FSMBlueprintEditorUtils::TryGetVariableByGuid(UBlueprint* Blueprint, const FGuid& Guid,
	FBPVariableDescription& VariableOut)
{
	while (Blueprint)
	{
		for (FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			if (Variable.VarGuid == Guid)
			{
				VariableOut = Variable;
				return true;
			}
		}

		Blueprint = Blueprint->ParentClass ? Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy) : nullptr;
	}

	return false;
}

FProperty* FSMBlueprintEditorUtils::GetPropertyByName(const UBlueprint* InBlueprint, const FName& InName)
{
	check(InBlueprint);
	
	FProperty* Property = FindFProperty<FProperty>(InBlueprint->SkeletonGeneratedClass, InName);
	if (Property)
	{
		return FBlueprintEditorUtils::GetMostUpToDateProperty(Property);
	}

	Property = FMemberReference::FindRemappedField<FProperty>(InBlueprint->SkeletonGeneratedClass, InName);
	if (Property)
	{
		return FBlueprintEditorUtils::GetMostUpToDateProperty(Property);
	}
	
	Property = FindFProperty<FProperty>(InBlueprint->GeneratedClass, InName);
	if (Property)
	{
		return FBlueprintEditorUtils::GetMostUpToDateProperty(Property);
	}

	Property = FMemberReference::FindRemappedField<FProperty>(InBlueprint->GeneratedClass, InName);
	if (Property)
	{
		return FBlueprintEditorUtils::GetMostUpToDateProperty(Property);
	}
	
	return nullptr;
}

void FSMBlueprintEditorUtils::CleanReferenceTemplates(USMInstance* Template)
{
	/*
	 * When instantiating a template it will load defaults from the CDO of the respective class.
	 * If that class has its own templates it will then construct them as part of this template
	 * and include them in the parent package. This isn't necessary and we don't need to export them because
	 * each template is just used as a single achetype during run time and only for user generated blueprint values.
	 * The instantiated class will read its nested template values from that class CDO, not this template.
	 *
	 * It also increases size of BP Nativization files because the constructor of the super template will
	 * fill out details of all nested templates.
	 *
	 * Normally Transient should only serialize properties only for the CDO, but again that doesn't work with
	 * BP Nativization.
	 *
	 * So instead we are recursively destroying all nested templates within this template and relying on
	 * those template's class default objects to construct them.
	 */

	if (!Template)
	{
		return;
	}

	TSet<USMInstance*> NestedTemplates;
	USMUtils::TryGetAllReferenceTemplatesFromInstance(Template, NestedTemplates, true);

	for (USMInstance* NestedTemplate : NestedTemplates)
	{
		TrashObject(NestedTemplate);
		NestedTemplate->ReferenceTemplates.Empty();
	}

	Template->ReferenceTemplates.Empty();
}

void FSMBlueprintEditorUtils::TrashObject(UObject* Object)
{
	if (!Object)
	{
		return;
	}
	const ERenameFlags RenFlags = REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_DoNotDirty;
	FName TrashName = *("TRASH_" + Object->GetName());

	if (UEdGraph* Graph = Cast<UEdGraph>(Object))
	{
		RemoveAllNodesFromGraph(Graph, nullptr, false, false, true);
	}
	
	TrashName = MakeUniqueObjectName(GetTransientPackage(), Object->GetClass(), TrashName);
	Object->Rename(*TrashName.ToString(), GetTransientPackage(), RenFlags);
	Object->SetFlags(RF_Transient);
	Object->RemoveFromRoot();
	FLinkerLoad::InvalidateExport(Object);
}

void FSMBlueprintEditorUtils::InvalidateCaches(const UBlueprint* InBlueprint, const FCacheInvalidationArgs& InInvalidationArgs)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMBlueprintEditorUtils::InvalidateCaches"), STAT_InvalidateCaches, STATGROUP_LogicDriverEditor);

	if (!GIsTransacting || InInvalidationArgs.bAllowIfTransacting)
	{
		if (const USMBlueprint* SMBlueprint = Cast<USMBlueprint>(InBlueprint))
		{
			if (!SMBlueprint->bPreventCacheInvalidation && (!InBlueprint->bBeingCompiled || InInvalidationArgs.bAllowDuringCompile))
			{
				OnCacheClearedEvent.Broadcast(SMBlueprint);
			}
		}
	}
}

FSMBlueprintEditorUtils::FOnCacheCleared FSMBlueprintEditorUtils::OnCacheClearedEvent;

FSMBlueprintEditorUtils::FBulkCacheInvalidation::FBulkCacheInvalidation(USMBlueprint* InBlueprint,
	const FCacheInvalidationArgs& InInvalidationArgs)
{
	if (InBlueprint)
	{
		InBlueprint->bPreventCacheInvalidation = true;
	}

	Blueprint = InBlueprint;
	InvalidationArgs = InInvalidationArgs;
}

FSMBlueprintEditorUtils::FBulkCacheInvalidation::~FBulkCacheInvalidation()
{
	if (Blueprint.IsValid())
	{
		Blueprint->bPreventCacheInvalidation = false;
		InvalidateCaches(Blueprint.Get(), InvalidationArgs);
	}
}

/* Looks for Composite nodes that have no bound graph then attempt to find that graph and link it to the node.*/
void FSMBlueprintEditorUtils::FixUpCollapsedGraphs(UEdGraph* TopLevelGraph)
{
	if (!TopLevelGraph)
	{
		return;
	}

	TArray<UK2Node_Composite*> CompositeNodes;
	GetAllNodesOfClassNested<UK2Node_Composite>(TopLevelGraph, CompositeNodes);

	if (CompositeNodes.Num() == 0)
	{
		return;
	}

	// Collect all known graphs.
	TSet<UEdGraph*> ChildGraphs;
	GetAllGraphsOfClassNested<UEdGraph>(TopLevelGraph, ChildGraphs);

	// Convert to array for predicate lookup.
	TArray<UEdGraph*> ChildGraphsArr(ChildGraphs.Array());

	for (UK2Node_Composite* Composite : CompositeNodes)
	{
		// This is an "Invalid Graph" node.
		if (Composite->BoundGraph == nullptr)
		{
			// Find the actual graph.
			UEdGraph** FoundGraph = ChildGraphsArr.FindByPredicate([&](const UEdGraph* Graph)
			{
				return Graph->GetOuter() == Composite;
			});

			// Relink it.
			if (FoundGraph)
			{
				Composite->BoundGraph = *FoundGraph;
				Composite->PostEditUndo();
			}

			// Fix up the node so it displays properly.
			Composite->ReconstructNode();
		}
	}
}

/* Looks for graph nodes that contain duplicate ids and change. */
void FSMBlueprintEditorUtils::FixUpDuplicateGraphNodeGuids(UBlueprint* Blueprint)
{
	check(Blueprint);
	
	TSet<FGuid> NodeGuids;
	NodeGuids.Reserve(200);

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!IsStateMachineInstanceGraph(Graph))
		{
			continue;
		}

		TSet<USMGraphK2*> ChildGraphSet;
		GetAllGraphsOfClassNested<USMGraphK2>(Graph, ChildGraphSet);

		TArray<USMGraphK2*> ChildGraphs(ChildGraphSet.Array());

		for (int32 Index = 0; Index < ChildGraphs.Num(); ++Index)
		{
			UEdGraph* ChildGraph = ChildGraphs[Index];

			for (const TObjectPtr<UEdGraphNode>& Node : ChildGraph->Nodes)
			{
				// Could be null if node class changed from a breaking plugin version update.
				if (!Node)
				{
					continue;
				}
				if (NodeGuids.Contains(Node->NodeGuid))
				{
					Node->CreateNewGuid();
				}
				else
				{
					NodeGuids.Add(Node->NodeGuid);
				}
			}
		}
	}
}

int32 FSMBlueprintEditorUtils::FixUpDuplicateRuntimeGuids(UBlueprint* Blueprint, FCompilerResultsLog* MessageLog)
{
	int32 TotalFixed = 0;
	TMap<FGuid, TArray<UEdGraphNode*>> RuntimeNodes;
	if (FindNodesWithDuplicateRuntimeGuids(Blueprint, RuntimeNodes))
	{
		for (const auto& KeyVal : RuntimeNodes)
		{
			if (KeyVal.Value.Num() < 2)
			{
				continue;
			}
			for (UEdGraphNode* Node : KeyVal.Value)
			{
				// First locate a container within this blueprint.
				UEdGraphNode* ThisNode = nullptr;

				// First locate a container within this blueprint.
				for (UEdGraphNode* OtherNode : KeyVal.Value)
				{
					if (FindBlueprintForNode(OtherNode) == Blueprint)
					{
						ThisNode = OtherNode;
						break;
					}
				}

				// There are duplicates but none in this blueprint. Let the parent class handle it during compile.
				if (!ThisNode)
				{
					continue;
				}

				// This is a duplicate because the parent has it. Most likely a BP was copied and pasted then reparented to the original.
				UBlueprint* OwningBlueprint = FindBlueprintForNode(Node);
				if (OwningBlueprint != Blueprint)
				{
					TotalFixed++;

					FSMNode_Base* RuntimeNode = GetRuntimeNodeFromExactNodeChecked(Node);
					RuntimeNode->GenerateNewNodeGuid();
					UpdateRuntimeNodeForBlueprint(RuntimeNode->GetNodeGuid(), RuntimeNode, Blueprint);

					if (MessageLog)
					{
						MessageLog->Warning(TEXT("Node @@ has duplicate runtime GUID with @@ from parent blueprint @@. Automatically fixing. Please save the package @@."), ThisNode, Node, OwningBlueprint, Blueprint);
					}
				}
				else
				{
					// Don't fix this one, fix others. ie Leave one with original guid, arbitrarily chosen as the first in the array within this blueprint.
					if (Node == ThisNode)
					{
						continue;
					}

					TotalFixed++;

					FSMNode_Base* RuntimeNode = GetRuntimeNodeFromExactNodeChecked(Node);
					RuntimeNode->GenerateNewNodeGuid();
					UpdateRuntimeNodeForBlueprint(RuntimeNode->GetNodeGuid(), RuntimeNode, Blueprint);

					if (MessageLog)
					{
						MessageLog->Warning(TEXT("Node @@ has duplicate runtime GUID with @@. Automatically fixing. This could have occurred by manually setting the NodeGuid or by duplicating certain nodes in earlier versions of the plugin. Please save the package @@."), Node, ThisNode, Blueprint);
					}
				}
			}
		}
	}

	return TotalFixed;
}

int32 FSMBlueprintEditorUtils::FixUpMismatchedRuntimeGuids(UBlueprint* Blueprint, FCompilerResultsLog* MessageLog)
{
	check(Blueprint);
	
	int32 TotalFixed = 0;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		TArray<USMGraphK2Node_RuntimeNodeReference*> References;
		GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeReference>(Graph, References);

		TSet<USMGraphK2Node_RuntimeNodeContainer*> ContainersUpdated;

		for (USMGraphK2Node_RuntimeNodeReference* Reference : References)
		{
			// Don't repeat, the call to update runtime node would have fixed it for the container and all references.
			USMGraphK2Node_RuntimeNodeContainer* Container = Reference->GetRuntimeContainer();
			if (!Container || ContainersUpdated.Contains(Container))
			{
				continue;
			}
			ContainersUpdated.Add(Container);

			FSMNode_Base* RuntimeNode = Container->GetRunTimeNodeChecked();
			if (RuntimeNode->GetNodeGuid() != Reference->RuntimeNodeGuid)
			{
				TotalFixed++;

				const UEdGraph* TopLevelGraph = FindTopLevelOwningGraph(Reference->GetGraph());
				UpdateRuntimeNodeForNestedGraphs(RuntimeNode->GetNodeGuid(), RuntimeNode, TopLevelGraph);

				if (MessageLog)
				{
					MessageLog->Warning(TEXT("Reference node @@ has mismatched Guid with container node @@. Automatically fixing. Please save the package @@."), Reference, Container, Blueprint);
				}
			}
		}
	}

	return TotalFixed;
}

void FSMBlueprintEditorUtils::FixUpAutoGeneratedFunctions(UBlueprint* Blueprint, bool bFocusTab, FCompilerResultsLog* MessageLog)
{
	check(Blueprint);

	static TSet<UBlueprint*> BlueprintsInProgress;
	if (BlueprintsInProgress.Contains(Blueprint))
	{
		return;
	}

	BlueprintsInProgress.Add(Blueprint);
	
	TArray<USMGraphK2*> GraphsToFix;
	
	for (const TObjectPtr<UEdGraph>& Graph : Blueprint->FunctionGraphs)
	{
		if (USMGraphK2* SMGraphK2 = Cast<USMGraphK2>(Graph))
		{
			GraphsToFix.Add(SMGraphK2);
		}
	}

	for (USMGraphK2* SMGraphK2 : GraphsToFix)
	{
		FString OriginalGraphName = SMGraphK2->GetName();
		FString TemporaryGraphName = OriginalGraphName + TEXT("_") + FGuid::NewGuid().ToString();

		if (UEdGraph* NewGraph = CreateNewGraph(Blueprint, *TemporaryGraphName, UEdGraph::StaticClass(), GetDefault<UEdGraphSchema_K2>()->GetClass()))
		{
			SMGraphK2->MoveNodesToAnotherGraph(NewGraph, true, MessageLog != nullptr);

			TArray<USMGraphK2Node_Base*> NodesToRemove;
			GetAllNodesOfClassNested<USMGraphK2Node_Base>(NewGraph, NodesToRemove);
			for (USMGraphK2Node_Base* Node : NodesToRemove)
			{
				RemoveNode(Blueprint, Node, true);
			}
			
			RemoveGraph(Blueprint, SMGraphK2);

			NewGraph->Rename(*OriginalGraphName, nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			Blueprint->FunctionGraphs.Add(NewGraph);

			if (bFocusTab)
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NewGraph);
			}
			
			if (MessageLog)
			{
				FString Message = FString::Printf(TEXT("Cleaned up incorrect auto generated function graph %s."), *OriginalGraphName);
				MessageLog->Note(*Message);
			}
		}
		else
		{
			if (MessageLog)
			{
				FString Message = FString::Printf(TEXT("Could not clean up incorrect auto generated function graph %s. This graph may need to be deleted."), *OriginalGraphName);
				MessageLog->Warning(*Message);
			}
		}
	}

	BlueprintsInProgress.Remove(Blueprint);
}

bool FSMBlueprintEditorUtils::FindNodesWithDuplicateRuntimeGuids(UBlueprint* Blueprint, TMap<FGuid, TArray<UEdGraphNode*>>& RuntimeNodes)
{
	UBlueprint* CurrentBlueprint = Blueprint;

	while (CurrentBlueprint != nullptr)
	{
		for (UEdGraph* Graph : CurrentBlueprint->UbergraphPages)
		{
			FindNodesWithDuplicateRuntimeGuids(Graph, RuntimeNodes);
		}

		CurrentBlueprint = UBlueprint::GetBlueprintFromClass(CurrentBlueprint->ParentClass);
	}

	for (const auto& KeyVal : RuntimeNodes)
	{
		if (KeyVal.Value.Num() > 1)
		{
			return true;
		}
	}

	return false;
}

bool FSMBlueprintEditorUtils::FindNodesWithDuplicateRuntimeGuids(UEdGraph* Graph, TMap<FGuid, TArray<UEdGraphNode*>>& RuntimeNodes)
{
	TArray<USMGraphNode_StateMachineEntryNode*> StateMachineEntryNodes;
	TArray<USMGraphK2Node_RuntimeNodeContainer*> RuntimeNodeContainers;
	GetAllNodesOfClassNested<USMGraphK2Node_RuntimeNodeContainer>(Graph, RuntimeNodeContainers);
	GetAllNodesOfClassNested<USMGraphNode_StateMachineEntryNode>(Graph, StateMachineEntryNodes);

	for (USMGraphK2Node_RuntimeNodeContainer* Container : RuntimeNodeContainers)
	{
		if (Container->IsA<USMGraphK2Node_StateMachineEntryNode>() && !Container->IsA<USMGraphK2Node_IntermediateEntryNode>())
		{
			// These nodes are compiler generated based on entry nodes which are checked below.
			continue;
		}

		FSMNode_Base* RuntimeNode = Container->GetRunTimeNodeChecked();
		TArray<UEdGraphNode*>& DuplicateNodes = RuntimeNodes.FindOrAdd(RuntimeNode->GetNodeGuid());
		DuplicateNodes.Add(Container);
	}

	for (USMGraphNode_StateMachineEntryNode* EntryNode : StateMachineEntryNodes)
	{
		UEdGraphNode* NodeToAdd = EntryNode;
		FSMNode_Base* RuntimeNode = &EntryNode->StateMachineNode;

		// Lookup the correct runtime node from the container which may have already been generated.
		if (USMGraph* SMGraph = Cast<USMGraph>(EntryNode->GetGraph()))
		{
			if (SMGraph->GeneratedContainerNode)
			{
				RuntimeNode = SMGraph->GeneratedContainerNode->GetRunTimeNode();
				NodeToAdd = SMGraph->GeneratedContainerNode;
			}
		}
		
		TArray<UEdGraphNode*>& DuplicateNodes = RuntimeNodes.FindOrAdd(RuntimeNode->GetNodeGuid());
		DuplicateNodes.Add(NodeToAdd);
	}

	for (const auto& KeyVal : RuntimeNodes)
	{
		if (KeyVal.Value.Num() > 1)
		{
			return true;
		}
	}

	return false;
}

void FSMBlueprintEditorUtils::CleanUpIsolatedTransitions(UEdGraph * Graph)
{
	check(Graph);

	TSet<UEdGraphNode*> NodesToRemove;
	for (const TObjectPtr<UEdGraphNode>& Node : Graph->Nodes)
	{
		if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Node))
		{
			// No connection to this transition.
			if (Transition->GetFromState() == nullptr || Transition->GetToState() == nullptr)
			{
				NodesToRemove.Add(Node);
				continue;
			}

			// The connections are on different graphs. Likely collapsed to a new state machine.
			if (Transition->GetFromState()->GetGraph() != Graph || Transition->GetToState()->GetGraph() != Graph)
			{
				NodesToRemove.Add(Node);
			}
		}
	}

	UBlueprint* Blueprint = FindBlueprintForGraphChecked(Graph);

	for (UEdGraphNode* Node : NodesToRemove)
	{
		RemoveNode(Blueprint, Node, true);
	}

	Graph->Modify();
}

void FSMBlueprintEditorUtils::CleanupInvalidPropertyGraphs(UBlueprint* InBlueprint, FCompilerResultsLog* MessageLog)
{
	TArray<TTuple<UEdGraph*, USMPropertyGraph*>> OutGraphs;

	USMGraph* SMGraph = GetRootStateMachineGraph(InBlueprint);
	if (!SMGraph)
	{
		return;
	}
	
	GetAllGraphsOfClassNestedWithParents<USMPropertyGraph>(SMGraph, OutGraphs);

	for (const TTuple<UEdGraph*, USMPropertyGraph*>& GraphTuple : OutGraphs)
	{
		USMPropertyGraph* PropertyGraph = GraphTuple.Value;
		if (!PropertyGraph)
		{
			continue;
		}
		if (FBlueprintEditorUtils::FindBlueprintForGraph(PropertyGraph) == nullptr || PropertyGraph->HasAnyFlags(RF_BeginDestroyed))
		{
			UEdGraph* ActualParentGraph = GraphTuple.Key;
			
			if (MessageLog)
			{
				MessageLog->Warning(TEXT("Removing property graph @@ because it is either missing a blueprint owner or has been destroyed."),
					PropertyGraph);
			}
			else
			{
				LDEDITOR_LOG_WARNING(TEXT("Removing property graph %s in blueprint %s because it is either missing a blueprint owner or has been destroyed."),
					*PropertyGraph->GetName(), *InBlueprint->GetName());
			}
			
			RemoveAllNodesFromGraph(GraphTuple.Value, InBlueprint, false, false, true);
			PropertyGraph->ResultNode = nullptr;

			// The parent that actually owns this graph.
			if (ActualParentGraph != nullptr)
			{
				ActualParentGraph->SubGraphs.Remove(PropertyGraph);
			}
			
			// The outer of the property graph. This graph may not be assigned the property graph.
			if (UEdGraph* CurrentParentGraph = Cast<UEdGraph>(PropertyGraph->GetOuter()))
			{
				CurrentParentGraph->SubGraphs.Remove(PropertyGraph);
			}
			
			RemoveGraph(InBlueprint, PropertyGraph, EGraphRemoveFlags::None);
			PropertyGraph->ConditionalBeginDestroy();
		}
	}
}

void FSMBlueprintEditorUtils::CleanupInvalidParentFunctionsInChildren(const UBlueprint* InBlueprint)
{
	// Check if we are being called from a child state machine. We have to clear out stale functions here because
	// UE will attempt to fixup bytecode references of the current functions, which will be invalid in the child
	// at this point. See GitHub issue #200.
	if (InBlueprint && InBlueprint->GeneratedClass)
	{
		TArray<UClass*> ChildClasses;
		GetDerivedClasses(InBlueprint->GeneratedClass, ChildClasses);
		for (const UClass* ChildClass : ChildClasses)
		{
			if (UBlueprint* ChildBlueprint = UBlueprint::GetBlueprintFromClass(ChildClass))
			{
				// Verify we're only on an SM generated class. It could be a macro library based off of an SM which will crash.
				if (USMBlueprintGeneratedClass* SMBPGC = Cast<USMBlueprintGeneratedClass>(ChildBlueprint->GeneratedClass))
				{
					if (ChildBlueprint->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad) || ChildBlueprint->bIsNewlyCreated)
					{
						continue;
					}
					
					if (const UEdGraph* TopLevelStateMachineGraph = GetTopLevelStateMachineGraph(ChildBlueprint))
					{
						TArray<USMGraphNode_StateMachineParentNode*> ParentCalls;
						GetAllNodesOfClassNested(TopLevelStateMachineGraph, ParentCalls);

						if (ParentCalls.Num() > 0)
						{
							// set bIsRegeneratingOnLoad so that we don't reset loaders, can't use TGuard for uint8:1.
							const bool bIsRegeneratingOnLoad = ChildBlueprint->bIsRegeneratingOnLoad;
							ChildBlueprint->bIsRegeneratingOnLoad = true;
							FBlueprintEditorUtils::RemoveStaleFunctions(Cast<UBlueprintGeneratedClass>(ChildBlueprint->GeneratedClass), ChildBlueprint);
							ChildBlueprint->bIsRegeneratingOnLoad = bIsRegeneratingOnLoad;
						}
					}
				}
			}
		}
	}
}

UK2Node_CallFunction* FSMBlueprintEditorUtils::CreateFunctionCall(UEdGraph* Graph, UFunction* Function)
{
	UK2Node_CallFunction* ExecuteNode = NewObject<UK2Node_CallFunction>(Graph);
	UFunction* MakeExecuteNodeFunction = Function;
	ExecuteNode->CreateNewGuid();
	ExecuteNode->PostPlacedNewNode();
	ExecuteNode->SetFromFunction(MakeExecuteNodeFunction);
	ExecuteNode->SetFlags(RF_Transactional);
	ExecuteNode->AllocateDefaultPins();
	Graph->AddNode(ExecuteNode);

	return ExecuteNode;
}

UK2Node_CallParentFunction* FSMBlueprintEditorUtils::CreateParentFunctionCall(UEdGraph* Graph, UFunction* ParentFunction, UEdGraphNode* ChildNode, int32 XPostion, TOptional<int32> YPosition, bool bIsDefault)
{
	FGraphNodeCreator<UK2Node_CallParentFunction> FunctionNodeCreator(*Graph);
	UK2Node_CallParentFunction* ParentFunctionNode = FunctionNodeCreator.CreateNode();
	ParentFunctionNode->SetFromFunction(ParentFunction);
	ParentFunctionNode->AllocateDefaultPins();

	for (UEdGraphPin* EventPin : ChildNode->Pins)
	{
		if (UEdGraphPin* ParentPin = ParentFunctionNode->FindPin(EventPin->PinName, EGPD_Input))
		{
			ParentPin->MakeLinkTo(EventPin);
		}
	}
	ParentFunctionNode->GetExecPin()->MakeLinkTo(ChildNode->FindPin(UEdGraphSchema_K2::PN_Then));

	ParentFunctionNode->NodePosX = ChildNode->NodePosX + ChildNode->NodeWidth + XPostion;
	ParentFunctionNode->NodePosY = YPosition.IsSet() ? *YPosition : ChildNode->NodePosY;
	if (bIsDefault)
	{
		UEdGraphSchema_K2::SetNodeMetaData(ParentFunctionNode, FNodeMetadata::DefaultGraphNode);
	}

	FunctionNodeCreator.Finalize();

	if (bIsDefault)
	{
		ParentFunctionNode->MakeAutomaticallyPlacedGhostNode();

		// Needs to be reset. Even if ghost node the wiring will have canceled that.
		ChildNode->MakeAutomaticallyPlacedGhostNode();
	}

	return ParentFunctionNode;
}

bool FSMBlueprintEditorUtils::CanStateMachineBePlacedInGraph(const USMGraph* Graph,
                                                             FSMStateMachineNodePlacementValidator& OutRules)
{
	if (const UClass* OwningStateMachineClass = GetStateMachineClassFromGraph(Graph))
	{
		if (const USMStateMachineInstance* StateMachineDefault = Cast<USMStateMachineInstance>(OwningStateMachineClass->GetDefaultObject()))
		{
			const FSMStateMachineNodePlacementValidator& Rules = StateMachineDefault->GetAllowedNodes();
			OutRules = Rules;

			return Rules.bAllowSubStateMachines && Rules.IsStateAllowed(!Rules.DefaultSubStateMachineClass.IsNull() ?
				Rules.DefaultSubStateMachineClass : USMStateMachineInstance::StaticClass());
		}
	}

	return true;
}

bool FSMBlueprintEditorUtils::CanStateMachineBeConvertedToReference(USMGraph* Graph)
{
	if (UClass* StateMachineClass = GetStateMachineClassFromGraph(Graph))
	{
		if (USMStateMachineInstance* StateMachineDefault = Cast<USMStateMachineInstance>(StateMachineClass->GetDefaultObject()))
		{
			const FSMStateMachineNodePlacementValidator& Rules = StateMachineDefault->GetAllowedNodes();
			return Rules.bAllowReferences;
		}
	}

	return true;
}

USMGraphNode_StateMachineStateNode* FSMBlueprintEditorUtils::CollapseNodesAndCreateStateMachine(const TSet<UObject*>& InNodes)
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "CollapseNodesToStateMachine", "Collapse To State Machine"));

	TSet<UObject*> Nodes = InNodes;

	USMGraphNode_StateNodeBase* FirstStateNode = nullptr;
	USMGraphNode_StateNodeBase* SampleState = nullptr;

	TSet<USMGraphNode_StateNodeBase*> InnerStates;
	TSet<USMGraphNode_TransitionEdge*> TransitionsToNewSM;
	TSet<USMGraphNode_TransitionEdge*> TransitionsFromNewSM;

	UEdGraphPin* EntryPin = nullptr;

	// Make sure all transitions between reroute nodes are selected. They generally aren't because the icon is hidden.
	for (UObject* Node : InNodes)
	{
		if (USMGraphNode_RerouteNode* Reroute = Cast<USMGraphNode_RerouteNode>(Node))
		{
			if (USMGraphNode_TransitionEdge* TransitionEdge = Reroute->GetPrimaryTransition())
			{
				TArray<USMGraphNode_TransitionEdge*> ReroutedTransitions;
				TArray<USMGraphNode_RerouteNode*> RerouteNodes;
				TransitionEdge->GetAllReroutedTransitions(ReroutedTransitions, RerouteNodes);

				Nodes.Append(*reinterpret_cast<TArray<UObject*>*>(&ReroutedTransitions));
				Nodes.Append(*reinterpret_cast<TArray<UObject*>*>(&RerouteNodes));
			}
		}
	}

	// Build up states 1 edge out of the selection and all transitions to and from the selection.
	for (UObject* Node : Nodes)
	{
		// Any state base.
		if (USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(Node))
		{
			InnerStates.Add(StateNode);

			if (SampleState == nullptr)
			{
				SampleState = StateNode;
			}

			// The pins going in or out of this state.
			for (UEdGraphPin* Pin : StateNode->Pins)
			{
				// The pins to the connected state.
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					// The transition to or from this state.
					if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(LinkedPin->GetOwningNode()))
					{
						if (Pin->Direction == EGPD_Input)
						{
							USMGraphNode_StateNodeBase* OuterNode = Transition->GetFromState();
							if (!Nodes.Contains(OuterNode))
							{
								if (USMGraphNode_TransitionEdge* PrimaryTransition = Transition->GetPrimaryReroutedTransition())
								{
									TransitionsToNewSM.Add(PrimaryTransition);
								}
								// Just assume the first edge transition to is the entry point. We can't really know which is correct with multiple entries.
								if (FirstStateNode == nullptr && !StateNode->IsA<USMGraphNode_RerouteNode>())
								{
									FirstStateNode = StateNode;
								}
							}
						}
						else if (Pin->Direction == EGPD_Output)
						{
							USMGraphNode_StateNodeBase* OuterNode = Transition->GetToState();
							if (!Nodes.Contains(OuterNode))
							{
								if (USMGraphNode_TransitionEdge* PrimaryTransition = Transition->GetPrimaryReroutedTransition())
								{
									TransitionsFromNewSM.Add(PrimaryTransition);
								}
							}
						}
					}
					else if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(LinkedPin->GetOwningNode()))
					{
						EntryPin = LinkedPin;

						// Always make the original state node the start node if possible.
						FirstStateNode = StateNode;
					}
				}
			}
		}
	}

	if (SampleState == nullptr)
	{
		return nullptr;
	}

	USMGraph* GraphOwner = CastChecked<USMGraph>(SampleState->GetGraph());

	FSMStateMachineNodePlacementValidator Validator;
	if (!CanStateMachineBePlacedInGraph(GraphOwner, Validator))
	{
		return nullptr;
	}
	
	// Create the new state machine node.
	FSMGraphSchemaAction_NewNode AddNodeAction;
	AddNodeAction.GraphNodeTemplate = NewObject<USMGraphNode_StateMachineStateNode>();
	AddNodeAction.NodeSoftClassPath = Validator.DefaultSubStateMachineClass;
	
	USMGraphNode_StateMachineStateNode* NewStateMachine = Cast<USMGraphNode_StateMachineStateNode>(AddNodeAction.PerformAction(GraphOwner, nullptr,
		FVector2D(SampleState->NodePosX, SampleState->NodePosY), false));

	// First wire the outer transitions to the new state machine.
	for (USMGraphNode_TransitionEdge* Transition : TransitionsToNewSM)
	{
		if (!NewStateMachine->HasTransitionFromNode(Transition->GetFromState()))
		{
			GraphOwner->GetSchema()->TryCreateConnection(Transition->GetLastReroutedTransition()->GetOutputPin(), NewStateMachine->GetInputPin());
			Transition->UpdatePrimaryTransition();
		}
	}

	for (USMGraphNode_TransitionEdge* Transition : TransitionsFromNewSM)
	{
		if (!NewStateMachine->HasTransitionToNode(Transition->GetToState()))
		{
			GraphOwner->GetSchema()->TryCreateConnection(NewStateMachine->GetOutputPin(), Transition->GetFirstReroutedTransition()->GetInputPin());
			Transition->UpdatePrimaryTransition();
		}
	}

	// Reconnect the entry pin to the new state machine if applicable.
	if (EntryPin != nullptr)
	{
		GraphOwner->GetSchema()->BreakPinLinks(*EntryPin, true);
		GraphOwner->GetSchema()->TryCreateConnection(EntryPin, NewStateMachine->GetInputPin());
	}

	USMGraph* DestinationGraph = CastChecked<USMGraph>(NewStateMachine->GetBoundGraph());
	DestinationGraph->Nodes.Reserve(Nodes.Num());

	// Now move all of the selected nodes to the new state machine.
	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineEntryNode* EntryNode = Cast<USMGraphNode_StateMachineEntryNode>(Node))
		{
			continue;
		}

		// Prevent transitions/reroutes from being moved if they're wired to the new state machine.
		USMGraphNode_TransitionEdge* PrimaryTransition = nullptr;
		if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Node))
		{
			PrimaryTransition = Transition->GetPrimaryReroutedTransition();
		}
		else if (USMGraphNode_RerouteNode* RerouteNode = Cast<USMGraphNode_RerouteNode>(Node))
		{
			PrimaryTransition = RerouteNode->GetPrimaryTransition();
		}

		if (PrimaryTransition)
		{
			// Outer edges which are selected should be ignored since they will point to the new state machine.
			if (TransitionsToNewSM.Contains(PrimaryTransition) || TransitionsFromNewSM.Contains(PrimaryTransition))
			{
				continue;
			}

			// If a random edge is selected we don't want that either.
			if (!InnerStates.Contains(PrimaryTransition->GetFromState()) || !InnerStates.Contains(PrimaryTransition->GetToState()))
			{
				continue;
			}

			PrimaryTransition->UpdatePrimaryTransition();
		}

		if (USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(Node))
		{
			GraphOwner->Nodes.Remove(GraphNode);
			GraphOwner->SubGraphs.Remove(GraphNode->GetBoundGraph());

			GraphNode->Rename(nullptr, DestinationGraph, REN_DontCreateRedirectors);
			DestinationGraph->Nodes.Add(GraphNode);
			if (UEdGraph* BoundGraph = GraphNode->GetBoundGraph())
			{
				DestinationGraph->SubGraphs.Add(BoundGraph);
			}
		}
	}

	// Connect the new state machine entry node to the original start node.
	if (FirstStateNode != nullptr)
	{
		DestinationGraph->GetSchema()->TryCreateConnection(DestinationGraph->EntryNode->GetOutputPin(), FirstStateNode->GetInputPin());
	}

	CleanUpIsolatedTransitions(GraphOwner);

	// Perform any post move cleanup.
	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateNodeBase* StateNodeBase = Cast<USMGraphNode_StateNodeBase>(Node))
		{
			StateNodeBase->RemoveInvalidLinkStates();
		}
	}

	DestinationGraph->NotifyGraphChanged();
	GraphOwner->NotifyGraphChanged();

	return NewStateMachine;
}

void FSMBlueprintEditorUtils::CombineStates(UEdGraphNode* DestinationNode, const TSet<UObject*>& NodesToMerge, bool bDestroyStates)
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "CombineStates", "Merge States"));
	DestinationNode->Modify();
	
	USMGraphNode_StateNode* DestinationStateNode = CastChecked<USMGraphNode_StateNode>(DestinationNode);

	TSet<USMGraphNode_StateNode*> NodesMerged;
	for (UObject* Node : NodesToMerge)
	{
		if (Node == DestinationNode)
		{
			continue;
		}
		
		if (USMGraphNode_StateNode* FromStateNode = Cast<USMGraphNode_StateNode>(Node))
		{
			bool bMerged = false;
			TArray<FStateStackContainer> StatesToAdd;
			{
				// Duplicate templates.
				
				if (USMNodeInstance* NodeTemplate = FromStateNode->GetNodeTemplate())
				{
					// The initial node template converted to a new state stack.
					if (NodeTemplate->GetClass() != FromStateNode->GetDefaultNodeClass())
					{
						FStateStackContainer Container(NodeTemplate->GetClass(), Cast<USMNodeInstance>(StaticDuplicateObject(NodeTemplate, DestinationNode)));
						StatesToAdd.Add(Container);
					}
				}
				for (const FStateStackContainer& Container : FromStateNode->GetAllNodeStackTemplates())
				{
					// Any existing state stacks.
					FStateStackContainer CopiedContainer(Container);
					if (CopiedContainer.NodeStackInstanceTemplate)
					{
						CopiedContainer.NodeStackInstanceTemplate = Cast<USMNodeInstance>(StaticDuplicateObject(Container.NodeStackInstanceTemplate, DestinationNode));
					}
					StatesToAdd.Add(CopiedContainer);
				}
			}

			// Add to destination state stack.
			for (FStateStackContainer& Container : StatesToAdd)
			{
				// Save the original template guid so we can look up the original graphs later.
				const FGuid OriginalTemplateGuid = Container.TemplateGuid;

				// Will re-init with old values and a new guid.
				Container.InitTemplate(DestinationNode, true, true);
				DestinationStateNode->StateStack.Add(Container);

				DuplicateStackTemplatePropertyGraphs(FromStateNode, DestinationStateNode, Container, OriginalTemplateGuid);
				bMerged = true;
			}

			if (bMerged)
			{
				NodesMerged.Add(FromStateNode);
			}
		}
	}
	
	if (bDestroyStates && NodesMerged.Num())
	{
		for (USMGraphNode_StateNode* MergedNode : NodesMerged)
		{
			// Move valid transitions.
			TArray<USMGraphNode_TransitionEdge*> InputTransitions;
			MergedNode->GetInputTransitions(InputTransitions);

			TSet<USMGraphNode_TransitionEdge*> TransitionsToRemove;

			for (USMGraphNode_TransitionEdge* Transition : InputTransitions)
			{
				if (Transition->GetFromState() == DestinationStateNode || Transition->GetToState() == DestinationStateNode)
				{
					// Transition connects to/from destination node, no choice but to destroy it.
					TransitionsToRemove.Add(Transition);
					continue;
				}

				MoveTransition(Transition, Transition->GetFromState(), DestinationStateNode);
			}

			TArray<USMGraphNode_TransitionEdge*> OutputTransitions;
			MergedNode->GetOutputTransitions(OutputTransitions);
			
			for (USMGraphNode_TransitionEdge* Transition : OutputTransitions)
			{
				if (Transition->GetFromState() == DestinationStateNode || Transition->GetToState() == DestinationStateNode || InputTransitions.Contains(Transition))
				{
					// Transition connects to/from destination node, no choice but to destroy it. Don't double count self transitions either.
					TransitionsToRemove.Add(Transition);
					continue;
				}

				MoveTransition(Transition, DestinationStateNode, Transition->GetToState());
			}

			// Check if entry pin needs to be reconnected.
			if (UEdGraphPin* EntryPin = MergedNode->GetConnectedEntryPin())
			{
				EntryPin->GetSchema()->TryCreateConnection(EntryPin, DestinationStateNode->GetInputPin());
			}
			
			// Cleanup old node.
			UBlueprint* Blueprint = FindBlueprintForNodeChecked(MergedNode);

			for (USMGraphNode_TransitionEdge* Transition : TransitionsToRemove)
			{
				if (IsValid(Transition))
				{
					Transition->DestroyReroutedTransitions();
				}
			}

			RemoveNode(Blueprint, MergedNode, true);
		}
	}

	UBlueprint* Blueprint = FindBlueprintForNodeChecked(DestinationStateNode);
	//ConditionallyCompileBlueprint(Blueprint, true, true);
	{
		// Ideally we would conditionally compile the BP, but when packaging (with nativization anyway) it doesn't perform a full compile of the BP
		// which we need. When a transition is moved that changes the name of the transition template.
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave);
		MarkBlueprintAsModified(Blueprint); // Only done so the BP doesn't have a green check mark and it's obvious it still needs to be saved.
	}
}

bool FSMBlueprintEditorUtils::DuplicateStackTemplatePropertyGraphs(USMGraphNode_StateNode* FromStateNode, USMGraphNode_StateNode* DestinationStateNode,
	FStateStackContainer& NewStackContainer, const FGuid& OriginalTemplateGuid)
{
	TSet<FGuid> LiveGuids;
	if (!DestinationStateNode->CreateGraphPropertyGraphsForTemplate(NewStackContainer.NodeStackInstanceTemplate, false, LiveGuids, true))
	{
		return false;
	}

	for (const FGuid& Guid : LiveGuids)
	{
		USMPropertyGraph* NewGraph = CastChecked<USMPropertyGraph>(DestinationStateNode->GetGraphPropertyGraph(Guid));
		{
			// Remove all nodes except the result node as everything else will be copied over. There aren't normally other nodes on initial creation unless for special graph properties.
			NewGraph->ResultNode->BreakAllNodeLinks();
			NewGraph->PruneDisconnectedNodes();
		}

		// Use the saved template guid to find the original guid of this property.
		FSMGraphProperty_Base NewGraphPropertyCopy = *NewGraph->ResultNode->GetPropertyNodeConstChecked();
		NewGraphPropertyCopy.SetTemplateGuid(OriginalTemplateGuid, true);

		// Find the original property graph.
		TArray<USMGraphK2Node_PropertyNode_Base*> GraphNodesArr = FromStateNode->GetAllPropertyGraphNodesAsArray();
		USMGraphK2Node_PropertyNode_Base** OldPropertyNode = GraphNodesArr.FindByPredicate([&](const USMGraphK2Node_PropertyNode_Base* PropertyNode)
		{
			return PropertyNode->GetPropertyNodeConstChecked()->GetGuid() == NewGraphPropertyCopy.GetGuid();
		});

		if (!OldPropertyNode)
		{
			continue;
		}

		USMGraphK2Node_PropertyNode_Base* OriginalK2PropertyNode = *OldPropertyNode;
		USMPropertyGraph* OldGraph = OriginalK2PropertyNode->GetPropertyGraph();
		check(OldGraph);

		USMPropertyGraph* ClonedOldGraph = CastChecked<USMPropertyGraph>(FEdGraphUtilities::CloneGraph(OldGraph, NewGraph->GetOuter()));

		TArray<UEdGraphNode*> AllNodes;
		GetAllNodesOfClassNested<UEdGraphNode>(ClonedOldGraph, AllNodes);

		// Copy over all nodes in the graph.
		for (UEdGraphNode* OriginalGraphNode : AllNodes)
		{
			if (USMGraphK2Node_PropertyNode_Base* OriginalPropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(OriginalGraphNode))
			{
				UEdGraphPin* NewPin = NewGraph->ResultNode->GetResultPinChecked();
				UEdGraphPin* OldPin = OriginalPropertyNode->GetResultPinChecked();

				// Copy default values.
				NewPin->DefaultObject = OldPin->DefaultObject;
				NewPin->DefaultValue = OldPin->DefaultValue;
				NewPin->DefaultTextValue = OldPin->DefaultTextValue;

				NewPin->GetOwningNode()->PinDefaultValueChanged(NewPin);
				continue;
			}

			// Find the result pin so we can link it to our new pin.
			UEdGraphPin** OutResultPin = OriginalGraphNode->Pins.FindByPredicate([&](const UEdGraphPin* Pin)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (USMGraphK2Node_PropertyNode_Base* PropertyNode = Cast<USMGraphK2Node_PropertyNode_Base>(LinkedPin->GetOwningNode()))
					{
						if (PropertyNode->GetPropertyNodeChecked()->GetGuid() == ClonedOldGraph->ResultNode->GetPropertyNodeChecked()->GetGuid())
						{
							return true;
						}
					}
				}
				return false;
			});

			ClonedOldGraph->RemoveNode(OriginalGraphNode, false);
			OriginalGraphNode->Rename(nullptr, /*NewOuter=*/ NewGraph, REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
			NewGraph->AddNode(OriginalGraphNode);

			if (OutResultPin)
			{
				(*OutResultPin)->BreakLinkTo(ClonedOldGraph->ResultNode->GetResultPin());
				(*OutResultPin)->MakeLinkTo(NewGraph->ResultNode->GetResultPin());
			}

			NewGraph->OnGraphManuallyCloned(ClonedOldGraph);
			NewGraph->NotifyGraphChanged();
		}
	}

	return true;
}

void FSMBlueprintEditorUtils::MoveTransition(USMGraphNode_TransitionEdge* Transition,
                                             USMGraphNode_StateNodeBase* FromState, USMGraphNode_StateNodeBase* ToState)
{
	USMGraphNode_TransitionEdge* NewTransition = Transition;  // TODO: Support copy transition. May have to reset pin links manually.
	NewTransition->GetBoundGraph()->Modify();

	NewTransition->UpdatePrimaryTransition();
	USMGraphNode_RerouteNode* RerouteNode = NewTransition->GetNextRerouteNode();
	
	const UEdGraphSchema* Schema = Transition->GetGraph()->GetSchema();

	Schema->TryCreateConnection(NewTransition->GetOutputPin(), ToState->GetInputPin());
	Schema->TryCreateConnection(FromState->GetOutputPin(), NewTransition->GetInputPin());

	// Destroy any reroutes after the first transition was moved.
	if (RerouteNode)
	{
		if (USMGraphNode_TransitionEdge* NextTransition = RerouteNode->GetNextTransition())
		{
			NextTransition->DestroyReroutedTransitions();
		}
		else
		{
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(RerouteNode);
			FBlueprintEditorUtils::RemoveNode(Blueprint, RerouteNode, true);
		}
	}
}

void FSMBlueprintEditorUtils::PlaceNodeIfNotSet_WireNode_Internal(UEdGraphNode* NodeToWireFrom, UEdGraphNode* ToNode,
	EEdGraphPinDirection FromPinDirection, int32 DistanceFromNode)
{
	check(NodeToWireFrom->GetGraph() == ToNode->GetGraph());

	ToNode->NodePosX = NodeToWireFrom->NodePosX + DistanceFromNode;
	ToNode->NodePosY = NodeToWireFrom->NodePosY;

	for (UEdGraphPin* OutPin : NodeToWireFrom->Pins)
	{
		if (OutPin->Direction == FromPinDirection)
		{
			if (UEdGraphPin* InPin = ToNode->FindPin(OutPin->GetFName(), OutPin->Direction == EGPD_Output ? EGPD_Input : EGPD_Output))
			{
				NodeToWireFrom->GetSchema()->TryCreateConnection(OutPin, InPin);
			}
		}
	}

	if (const USMGraphK2Node_Base* SMGraphNode = Cast<USMGraphK2Node_Base>(NodeToWireFrom))
	{
		// If we're wiring from one of our nodes the exec and then pins may not be set.
		// PN_Execute and PN_Then used to be set to None on our nodes through 2.0.1.
		if (UEdGraphPin* ExecutePin = ToNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input))
		{
			UEdGraphPin* ThenPin = SMGraphNode->GetThenPin();
			if (ThenPin)
			{
				SMGraphNode->GetSchema()->TryCreateConnection(ThenPin, ExecutePin);
			}
		}
	}
}

bool FSMBlueprintEditorUtils::PlacePropertyOnGraph(UEdGraph* Graph, FProperty* Property, UEdGraphPin* DestinationPin,
                                                   UK2Node_VariableGet** VariableNodeOut, float WidthOffset, bool bAutoWireObjects)
{
	if (const UEdGraphSchema_K2* K2_Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema()))
	{
		const float VerticalSpacing = 70.f;
		FVector2D Position(WidthOffset, 100.f);

		// Based on the vertical index of the pin offset the position.
		if (DestinationPin != nullptr)
		{
			UEdGraphNode* DestinationNode = DestinationPin->GetOwningNode();
			for (int32 PinIndex = 0; PinIndex < DestinationNode->Pins.Num(); PinIndex++)
			{
				UEdGraphPin* InputPin = DestinationNode->Pins[PinIndex];
				if (InputPin == DestinationPin)
				{
					Position.Y += VerticalSpacing * (PinIndex - 1); // First index isn't an argument index.
					break;
				}
			}
		}

		// Spawn the variable getter.
		UK2Node_VariableGet* VariableNode = K2_Schema->SpawnVariableGetNode(Position, Graph, Property->GetFName(), Property->GetOwnerStruct());

		if (VariableNodeOut != nullptr)
		{
			*VariableNodeOut = VariableNode;
		}

		// Wire the variable getter to the format argument.
		if (DestinationPin != nullptr)
		{
			UEdGraphPin* ArgumentPin = VariableNode->GetValuePin();
			if (ArgumentPin == nullptr)
			{
				return false;
			}

			// Check if we're wiring an object. 4.25+ accepts an object type directly and we don't necessarily want that.
			if (!bAutoWireObjects && ArgumentPin->PinType.PinSubCategoryObject.Get() != nullptr)
			{
				return false;
			}

			// See if the text formatting input will take this argument naturally.
			if (!K2_Schema->TryCreateConnection(ArgumentPin, DestinationPin))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool FSMBlueprintEditorUtils::PlaceFunctionOnGraph(UEdGraph* Graph, UFunction* Function, UEdGraphPin* DestinationPin,
	UEdGraphNode** FunctionNodeOut, UEdGraphPin** FunctionArgumentPinOut, float WidthOffset, float VerticalOffset, bool bAutoWireObjects)
{
	if (const UEdGraphSchema_K2* K2_Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema()))
	{
		FVector2D Position(WidthOffset, VerticalOffset);

		// Based on the vertical index of the pin offset the position.
		if (DestinationPin != nullptr)
		{
			UEdGraphNode* DestinationNode = DestinationPin->GetOwningNode();
			for (int32 PinIndex = 0; PinIndex < DestinationNode->Pins.Num(); PinIndex++)
			{
				UEdGraphPin* InputPin = DestinationNode->Pins[PinIndex];
				if (InputPin == DestinationPin)
				{
					const float VerticalSpacing = 70.f;
					Position.Y += VerticalSpacing * (PinIndex - 1); // First index isn't an argument index.
					break;
				}
			}
		}

		// Spawn the function node.
		UK2Node_CallFunction* FunctionNode = CreateFunctionCall(Graph, Function);
		FunctionNode->NodePosX = Position.X;
		FunctionNode->NodePosY = Position.Y;
		
		if (FunctionNodeOut != nullptr)
		{
			*FunctionNodeOut = FunctionNode;
		}

		// Wire the variable getter to the format argument.
		if (DestinationPin != nullptr)
		{
			UEdGraphPin* ArgumentPin = FunctionNode->GetReturnValuePin();
			if (ArgumentPin == nullptr)
			{
				// Attempt lookup of any other pin.
				for (UEdGraphPin* Pin : FunctionNode->Pins)
				{
					if (Pin->Direction == EGPD_Output && !USMGraphK2Schema::IsExecPin(*Pin) && !USMGraphK2Schema::IsThenPin(Pin))
					{
						ArgumentPin = Pin;
						break;
					}
				}

				if (ArgumentPin == nullptr)
				{
					return false;
				}
			}

			if (FunctionArgumentPinOut)
			{
				*FunctionArgumentPinOut = ArgumentPin;
			}

			// Check if we're wiring an object. 4.25+ accepts an object type directly and we don't necessarily want that.
			if (!bAutoWireObjects && ArgumentPin->PinType.PinSubCategoryObject.Get() != nullptr)
			{
				return false;
			}

			// See if the text formatting input will take this argument naturally.
			if (!K2_Schema->TryCreateConnection(ArgumentPin, DestinationPin))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool FSMBlueprintEditorUtils::GetOutputProperties(UFunction* Function, TArray<FProperty*>& Outputs)
{
	if (Function)
	{
		if (FProperty* Property = Function->GetReturnProperty())
		{
			Outputs.Add(Property);
		}
		
		if (Function->HasAnyFunctionFlags(FUNC_HasOutParms))
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

			for (TFieldIterator<FProperty> ParamIt(Function, EFieldIteratorFlags::ExcludeSuper); ParamIt; ++ParamIt)
			{
				FProperty* Property = *ParamIt;

				// mirrored from UK2Node_FunctionResult::CreatePinsForFunctionEntryExit()
				const bool bIsFunctionInput = !Property->HasAnyPropertyFlags(CPF_OutParm) || Property->HasAnyPropertyFlags(CPF_ReferenceParm);
				if (bIsFunctionInput)
				{
					continue;
				}
				
				Outputs.Add(Property);
			}
		}
	}

	return Outputs.Num() > 0;
}

UEdGraphNode* FSMBlueprintEditorUtils::ConvertNodeTo_Internal(const UClass* NewClass, USMGraphNode_Base* OriginalNode,
                                                              bool bDontOverrideDefaultClass, bool bClearEditorSelection)
{
	if (OriginalNode == nullptr)
	{
		return nullptr;
	}

	USMGraph* GraphOwner = CastChecked<USMGraph>(OriginalNode->GetGraph());

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ConvertNode", "Convert Node"));

	OriginalNode->Modify();
	GraphOwner->Modify();

	// Create the new node.
	FSMGraphSchemaAction_NewNode AddNodeAction;
	AddNodeAction.bDontOverrideDefaultClass = bDontOverrideDefaultClass;
	AddNodeAction.GraphNodeTemplate = NewObject<UEdGraphNode>(GetTransientPackage(), NewClass);
	UEdGraphNode* NewNode = AddNodeAction.PerformAction(GraphOwner, nullptr,
													 FVector2D(OriginalNode->NodePosX, OriginalNode->NodePosY), false);

	const USMGraphNode_Base* NewNodeBase = CastChecked<USMGraphNode_Base>(NewNode);

	UEdGraphPin* NewInputPin = NewNodeBase->GetInputPin();
	UEdGraphPin* NewOutputPin = NewNodeBase->GetOutputPin();

	const UEdGraphPin* OldInputPin = OriginalNode->GetInputPin();
	const UEdGraphPin* OldOutputPin = OriginalNode->GetOutputPin();

	if (OldInputPin)
	{
		NewInputPin->CopyPersistentDataFromOldPin(*OldInputPin);
	}

	if (OldOutputPin)
	{
		NewOutputPin->CopyPersistentDataFromOldPin(*OldOutputPin);
	}

	// Remove the old node.
	OriginalNode->BreakAllNodeLinks();

	UBlueprint* Blueprint = FindBlueprintForNode(OriginalNode);
	RemoveNode(Blueprint, OriginalNode, true);

	GraphOwner->Modify();

	if (bClearEditorSelection)
	{
		ClearEditorSelection(Blueprint);
	}

	if (USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(NewNode))
	{
		StateNode->RefreshConnectedTransitions();
	}
		
	return NewNode;
}

USMBlueprint* FSMBlueprintEditorUtils::ConvertStateMachineToReference(USMGraphNode_StateMachineStateNode* StateMachineNode,
                                                                      bool bUserPrompt, const FString* AssetName, const FString* AssetPath, TSubclassOf<USMInstance> ParentClass)
{
	UBlueprint* Blueprint = FindBlueprintForNodeChecked(StateMachineNode);

	const UObject* AssetOuter = Blueprint->GetOuter();
	UPackage* AssetPackage = AssetOuter->GetOutermost();
	const FString NewAssetName = AssetName != nullptr ? *AssetName
		: GetProjectEditorSettings()->DefaultStateMachineBlueprintNamePrefix + StateMachineNode->GetStateName().Replace(TEXT(" "), TEXT("_"));

	// Find the folder path this asset is stored.
	FString NewAssetPath = AssetPath != nullptr ? *AssetPath : AssetPackage->GetName();
	if (AssetPath == nullptr)
	{
		// Remove the file name and go directly to the folder.
		const int LastSlashPos = NewAssetPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		NewAssetPath = NewAssetPath.Left(LastSlashPos);
	}

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	USMBlueprint* NewBlueprint = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		const UClass* CurrentClass = *It;
		if (CurrentClass->IsChildOf(USMBlueprintFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			USMBlueprintFactory* Factory = CastChecked<USMBlueprintFactory>(CurrentClass->GetDefaultObject());
			if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == USMBlueprint::StaticClass())
			{
				if (ParentClass)
				{
					Factory->SetParentClass(ParentClass);
				}
				
				if (bUserPrompt)
				{
					Factory->SetDisplayDialog(false);

					const IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
					const TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

					const TSharedPtr<SSMConvertToReferenceDialog> ConvertToReferenceDialog =
						SNew(SSMConvertToReferenceDialog, Factory)
						.OwningInstanceClass(Blueprint->GeneratedClass.Get())
						.AssetName(NewAssetName)
						.AssetPath(NewAssetPath);
					FSlateApplication::Get().AddModalWindow(ConvertToReferenceDialog.ToSharedRef(), ParentWindow);
					
					NewBlueprint = ConvertToReferenceDialog->GetNewlyCreatedBlueprint();
					
					Factory->SetDisplayDialog(true);

					if (NewBlueprint)
					{
						const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

						const TArray<UObject*> ObjectsToSync { NewBlueprint };
						ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
					}
				}
				else
				{
					const EObjectFlags Flags = RF_Public | RF_Standalone;

					// Don't use the create asset method, it has limitations to the content directory which makes unit testing difficult.
					NewBlueprint = Cast<USMBlueprint>(Factory->FactoryCreateNew(USMBlueprint::StaticClass(), AssetPackage, FName(*NewAssetName), Flags, nullptr, GWarn));
					if (NewBlueprint)
					{
						FAssetRegistryModule::AssetCreated(NewBlueprint);
						AssetPackage->MarkPackageDirty();
					}
				}
				break;
			}
		}
	}

	if (NewBlueprint == nullptr)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ConvertStateMachineToReference", "Convert State Machine To Reference"));
	StateMachineNode->Modify();
	
	// Clear out any templates / graph properties if there are any.
	const TSubclassOf<USMStateMachineInstance> StateMachineClass = StateMachineNode->StateMachineClass;
	StateMachineNode->SetNodeClass(nullptr);
	
	USMGraph* OldStateMachineGraph = Cast<USMGraph>(StateMachineNode->GetBoundGraph());

	if (OldStateMachineGraph == nullptr)
	{
		LDEDITOR_LOG_ERROR(TEXT("Cannot convert to reference, the state machine contains an invalid graph. Was a state machine reference removed?"))
		if (FApp::CanEverRender())
		{
			FNotificationInfo Info(LOCTEXT("StateMachineGraphInvalid", "State Machine contains an invalid graph. Was a state machine reference removed?"));

			Info.bUseLargeFont = false;
			Info.ExpireDuration = 5.0f;

			const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}

		return nullptr;
	}

	const USMGraphK2Node_StateMachineNode* NewRootStateMachineNode = GetRootStateMachineNode(NewBlueprint);
	USMGraph* NewStateMachineGraph = NewRootStateMachineNode->GetStateMachineGraph();

	// Remove the original entry node since it will be moved over.
	RemoveNode(NewBlueprint, NewStateMachineGraph->GetEntryNode(), true);

	// Clone the graph and move the contents to the new graph.
	USMGraph* ClonedGraph = CastChecked<USMGraph>(FEdGraphUtilities::CloneGraph(OldStateMachineGraph, NewStateMachineGraph->GetOuter()));
	ClonedGraph->MoveNodesToAnotherGraph(NewStateMachineGraph, IsAsyncLoading(), false);

	TArray<UEdGraph*> OldGraphs = ClonedGraph->SubGraphs;
	for (UEdGraph* Graph : OldGraphs)
	{
		ClonedGraph->SubGraphs.Remove(Graph);
		NewStateMachineGraph->SubGraphs.Add(Graph);
	}

	// Relink entry node.
	NewStateMachineGraph->EntryNode = ClonedGraph->EntryNode;

	// Match the old graph name.
	NewStateMachineGraph->Rename(*OldStateMachineGraph->GetName(), NewStateMachineGraph->GetOuter());

	// Graphs won't save properly without this.
	NewStateMachineGraph->Modify();

	// This graph may reference variables that won't exist.
	{
		TArray<UK2Node_Variable*> MissingVariables;
		GetAllNodesOfClassNested<UK2Node_Variable>(NewStateMachineGraph, MissingVariables);

		for (UK2Node_Variable* VariableNode : MissingVariables)
		{
			// Only add the property if it doesn't already exist.
			if (GetPropertyByName(NewBlueprint, VariableNode->GetVarName()) == nullptr)
			{
				// Find the variable in the original blueprint.
				FBPVariableDescription VariableDescription;
				if (TryGetVariableByName(Blueprint, VariableNode->GetVarName(), VariableDescription))
				{
					// Now add it to the new blueprint.
					AddMemberVariable(NewBlueprint, VariableDescription.VarName, VariableDescription.VarType, VariableDescription.DefaultValue);
				}
			}

			// Resync node with new variable.
			VariableNode->ReconstructNode();
		}
	}

	// Set class default values.
	USMInstance* NewDefaultSM = Cast<USMInstance>(NewBlueprint->GetGeneratedClass()->GetDefaultObject());
	NewDefaultSM->SetRootStateMachineNodeClass(StateMachineClass);

	// Needed for the reference template to be applied correctly.
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);
	
	StateMachineNode->ReferenceStateMachine(NewBlueprint);

	// Remove all nodes since this is now a reference.
	RemoveAllNodesFromGraph(OldStateMachineGraph, Blueprint);

	return NewBlueprint;
}

void FSMBlueprintEditorUtils::ClearEditorSelection(UObject* EditorContextObject, bool bNextTick)
{
	if (const UBlueprint* Blueprint = FindBlueprintFromObject(EditorContextObject))
	{
		const TWeakObjectPtr<const UBlueprint> BlueprintWeakPtr(Blueprint);
		
			auto ClearSelection = [BlueprintWeakPtr]()
			{
				if (BlueprintWeakPtr.IsValid())
				{
					if (FSMBlueprintEditor* Editor = GetStateMachineEditor(BlueprintWeakPtr.Get()))
					{
						Editor->ClearSelectionStateFor(FBlueprintEditor::SelectionState_Graph);
					}
				}
			};
		
			if (bNextTick && GEditor && GEditor->IsTimerManagerValid())
			{
				GEditor->GetTimerManager()->SetTimerForNextTick(ClearSelection);
			}
			else
			{
				ClearSelection();
			}
		}
}

void FSMBlueprintEditorUtils::DisableToolTipsTemporarily()
{
	static bool bDisablingToolTips = false;

	if (bDisablingToolTips || !FSlateApplication::IsInitialized())
	{
		return;
	}

	if (GEditor && FSlateApplication::Get().GetAllowTooltips() && GEditor->IsTimerManagerValid())
	{
		bDisablingToolTips = true;
		FSlateApplication::Get().SetAllowTooltips(false);
		GEditor->GetTimerManager()->SetTimerForNextTick([]()
		{
			if (GEditor && GEditor->IsTimerManagerValid())
			{
				GEditor->GetTimerManager()->SetTimerForNextTick([]()
				{
					FSlateApplication::Get().SetAllowTooltips(true);
					bDisablingToolTips = false;
				});
			}
			else
			{
				bDisablingToolTips = false;
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE
