// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNodeRules.h"

#include "Templates/SubclassOf.h"

class USMNodeInstance;
class USMStateInstance_Base;
class USMStateMachineInstance;
class USMGraphNode_Base;

/**
 *  Utilities for reading node rules.
 */
namespace LD::NodeRuleUtils
{
	struct FNodeClassAllowedResult
	{
		/** If the node isn't allowed because the owning state machine rules prevented it from being placed. */
		bool bFailedFromOwningStateMachineRules = false;
	};
	
	struct FExtractedNodeConnectionValidator
	{
		/** The base connection pointer which should be instantiated to the correct child class version. */
		TSharedPtr<FSMConnectionValidator> ConnectionValidator;
		
		/** The most recent native class this belongs to. Use this to determine what to static cast the result to. */
		TSubclassOf<USMNodeInstance> NativeClass;

		/** Convert to StateConnection. */
		FSMStateConnectionValidator& AsStateConnectionValidator() const;

		/** Convert to TransitionConnection. */
		FSMTransitionConnectionValidator& AsTransitionConnectionValidator() const;

		/** Verifies this contains a valid connection validator and can be identified by a native class. */
		bool IsValid() const;
	};
	
	/**
	 * Check if the given node class is allowed to be assigned for the given node.
	 *
	 * Handles states and transitions.
	 *
	 * @param InNodeClass The node class which needs to pass the rules for the given graph node.
	 * @param InGraphNode The graph node this class will be assigned to.
	 * @param OutResult   Additional data explaining the result.
	 *
	 * @return            True if the node class is allowed to be assigned.
	 */
	SMSYSTEMEDITOR_API bool IsNodeClassAllowed(const TSoftClassPtr<USMNodeInstance>& InNodeClass, const USMGraphNode_Base* InGraphNode,
		FNodeClassAllowedResult& OutResult);
	SMSYSTEMEDITOR_API bool IsNodeClassAllowed(const TSoftClassPtr<USMNodeInstance>& InNodeClass, const USMGraphNode_Base* InGraphNode);
	
	/**
	 * Checks if a transition class passes its rules given its surrounding classes.
	 * 
	 * @param InTransitionClass   The transition class which contains the rules.
	 * @param InFromStateClass    The from state leading to this transition.
	 * @param InToStateClass      The to state leading to this transition.
	 * @param InStateMachineClass The state machine containing the transition.
	 * @param bPassOnNoRules      If we should return true if there are no rules.
	 * @return                    True if transition passes its rules.
	 */
	bool DoesTransitionClassPassRules(const TSoftClassPtr<USMNodeInstance>& InTransitionClass,
	                                  const TSoftClassPtr<USMStateInstance_Base>& InFromStateClass,
	                                  const TSoftClassPtr<USMStateInstance_Base>& InToStateClass,
	                                  const TSoftClassPtr<USMStateMachineInstance>& InStateMachineClass,
	                                  bool bPassOnNoRules);
	
	/**
	 * Find the correct connection validator from a node class. This will either extract it from the asset or use the
	 * live CDO version. The result will need to be cast to the correct type, based on the NativeClass.
	 *
	 * StateMachineNodeClass specific versions are not extracted as those are expected to always be loaded.
	 *
	 * @param InNodeClass The node class which contains the validator.
	 *
	 * @return            The connection validator. Check IsValid() on it to verify it was found.
	 */
	FExtractedNodeConnectionValidator GetConnectionValidators(const TSoftClassPtr<USMNodeInstance>& InNodeClass);
}