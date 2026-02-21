// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMNodeRules.generated.h"

class USMTransitionInstance;
class USMNodeInstance;
class USMStateInstance_Base;
class USMStateMachineInstance;

USTRUCT()
struct SMSYSTEM_API FSMNodeClassRule
{
	GENERATED_USTRUCT_BODY()

	FSMNodeClassRule();
	virtual ~FSMNodeClassRule() = default;
	virtual TSoftClassPtr<USMNodeInstance> GetClass() const
	{
		return nullptr;
	}

	/** Checks if a class is a base node class. Considers null a base class. */
	static bool IsBaseClass(const UClass* Class);
	static bool IsBaseClass(const TSoftClassPtr<USMNodeInstance>& Class);
	
	/** If all children of this class should be considered. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Rule", meta = (NoResetToDefault))
	bool bIncludeChildren;

	/** Invert the rule. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Rule", meta = (NoResetToDefault))
	bool bNOT;
};

USTRUCT()
struct SMSYSTEM_API FSMStateClassRule : public FSMNodeClassRule
{
	GENERATED_USTRUCT_BODY()

	FSMStateClassRule() {}
	FSMStateClassRule(const UClass* InStateClass) { StateClass = InStateClass; }
	
	virtual TSoftClassPtr<USMNodeInstance> GetClass() const override;
	
	/** The state class to look for. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule", meta = (NoResetToDefault, AllowAbstract))
	TSoftClassPtr<USMStateInstance_Base> StateClass;
};

USTRUCT()
struct SMSYSTEM_API FSMTransitionClassRule : public FSMNodeClassRule
{
	GENERATED_USTRUCT_BODY()

	FSMTransitionClassRule() {}
	FSMTransitionClassRule(const UClass* InTransitionClass) { TransitionClass = InTransitionClass; }

	virtual TSoftClassPtr<USMNodeInstance> GetClass() const override;
	
	/** The state class to look for. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule", meta = (NoResetToDefault, AllowAbstract))
	TSoftClassPtr<USMTransitionInstance> TransitionClass;
};

USTRUCT()
struct SMSYSTEM_API FSMStateMachineClassRule : public FSMNodeClassRule
{
	GENERATED_USTRUCT_BODY()

	FSMStateMachineClassRule() {}
	FSMStateMachineClassRule(const UClass* InStateMachineClass) { StateMachineClass = InStateMachineClass; }
	
	virtual TSoftClassPtr<USMNodeInstance> GetClass() const override;
	
	/** The state machine class to look for. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule", meta = (NoResetToDefault, AllowAbstract))
	TSoftClassPtr<USMStateMachineInstance> StateMachineClass;
};

USTRUCT()
struct SMSYSTEM_API FSMNodeConnectionRule
{
	GENERATED_USTRUCT_BODY()

	FSMNodeConnectionRule();

	/** The start of a connection. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule", meta = (InstancedTemplate))
	FSMStateClassRule FromState;

	/** The end of a connection. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule", meta = (InstancedTemplate))
	FSMStateClassRule ToState;

	/** The state machine this connection exists in. */
	UPROPERTY(EditDefaultsOnly, Category = "Rule", meta = (InstancedTemplate))
	FSMStateMachineClassRule InStateMachine;

	static bool DoesClassMatch(const TSoftClassPtr<USMNodeInstance>& ExpectedClass, const TSoftClassPtr<USMNodeInstance>& ActualClass, const FSMNodeClassRule& Rule);

	template<typename T>
	static bool DoRulesPass(const TSoftClassPtr<USMNodeInstance>& Class, const TArray<T>& Rules)
	{
		if (Rules.Num() == 0)
		{
			return true;
		}

		bool bCheckingInverse = false;
		bool bAllInversedPassed = true;
		for (const FSMNodeClassRule& Rule : Rules)
		{
			if (Rule.bNOT)
			{
				bCheckingInverse = true;
			}
			if (DoesClassMatch(Rule.GetClass(), Class, Rule))
			{
				// Only one regular rules needs to pass.
				if (!Rule.bNOT)
				{
					return true;
				}
			}
			else if (Rule.bNOT)
			{
				// There was more than one NOT. They all need to pass.
				bAllInversedPassed = false;
			}
		}

		return bCheckingInverse && bAllInversedPassed;
	}
};

