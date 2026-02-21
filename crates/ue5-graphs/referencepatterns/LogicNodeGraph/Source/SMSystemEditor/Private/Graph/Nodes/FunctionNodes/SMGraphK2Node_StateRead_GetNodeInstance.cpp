// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_StateReadNodes.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_StructMemberGet.h"

#define LOCTEXT_NAMESPACE "SMSStateNodeInstance"

#define INSTANCE_PIN_NAME TEXT("Instance")

USMGraphK2Node_StateReadNode_GetNodeInstance::USMGraphK2Node_StateReadNode_GetNodeInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), NodeInstanceIndex(-1), bCanCreateNodeInstanceOnDemand(true)
{
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::AllocateDefaultPins()
{
	if (const TSubclassOf<UObject> TargetType = FSMBlueprintEditorUtils::GetNodeTemplateClass(GetGraph(), true, NodeInstanceGuid))
	{
		AllocatePinsForType(TargetType);
	}
}

bool USMGraphK2Node_StateReadNode_GetNodeInstance::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->GetSchema()->GetClass()->IsChildOf<USMGraphK2Schema>() && FSMBlueprintEditorUtils::GetNodeTemplateClass(Graph, true, NodeInstanceGuid) != nullptr;
}

FText USMGraphK2Node_StateReadNode_GetNodeInstance::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		if (UClass* NodeClass = FSMBlueprintEditorUtils::GetNodeTemplateClass(GetGraph(), false, NodeInstanceGuid))
		{
			FString Name = NodeClass->GetName();
			Name.RemoveFromEnd("_C");
			
			return FText::FromString(FString::Printf(TEXT("Get Node Instance '%s'"), *Name));
		}
	}

	FName NodeType = "Node";
	if (USMGraphNode_Base* NodeOwner = FSMBlueprintEditorUtils::FindTopLevelOwningNode(GetGraph()))
	{
		NodeType = NodeOwner->GetFriendlyNodeName();
	}
	
	return FText::FromString(FString::Printf(TEXT("Get %s Instance"), *NodeType.ToString()));
}

FText USMGraphK2Node_StateReadNode_GetNodeInstance::GetTooltipText() const
{
	return LOCTEXT("NodeInstanceTooltip", "Get the class instance of this node.");
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::PostPasteNode()
{
	Super::PostPasteNode();
	ReconstructNode();
}

FBlueprintNodeSignature USMGraphK2Node_StateReadNode_GetNodeInstance::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddSubObject(ReferencedObject.Get());

	return NodeSignature;
}

UObject* USMGraphK2Node_StateReadNode_GetNodeInstance::GetJumpTargetForDoubleClick() const
{
	if (ReferencedObject.Get() != nullptr && !ReferencedObject->IsNative())
	{
		if (UBlueprint* NodeBlueprint = FSMBlueprintEditorUtils::GetNodeBlueprintFromClassAndSetDebugObject(ReferencedObject.Get(),
			Cast<USMGraphNode_Base>(GetTypedOuter(USMGraphNode_Base::StaticClass()))))
		{
			return NodeBlueprint;
		}
	}
	
	return Super::GetJumpTargetForDoubleClick();
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	if (ReferencedObject == nullptr || ReferencedObject.Get() == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Referenced object no longer exists for node @@. Was the class for this node removed?"), this);
		return;
	}

	UK2Node_DynamicCast* CastNode = nullptr;
	CreateAndWireExpandedNodes(this, ReferencedObject, CompilerContext, RuntimeNodeContainer, NodeProperty, &CastNode);

	if (CastNode == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Could not create cast node for @@."), this);
		return;
	}
	
	if (CastNode->GetCastResultPin() == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Can't create cast node for @@."), this);
		return;
	}

	if (GetOutputPin() == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("No valid output pin for @@."), this);
		return;
	}
	
	CastNode->GetCastResultPin()->CopyPersistentDataFromOldPin(*GetOutputPin());
	
	BreakAllNodeLinks();
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::AllocatePinsForType(TSubclassOf<UObject> TargetType)
{
	ReferencedObject = TargetType;
	const FString CastResultPinName = UEdGraphSchema_K2::PN_CastedValuePrefix + TargetType->GetDisplayNameText().ToString();
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, *TargetType, INSTANCE_PIN_NAME);
}

