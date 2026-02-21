// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

UENUM(BlueprintType)
enum ESMDelegateOwner
{
	/** This state machine instance. */
	SMDO_This			UMETA(DisplayName = "This"),
	/**
	 * The context object for this state machine.
	 * The class is not known until run-time and needs to be chosen manually.
	 */
	SMDO_Context		UMETA(DisplayName = "Context"),
	/**
	 * The previous state instance. The class is determined by the state.
	 */
	SMDO_PreviousState	UMETA(DisplayName = "Previous State")
};