USTRUCT()
struct SMSYSTEM_API FSMConnectionValidator
{
	GENERATED_USTRUCT_BODY()
};

/**
 * Describe under what conditions transitions should be allowed.
 */
USTRUCT()
struct SMSYSTEM_API FSMTransitionConnectionValidator : public FSMConnectionValidator
{
	GENERATED_USTRUCT_BODY()

	/**
	 * If any connection rules are present at least one must be valid for this connection be allowed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	TArray<FSMNodeConnectionRule> AllowedConnections;
	
	/** Checks if this class has rules and if any of them apply. */
	bool IsConnectionValid(const TSoftClassPtr<USMNodeInstance>& FromClass, const TSoftClassPtr<USMNodeInstance>& ToClass, const TSoftClassPtr<USMNodeInstance>& StateMachineClass, bool bPassOnNoRules = true) const;
};

/**
 * Describe under what conditions nodes are allowed to be connected.
 */
USTRUCT()
struct SMSYSTEM_API FSMStateConnectionValidator : public FSMConnectionValidator
{
	GENERATED_USTRUCT_BODY()

	/**
	 * States that can connect to this state.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	TArray<FSMStateClassRule> AllowedInboundStates;

	/**
	 * States that this state can connect to.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	TArray<FSMStateClassRule> AllowedOutboundStates;
	
	/**
	 * State machines that this state can be placed in.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	TArray<FSMStateMachineClassRule> AllowedInStateMachines;

	/** Checks if this class has rules and if any of them apply. */
	bool IsInboundConnectionValid(const TSoftClassPtr<USMNodeInstance>& FromClass, const TSoftClassPtr<USMNodeInstance>& StateMachineClass) const;
	
	/** Checks if this class has rules and if any of them apply. */
	bool IsOutboundConnectionValid(const TSoftClassPtr<USMNodeInstance>& ToClass, const TSoftClassPtr<USMNodeInstance>& StateMachineClass) const;
};

/**
 * Describe under what conditions nodes are allowed to be placed.
 */
USTRUCT()
struct SMSYSTEM_API FSMStateMachineNodePlacementValidator : public FSMConnectionValidator
{
	GENERATED_USTRUCT_BODY()

	FSMStateMachineNodePlacementValidator();
	
	/**
	 * States that can be placed in this state machine.
	 * This restricts which items show up in the graph context menu and node class drop down.
	 * None implies all are allowed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	TArray<FSMStateClassRule> AllowedStates;

	/**
	 * Transitions that can be placed in this state machine.
	 * This restricts which items show up in the node class drop down.
	 * None implies all are allowed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	TArray<FSMTransitionClassRule> AllowedTransitions;
	
	/**
	 * Restricts the placement of state machine references within this state machine.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	bool bAllowReferences;

	/**
	 * Restricts the placement of state machine parents within this state machine.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	bool bAllowParents;

	/**
	 * Allow sub state machines to be added or collapsed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate))
	bool bAllowSubStateMachines;

	/**
	 * The default state machine class to assign when adding or collapsing a state machine.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (InstancedTemplate, EditCondition = "bAllowSubStateMachines"))
	TSoftClassPtr<USMStateMachineInstance> DefaultSubStateMachineClass;

	/** Checks if this state can be placed in this state machine. */
	bool IsStateAllowed(const TSoftClassPtr<USMNodeInstance>& StateClass) const;

	/** Checks if this transition can be placed in this state machine. */
	bool IsTransitionAllowed(const TSoftClassPtr<USMNodeInstance>& TransitionClass) const;
};

