// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "ExposedFunctions/SMExposedFunctionHelpers.h"

#include "SMInstance.h"
#include "SMNodeInstance.h"

namespace LD
{
	namespace ExposedFunctions
	{
		FORCEINLINE UObject* OwningObjectSelector(const FSMExposedFunctionHandler& FunctionHandler,
			USMInstance* Instance, USMNodeInstance* NodeInstance)
		{
			UObject* Object = FunctionHandler.ExecutionType == ESMExposedFunctionExecutionType::SM_NodeInstance ? Cast<UObject>(NodeInstance) : Cast<UObject>(Instance);
			return Object;
		}

		FORCEINLINE const UClass* OwningObjectSelector(const FSMExposedFunctionHandler& FunctionHandler,
			const UClass* InstanceClass, const UClass* NodeInstanceClass)
		{
			return FunctionHandler.ExecutionType == ESMExposedFunctionExecutionType::SM_NodeInstance ? NodeInstanceClass : InstanceClass;
		}

		FORCEINLINE const UClass* OwningObjectSelector(const FSMExposedFunctionHandler* FunctionHandler,
			const UClass* InstanceClass, const UClass* NodeInstanceClass)
		{
			return FunctionHandler->ExecutionType == ESMExposedFunctionExecutionType::SM_NodeInstance ? NodeInstanceClass : InstanceClass;
		}
	}
}

FSMExposedNodeFunctions* LD::ExposedFunctions::FindExposedNodeFunctions(const FSMNode_Base* InNode)
{
	check(InNode);
	if (USMInstance* Instance = InNode->GetOwningInstance())
	{
		return Instance->GetNodeExposedFunctions().Find(InNode->GetNodeGuid());
	}
	return nullptr;
}

void LD::ExposedFunctions::InitializeGraphFunctions(TArray<FSMExposedFunctionHandler*>& InGraphFunctions,
	const UClass* InSMClass, const UClass* InNodeClass)
{
	for (FSMExposedFunctionHandler* FunctionHandler : InGraphFunctions)
	{
		FunctionHandler->Initialize(OwningObjectSelector(FunctionHandler, InSMClass, InNodeClass));
	}
}

void LD::ExposedFunctions::InitializeGraphFunctions(TArray<FSMExposedFunctionHandler>& InGraphFunctions,
	const UClass* InSMClass, const UClass* InNodeClass)
{
	for (FSMExposedFunctionHandler& FunctionHandler : InGraphFunctions)
	{
		FunctionHandler.Initialize(OwningObjectSelector(FunctionHandler, InSMClass, InNodeClass));
	}
}

void LD::ExposedFunctions::InitializeGraphFunctions(TArray<FSMExposedFunctionHandler>& InGraphFunctions,
													const USMInstance* InInstance, const USMNodeInstance* InNodeInstance)
{
	check(InInstance);
	InitializeGraphFunctions(InGraphFunctions, InInstance->GetClass(), InNodeInstance ? InNodeInstance->GetClass() : nullptr);
}

void LD::ExposedFunctions::ExecuteGraphFunctions(const TArray<FSMExposedFunctionHandler>& InGraphFunctions,
	USMInstance* InInstance, USMNodeInstance* InNodeInstance, void* InParams)
{
	for (const FSMExposedFunctionHandler& FunctionHandler : InGraphFunctions)
	{
		FunctionHandler.Execute(OwningObjectSelector(FunctionHandler, InInstance, InNodeInstance), InParams);
	}
}


