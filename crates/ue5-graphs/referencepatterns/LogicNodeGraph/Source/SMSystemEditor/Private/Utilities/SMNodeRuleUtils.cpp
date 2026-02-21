// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeRuleUtils.h"

#include "SMStateInstance.h"
#include "SMStateMachineInstance.h"

#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "SMBlueprintEditorUtils.h"
#include "SMSystemEditorLog.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"

FSMStateConnectionValidator& LD::NodeRuleUtils::FExtractedNodeConnectionValidator::AsStateConnectionValidator() const
{
	check(NativeClass && NativeClass->IsChildOf(USMStateInstance_Base::StaticClass()))
	return *StaticCastSharedRef<FSMStateConnectionValidator>(ConnectionValidator.ToSharedRef());
}

FSMTransitionConnectionValidator& LD::NodeRuleUtils::FExtractedNodeConnectionValidator::AsTransitionConnectionValidator() const
{
	check(NativeClass && NativeClass->IsChildOf(USMTransitionInstance::StaticClass()))
	return *StaticCastSharedRef<FSMTransitionConnectionValidator>(ConnectionValidator.ToSharedRef());
}

bool LD::NodeRuleUtils::FExtractedNodeConnectionValidator::IsValid() const
{
	return ConnectionValidator.IsValid() && NativeClass != nullptr;
}

bool LD::NodeRuleUtils::IsNodeClassAllowed(const TSoftClassPtr<USMNodeInstance>& InNodeClass,
                                       const USMGraphNode_Base* InGraphNode, FNodeClassAllowedResult& OutResult)
{
	if (InNodeClass.IsNull())
	{
		return false;
	}

	check(InGraphNode);
	
	const UClass* StateMachineClass = FSMBlueprintEditorUtils::GetStateMachineClassFromGraph(InGraphNode->GetOwningStateMachineGraph());
	const USMStateMachineInstance* StateMachineDefault = StateMachineClass ? Cast<USMStateMachineInstance>(StateMachineClass->GetDefaultObject()) : nullptr;
	
	// Validate connection.
	const FExtractedNodeConnectionValidator NodeConnectionValidator = GetConnectionValidators(InNodeClass);
	if (!ensure(NodeConnectionValidator.IsValid()))
	{
		return false;
	}
	
	if (const USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(InGraphNode))
	{
		// Validate allowed placement in state machine.
		if (StateMachineDefault && !StateMachineDefault->GetAllowedNodes().IsStateAllowed(InNodeClass))
		{
			OutResult.bFailedFromOwningStateMachineRules = true;
			return false;
		}

		const FSMStateConnectionValidator& StateConnectionValidator = NodeConnectionValidator.AsStateConnectionValidator();

		// If the rule has inbound states make sure at least one passes.
		bool bPasses = true;
		if (StateConnectionValidator.AllowedInboundStates.Num() > 0)
		{
			bPasses = false;
			const int32 NumInputTransitions = StateNode->GetNumInputConnections();
			for (int32 Idx = 0; Idx < NumInputTransitions; ++Idx)
			{
				constexpr bool bIncludeReroute = false;
				if (const USMGraphNode_StateNodeBase* PrevState = StateNode->GetPreviousState(Idx, bIncludeReroute, true))
				{
					if (StateConnectionValidator.IsInboundConnectionValid(PrevState->GetNodeClass(), StateMachineClass))
					{
						bPasses = true;
						break;
					}
				}
			}
		}

		// If it passes check outbound states if the rule requires it.
		if (bPasses && StateConnectionValidator.AllowedOutboundStates.Num() > 0)
		{
			bPasses = false;
			const int32 NumOutputTransitions = StateNode->GetNumOutputConnections();
			for (int32 Idx = 0; Idx < NumOutputTransitions; ++Idx)
			{
				if (const USMGraphNode_StateNodeBase* NextState = StateNode->GetNextState(Idx, true))
				{
					if (StateConnectionValidator.IsOutboundConnectionValid(NextState->GetNodeClass(), StateMachineClass))
					{
						bPasses = true;
						break;
					}
				}
			}
		}

		// Check no connection allowed.
		if (bPasses
			&& StateConnectionValidator.AllowedInboundStates.Num() == 0
			&& StateConnectionValidator.AllowedOutboundStates.Num() == 0)
		{
			bPasses = StateConnectionValidator.IsInboundConnectionValid(nullptr, StateMachineClass);
		}

		return bPasses;
	}
	else if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(InGraphNode))
	{
		// Validate allowed placement in state machine.
		if (StateMachineDefault && !StateMachineDefault->GetAllowedNodes().IsTransitionAllowed(InNodeClass))
		{
			OutResult.bFailedFromOwningStateMachineRules = true;
			return false;
		}

		constexpr bool bIncludeReroute = false;
		
		const UClass* FromStateClass =
			Transition->GetFromState(bIncludeReroute) ? Transition->GetFromState(bIncludeReroute)->GetNodeClass() : nullptr;
		const UClass* ToStateClass =
			Transition->GetToState(bIncludeReroute) ? Transition->GetToState(bIncludeReroute)->GetNodeClass() : nullptr;
		
		return DoesTransitionClassPassRules(InNodeClass,
			FromStateClass, ToStateClass, StateMachineClass, true);
	}

	return true;
}

