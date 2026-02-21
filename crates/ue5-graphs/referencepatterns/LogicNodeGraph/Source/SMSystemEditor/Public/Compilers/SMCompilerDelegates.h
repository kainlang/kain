// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FKismetCompilerContext;

/**
 * Delegates used by the compile process.
 */
struct SMSYSTEMEDITOR_API FSMCompilerDelegates
{
	/**
	 * Delegate type when a blueprint is compiled. Broadcasts the FKismetCompilerContext which has access to
	 * the MessageLog and Blueprint.
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlueprintCompiledSignature, const FKismetCompilerContext& /* CompilerContext */);

	/** Called during PreCompile() of a state machine blueprint. */
	static FOnBlueprintCompiledSignature OnStateMachinePreCompiled;

	/** Called during PostCompile() of a state machine blueprint. */
	static FOnBlueprintCompiledSignature OnStateMachinePostCompiled;

	/** Called during PreCompile() of a node blueprint. */
	static FOnBlueprintCompiledSignature OnNodePreCompiled;

	/** Called during PostCompile() of a node blueprint. */
	static FOnBlueprintCompiledSignature OnNodePostCompiled;
};
