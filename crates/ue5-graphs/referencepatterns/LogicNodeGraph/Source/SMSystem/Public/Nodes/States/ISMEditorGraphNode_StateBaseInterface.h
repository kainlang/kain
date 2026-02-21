// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "ISMEditorGraphNode_StateBaseInterface.generated.h"

UINTERFACE(MinimalApi, DisplayName = "Editor Graph Node (State Base)", meta = (CannotImplementInterfaceInBlueprint))
class USMEditorGraphNode_StateBaseInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for accessing editor graph state nodes from non-editor modules.
 * This covers States, State Machines, and Conduits.
 */
class ISMEditorGraphNode_StateBaseInterface
{
	GENERATED_BODY()

public:
	/**
	 * Retrieve the Any State tags used by the state node.
	 */
	UFUNCTION(BlueprintCallable, Category = AnyState, meta = (DevelopmentOnly, DefaultToSelf = "NodeInstance"))
	virtual FGameplayTagContainer& GetAnyStateTags() = 0;

	/**
	 * Set the Any State tags used by the state node. These can be modified during construction scripts.
	 */
	UFUNCTION(BlueprintCallable, Category = AnyState, meta = (DevelopmentOnly, DefaultToSelf = "NodeInstance"))
	virtual void SetAnyStateTags(const FGameplayTagContainer& InAnyStateTags) = 0;
};
