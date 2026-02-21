// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMExposedFunctions.h"

class USMInstance;
class USMNodeInstance;
struct FSMNode_Base;

namespace LD
{
	/**
	 * Utility functions for managing blueprint exposed functions. These utilities are meant for internal use with
	 * the plugin and are exported for other modules to use. Licensees may want to write their own implementations.
	 */
	namespace ExposedFunctions
	{
		/** Locate the all exposed functions on a node. */
		SMSYSTEM_API FSMExposedNodeFunctions* FindExposedNodeFunctions(const FSMNode_Base* InNode);

		/** Iterate through all functions initializing them. */
		SMSYSTEM_API void InitializeGraphFunctions(TArray<FSMExposedFunctionHandler*>& InGraphFunctions, const UClass* InSMClass, const UClass* InNodeClass);
		SMSYSTEM_API void InitializeGraphFunctions(TArray<FSMExposedFunctionHandler>& InGraphFunctions, const UClass* InSMClass, const UClass* InNodeClass);
		SMSYSTEM_API void InitializeGraphFunctions(TArray<FSMExposedFunctionHandler>& InGraphFunctions, const USMInstance* InInstance, const USMNodeInstance* InNodeInstance);

		/** Iterates through all functions executing them. */
		SMSYSTEM_API void ExecuteGraphFunctions(const TArray<FSMExposedFunctionHandler>& InGraphFunctions, USMInstance* InInstance, USMNodeInstance* InNodeInstance, void* InParams = nullptr);
	}
}