UEdGraphPin* USMGraphK2Node_StateReadNode_GetNodeInstance::GetInstancePinChecked() const
{
	return FindPinChecked(INSTANCE_PIN_NAME, EGPD_Output);
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::CreateAndWireExpandedNodes(UEdGraphNode* SourceNode, TSubclassOf<UObject> Class, FSMKismetCompilerContext& CompilerContext,
	USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty, UK2Node_DynamicCast** CastOutputNode)
{
	bool bCreateStruct = true;
	if (const USMGraphK2Node_StateReadNode_GetNodeInstance* ThisNode = Cast<USMGraphK2Node_StateReadNode_GetNodeInstance>(SourceNode))
	{
		bCreateStruct = ThisNode->RequiresInstance();
	}

	// Check if there's a newer version of this class. It's possible this compile could have triggered a recompile of dependent classes.
	Class = FSMBlueprintEditorUtils::GetMostUpToDateClass(Class);
	
	if (bCreateStruct)
	{
		CreateAndWireExpandedNodesWithStruct(SourceNode, Class, CompilerContext, RuntimeNodeContainer, NodeProperty, CastOutputNode);
	}
	else
	{
		CreateAndWireExpandedNodesWithFunction(SourceNode, Class, CompilerContext, RuntimeNodeContainer, NodeProperty, CastOutputNode);
	}
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::CreateAndWireExpandedNodesWithFunction(UEdGraphNode* SourceNode, TSubclassOf<UObject> Class,
                                                                                                         FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty, UK2Node_DynamicCast** CastOutputNode)
{
	if (NodeProperty == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Node property not found for node @@."), SourceNode);
		return;
	}
	
	if (USMGraphK2Node_StateReadNode_GetNodeInstance* ThisNode = Cast<USMGraphK2Node_StateReadNode_GetNodeInstance>(SourceNode))
	{
		UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetNodeInstanceByGuid));
		const UK2Node_CallFunction* GetNodeInstanceFunction = ThisNode->CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty);
		UEdGraphPin* GetReferenceOutputPin = GetNodeInstanceFunction->GetReturnValuePin();

		UK2Node_DynamicCast* CastNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(ThisNode, CompilerContext.ConsolidatedEventGraph);
		CastNode->TargetType = FSMBlueprintEditorUtils::GetMostUpToDateClass(Class);
		CastNode->PostPlacedNewNode();
		CastNode->SetPurity(true);
		CastNode->ReconstructNode();

		ensure(ThisNode->GetSchema()->TryCreateConnection(GetReferenceOutputPin, CastNode->GetCastSourcePin()));

		if (CastOutputNode)
		{
			*CastOutputNode = CastNode;
		}
	}
}

void USMGraphK2Node_StateReadNode_GetNodeInstance::CreateAndWireExpandedNodesWithStruct(UEdGraphNode* SourceNode, TSubclassOf<UObject> Class,
                                                                                                      FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer,
                                                                                                      FProperty* NodeProperty, UK2Node_DynamicCast** CastOutputNode)
{
	if (NodeProperty == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Node property not found for node @@."), SourceNode);
		return;
	}
	
	const UEdGraphSchema* Schema = SourceNode->GetSchema();
	
	UK2Node_StructMemberGet* GetInstanceNode = CompilerContext.SpawnIntermediateNode<UK2Node_StructMemberGet>(SourceNode, CompilerContext.ConsolidatedEventGraph);
	GetInstanceNode->VariableReference.SetSelfMember(NodeProperty->GetFName());
	GetInstanceNode->StructType = RuntimeNodeContainer->GetRunTimeNodeType();
	GetInstanceNode->AllocateDefaultPins();

	UEdGraphPin* NodeInstancePin = nullptr;
	{
		// NodeInstancePin assignment.
		
		if (USMGraphK2Node_StateReadNode_GetNodeInstance* ThisNode = Cast<USMGraphK2Node_StateReadNode_GetNodeInstance>(SourceNode))
		{
			// Check if this is an array lookup for a stack template.
			if (ThisNode->NodeInstanceIndex >= 0)
			{
				UEdGraphPin* InstanceArrayPin = GetInstanceNode->FindPinChecked(TEXT("StackNodeInstances"));
				
				UK2Node_GetArrayItem* ArrayGet = CompilerContext.SpawnIntermediateNode<UK2Node_GetArrayItem>(SourceNode, CompilerContext.ConsolidatedEventGraph);
				ArrayGet->AllocateDefaultPins();
				NodeInstancePin = ArrayGet->GetResultPin();
				
				/*
				 * HACK: Set bIsReference to false otherwise 'Array Get node altered. Now returning a copy.' is displayed as a warning.
				 * The correct way to fix this is to call ArrayGet->SetDesiredReturnType(false) but that method isn't exported.
				 */
				NodeInstancePin->PinType.bIsReference = false;
				
				Schema->TryCreateConnection(InstanceArrayPin, ArrayGet->GetTargetArrayPin());
				Schema->TrySetDefaultValue(*ArrayGet->GetIndexPin(), FString::FromInt(ThisNode->NodeInstanceIndex));
			}
		}

		if (!NodeInstancePin)
		{
			// Standard template.
			NodeInstancePin = GetInstanceNode->FindPinChecked(TEXT("NodeInstance"));
		}
	}
	
	UK2Node_DynamicCast* CastNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(SourceNode, CompilerContext.ConsolidatedEventGraph);
	CastNode->TargetType = Class;
	CastNode->PostPlacedNewNode();
	CastNode->SetPurity(true);
	CastNode->ReconstructNode();

	if (CastNode->GetCastResultPin() == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Can't create cast node for @@."), SourceNode);
		return;
	}

	Schema->TryCreateConnection(NodeInstancePin, CastNode->GetCastSourcePin());

	if (CastOutputNode)
	{
		*CastOutputNode = CastNode;
	}
}


#undef LOCTEXT_NAMESPACE
