// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMGraphK2Node_StateReadNodes.h"

#include "Compilers/SMKismetCompiler.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "Graph/SMIntermediateGraph.h"

#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

#define LOCTEXT_NAMESPACE "SMStateMachineReadNodeStateMachineReference"

USMGraphK2Node_StateReadNode_GetStateMachineReference::USMGraphK2Node_StateReadNode_GetStateMachineReference(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphK2Node_StateReadNode_GetStateMachineReference::AllocateDefaultPins()
{
	if (USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GetMostRecentState()))
	{
		if (const TSubclassOf<UObject> TargetType = GetStateMachineReferenceClass())
		{
			ReferencedObject = TargetType;
			const FString CastResultPinName = UEdGraphSchema_K2::PN_CastedValuePrefix + TargetType->GetDisplayNameText().ToString();
			CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, *TargetType, TEXT("StateMachineReference"));
			return;
		}

		CreatePin(EGPD_Output, USMGraphK2Schema::PC_Object, StateMachineNode->GetStateMachineReference(), TEXT("StateMachineReference"));
	}
}

bool USMGraphK2Node_StateReadNode_GetStateMachineReference::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return Graph->IsA<USMIntermediateGraph>();
}

FText USMGraphK2Node_StateReadNode_GetStateMachineReference::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		if (USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GetMostRecentState()))
		{
			if (USMBlueprint* Blueprint = StateMachineNode->GetStateMachineReference())
			{
				return FText::FromString(FString::Printf(TEXT("Get Reference '%s'"), *Blueprint->GetName()));
			}
		}
	}

	return FText::FromString(TEXT("Get State Machine Reference"));
}

FText USMGraphK2Node_StateReadNode_GetStateMachineReference::GetTooltipText() const
{
	return LOCTEXT("StateMachineReferenceTooltip", "Get the state machine reference.");
}

void USMGraphK2Node_StateReadNode_GetStateMachineReference::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	GetMenuActions_Internal(ActionRegistrar);
}

void USMGraphK2Node_StateReadNode_GetStateMachineReference::PostPasteNode()
{
	Super::PostPasteNode();
	ReconstructNode();
}

bool USMGraphK2Node_StateReadNode_GetStateMachineReference::HasExternalDependencies(
	TArray<UStruct*>* OptionalOutput) const
{
	const UBlueprint* SourceBlueprint = GetBlueprint();
	UClass* SourceClass = *ReferencedObject;
	const bool bResult = (SourceClass != nullptr) && (SourceClass->ClassGeneratedBy != SourceBlueprint);
	if (bResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(SourceClass);
	}
	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || bResult;
}

FBlueprintNodeSignature USMGraphK2Node_StateReadNode_GetStateMachineReference::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddSubObject(ReferencedObject.Get());

	return NodeSignature;
}

UObject* USMGraphK2Node_StateReadNode_GetStateMachineReference::GetJumpTargetForDoubleClick() const
{
	if (ReferencedObject.Get())
	{
		if (const USMGraphNode_StateMachineStateNode* OwningNode =
			Cast<USMGraphNode_StateMachineStateNode>(GetTypedOuter(USMGraphNode_StateMachineStateNode::StaticClass())))
		{
			OwningNode->SetDebugObjectForReference();
			return OwningNode->GetReferenceToJumpTo();
		}
	}
	
	return Super::GetJumpTargetForDoubleClick();
}

void USMGraphK2Node_StateReadNode_GetStateMachineReference::CustomExpandNode(FSMKismetCompilerContext& CompilerContext, USMGraphK2Node_RuntimeNodeContainer* RuntimeNodeContainer, FProperty* NodeProperty)
{
	if (ReferencedObject == nullptr || ReferencedObject.Get() == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Referenced object no longer exists for node @@. Was a state machine reference removed?"), this);
		return;
	}

	UFunction* Function = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetReferencedInstanceByGuid));
	UK2Node_CallFunction* GetReferenceFunctionNode = CreateFunctionCallWithGuidInput(Function, CompilerContext, RuntimeNodeContainer, NodeProperty);
	UEdGraphPin* GetReferenceOutputPin = GetReferenceFunctionNode->GetReturnValuePin();

	UK2Node_DynamicCast* CastNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(this, CompilerContext.ConsolidatedEventGraph);
	CastNode->TargetType = ReferencedObject;
	CastNode->PostPlacedNewNode();
	CastNode->SetPurity(true);
	CastNode->ReconstructNode();

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

	GetSchema()->TryCreateConnection(GetReferenceOutputPin, CastNode->GetCastSourcePin());
	CastNode->GetCastResultPin()->CopyPersistentDataFromOldPin(*GetOutputPin());
	
	BreakAllNodeLinks();
}

TSubclassOf<UObject> USMGraphK2Node_StateReadNode_GetStateMachineReference::GetStateMachineReferenceClass() const
{
	if (USMGraphNode_StateMachineStateNode* StateMachineNode = Cast<USMGraphNode_StateMachineStateNode>(GetMostRecentState()))
	{
		if (USMBlueprint* Blueprint = StateMachineNode->GetStateMachineReference())
		{
			if (const TSubclassOf<UObject> TargetType = Blueprint->GeneratedClass)
			{
				return TargetType;
			}
		}
	}

	return nullptr;
}


#undef LOCTEXT_NAMESPACE
