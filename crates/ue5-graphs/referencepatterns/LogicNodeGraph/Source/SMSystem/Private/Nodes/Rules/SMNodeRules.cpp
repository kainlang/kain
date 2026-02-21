// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeRules.h"
#include "SMConduitInstance.h"
#include "SMStateInstance.h"
#include "SMStateMachineInstance.h"
#include "SMTransitionInstance.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

FSMNodeClassRule::FSMNodeClassRule(): bIncludeChildren(true), bNOT(false)
{
}

bool FSMNodeClassRule::IsBaseClass(const UClass* Class)
{
	return !Class ||
		Class == USMNodeInstance::StaticClass() || Class == USMStateInstance_Base::StaticClass() ||
		Class == USMStateInstance::StaticClass() || Class == USMStateMachineInstance::StaticClass() ||
		Class == USMTransitionInstance::StaticClass() || Class == USMConduitInstance::StaticClass();
}

bool FSMNodeClassRule::IsBaseClass(const TSoftClassPtr<USMNodeInstance>& Class)
{
	return Class.IsValid() && Class->IsNative() && IsBaseClass(Class.Get());
}

TSoftClassPtr<USMNodeInstance> FSMStateClassRule::GetClass() const
{
	return StateClass;
}

TSoftClassPtr<USMNodeInstance> FSMTransitionClassRule::GetClass() const
{
	return TransitionClass;
}

TSoftClassPtr<USMNodeInstance> FSMStateMachineClassRule::GetClass() const
{
	return StateMachineClass;
}

FSMNodeConnectionRule::FSMNodeConnectionRule()
{
}

bool FSMNodeConnectionRule::DoesClassMatch(const TSoftClassPtr<USMNodeInstance>& ExpectedClass, const TSoftClassPtr<USMNodeInstance>& ActualClass, const FSMNodeClassRule& Rule)
{
	if (ExpectedClass.IsNull())
	{
		// None implies all.
		return true;
	}

	if (ActualClass.IsNull())
	{
		// Null classes no longer convert to a base class. The proper base class should always be provided.
		return ActualClass == ExpectedClass ? !Rule.bNOT : Rule.bNOT;
	}

	bool bResult = false;
	
	if (Rule.bIncludeChildren)
	{
		TSet<FTopLevelAssetPath> DerivedClassNames;
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.GetDerivedClassNames({ ExpectedClass.ToSoftObjectPath().GetAssetPath() }, {}, DerivedClassNames);
		const FTopLevelAssetPath ActualAssetPath = ActualClass.ToSoftObjectPath().GetAssetPath();
		bResult = DerivedClassNames.Contains(ActualAssetPath);
	}
	else
	{
		bResult = ActualClass == ExpectedClass;
	}
	
	return Rule.bNOT ? !bResult : bResult;
}

bool FSMTransitionConnectionValidator::IsConnectionValid(const TSoftClassPtr<USMNodeInstance>& FromClass, const TSoftClassPtr<USMNodeInstance>& ToClass, const TSoftClassPtr<USMNodeInstance>& StateMachineClass, bool bPassOnNoRules) const
{
	// No rules makes this action always valid.
	if((AllowedConnections.Num() == 0 || (FromClass.IsNull() && ToClass.IsNull())) && bPassOnNoRules)
	{
		return true;
	}
	
	for (const FSMNodeConnectionRule& Rule : AllowedConnections)
	{
		if (!FSMNodeConnectionRule::DoesClassMatch(Rule.InStateMachine.StateMachineClass, StateMachineClass, Rule.InStateMachine)
			|| !FSMNodeConnectionRule::DoesClassMatch(Rule.FromState.StateClass, FromClass, Rule.FromState)
			|| !FSMNodeConnectionRule::DoesClassMatch(Rule.ToState.StateClass, ToClass, Rule.ToState))
		{
			continue;
		}
		
		return true;
	}

	return false;
}

bool FSMStateConnectionValidator::IsInboundConnectionValid(const TSoftClassPtr<USMNodeInstance>& FromClass,
                                                           const TSoftClassPtr<USMNodeInstance>& StateMachineClass) const
{
	if (!FSMNodeConnectionRule::DoRulesPass<FSMStateMachineClassRule>(StateMachineClass, AllowedInStateMachines))
	{
		return false;
	}
	
	if (!FSMNodeConnectionRule::DoRulesPass<FSMStateClassRule>(FromClass, AllowedInboundStates))
	{
		return false;
	}

	return true;
}

bool FSMStateConnectionValidator::IsOutboundConnectionValid(const TSoftClassPtr<USMNodeInstance>& ToClass,
                                                            const TSoftClassPtr<USMNodeInstance>& StateMachineClass) const
{
	if (!FSMNodeConnectionRule::DoRulesPass<FSMStateMachineClassRule>(StateMachineClass, AllowedInStateMachines))
	{
		return false;
	}

	if (!FSMNodeConnectionRule::DoRulesPass<FSMStateClassRule>(ToClass, AllowedOutboundStates))
	{
		return false;
	}

	return true;
}

FSMStateMachineNodePlacementValidator::FSMStateMachineNodePlacementValidator() : Super(),
	bAllowReferences(true),
	bAllowParents(true), bAllowSubStateMachines(true)
{
}

bool FSMStateMachineNodePlacementValidator::IsStateAllowed(const TSoftClassPtr<USMNodeInstance>& StateClass) const
{
	return FSMNodeConnectionRule::DoRulesPass<FSMStateClassRule>(StateClass, AllowedStates);
}

bool FSMStateMachineNodePlacementValidator::IsTransitionAllowed(const TSoftClassPtr<USMNodeInstance>& TransitionClass) const
{
	return FSMNodeConnectionRule::DoRulesPass<FSMTransitionClassRule>(TransitionClass, AllowedTransitions);
}