bool LD::NodeRuleUtils::IsNodeClassAllowed(const TSoftClassPtr<USMNodeInstance>& InNodeClass,
	const USMGraphNode_Base* InGraphNode)
{
	FNodeClassAllowedResult Result;
	return IsNodeClassAllowed(InNodeClass, InGraphNode, Result);
}

bool LD::NodeRuleUtils::DoesTransitionClassPassRules(const TSoftClassPtr<USMNodeInstance>& InTransitionClass,
                                                     const TSoftClassPtr<USMStateInstance_Base>& InFromStateClass,
                                                     const TSoftClassPtr<USMStateInstance_Base>& InToStateClass,
                                                     const TSoftClassPtr<USMStateMachineInstance>& InStateMachineClass,
                                                     bool bPassOnNoRules)
{
	const FExtractedNodeConnectionValidator NodeConnectionValidator = GetConnectionValidators(InTransitionClass);
	if (!ensure(NodeConnectionValidator.IsValid()))
	{
		return false;
	}
		
	return NodeConnectionValidator.AsTransitionConnectionValidator().IsConnectionValid(InFromStateClass,
		InToStateClass, InStateMachineClass, bPassOnNoRules);
}

LD::NodeRuleUtils::FExtractedNodeConnectionValidator LD::NodeRuleUtils::GetConnectionValidators(
	const TSoftClassPtr<USMNodeInstance>& InNodeClass)
{
	FExtractedNodeConnectionValidator ExtractedConnectionValidator;

	auto ResolveLoadedClass = [&](UClass* LoadedClass)
	{
		// Find most recent native class.
		UClass* NativeClass = LoadedClass;
		for (; NativeClass; NativeClass = NativeClass->GetSuperClass())
		{
			if (NativeClass->IsNative())
			{
				break;
			}
		}
		ExtractedConnectionValidator.NativeClass = NativeClass;

		if (ensure(LoadedClass && NativeClass))
		{
			USMNodeInstance* CDO = CastChecked<USMNodeInstance>(LoadedClass->GetDefaultObject());
			if (NativeClass->IsChildOf(USMStateInstance_Base::StaticClass()))
			{
				ExtractedConnectionValidator.ConnectionValidator = MakeShared<FSMStateConnectionValidator>(CastChecked<USMStateInstance_Base>(CDO)->GetAllowedConnections());
			
			}
			else if (NativeClass->IsChildOf(USMTransitionInstance::StaticClass()))
			{
				ExtractedConnectionValidator.ConnectionValidator = MakeShared<FSMTransitionConnectionValidator>(CastChecked<USMTransitionInstance>(CDO)->GetAllowedConnections());
			}
		}
	};
	
	if (UClass* LoadedClass = InNodeClass.Get())
	{
		// Handle preloaded assets or native classes.
		ResolveLoadedClass(LoadedClass);
	}
	else
	{
		// Handle unloaded blueprint assets.
		
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		const FString BlueprintPath = InNodeClass.ToSoftObjectPath().ToString().LeftChop(2); // Chop off _C
		const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(BlueprintPath);

		if (ensure(AssetData.IsValid()))
		{
			// Locate the most recent native class this asset is derived from.
			TSubclassOf<USMNodeInstance> NativeNodeClass = nullptr;
			{
				const FString NativeParentClassPath = AssetData.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath);
				const FSoftClassPath ClassPath(NativeParentClassPath);
				NativeNodeClass = ClassPath.ResolveClass();
			}

			check(NativeNodeClass);
			ExtractedConnectionValidator.NativeClass = NativeNodeClass;

			// This name is shared between Transition and State node instances, but are separate properties.
			static FName ConnectionRulesPropertyName = TEXT("ConnectionRules");
		
			if (const FStructProperty* ConnectionRulesProperty =
				CastField<FStructProperty>(
					NativeNodeClass->FindPropertyByName(ConnectionRulesPropertyName)))
			{
				const FString ConnectionRulesDataString =
					AssetData.GetTagValueRef<FString>(ConnectionRulesProperty->GetFName());

				if (!ConnectionRulesDataString.IsEmpty())
				{
					if (NativeNodeClass->IsChildOf(USMStateInstance_Base::StaticClass()))
					{
						FSMStateConnectionValidator StateConnectionValidator;
						ConnectionRulesProperty->ImportText_Direct(*ConnectionRulesDataString, &StateConnectionValidator, nullptr, PPF_None);
						ExtractedConnectionValidator.ConnectionValidator = MakeShared<FSMStateConnectionValidator>(StateConnectionValidator);
					}
					else if (NativeNodeClass->IsChildOf(USMTransitionInstance::StaticClass()))
					{
						FSMTransitionConnectionValidator TransitionConnectionValidator;
						ConnectionRulesProperty->ImportText_Direct(*ConnectionRulesDataString, &TransitionConnectionValidator, nullptr, PPF_None);
						ExtractedConnectionValidator.ConnectionValidator = MakeShared<FSMTransitionConnectionValidator>(TransitionConnectionValidator);
					}
				}
				else
				{
					// Assets likely not saved with asset registry support.
					ResolveLoadedClass(InNodeClass.LoadSynchronous());
					LDEDITOR_LOG_WARNING(TEXT("Unloaded asset '%s' could not resolve property name '%s'. The asset needs to be resaved."),
						*InNodeClass.GetAssetName(), *ConnectionRulesPropertyName.ToString());
				}
			}
		}
	}

	return ExtractedConnectionValidator;
}
