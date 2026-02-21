// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "ExposedFunctions/SMExposedFunctions.h"

#include "SMConduit.h"
#include "SMLogging.h"
#include "SMTransition.h"

#include "Algo/Transform.h"

void FSMExposedFunctionHandler::Initialize(const UClass* InClass)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMExposedFunctionHandler::Initialize"), STAT_SMExposedFunctionHandler_Initialize, STATGROUP_LogicDriver);

#if !WITH_EDITOR
	// FProperty gets copied on new instances so we don't have to look up the function. In the editor compiling
	// and re-instancing the class will invalidate it so always look it up to be safe.
	if (Function == nullptr)
#endif
	{
		if (BoundFunction != NAME_None && ExecutionType != ESMExposedFunctionExecutionType::SM_None)
		{
			// Only game thread is safe to call this on -- access shared map of the class.
			check(IsInGameThread() && InClass);
			Function = InClass->FindFunctionByName(BoundFunction);
			check(Function)
		}
		else
		{
			Function = nullptr;
		}
	}
}

void FSMExposedFunctionHandler::Execute(UObject* InObject, void* InParams) const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMExposedFunctionHandler::Execute"), STAT_SMExposedFunctionHandler_Execute, STATGROUP_LogicDriver);

	if (Function == nullptr || !IsValid(InObject) || InObject->IsUnreachable())
	{
		return;
	}

	InObject->ProcessEvent(Function, InParams);
}

namespace LD
{
	namespace ExposedFunctions
	{
		template<typename T>
		static T& GetOrAddInitialArrayElement(TArray<T>& InArray)
		{
			if (InArray.Num() == 0)
			{
				return InArray.AddDefaulted_GetRef();
			}
			ensure(InArray.Num() == 1);
			return InArray[0];
		}
	}
}

TArray<FSMExposedFunctionHandler*> FSMExposedNodeFunctions::GetFlattedArrayOfAllNodeFunctionHandlers()
{
	TArray<FSMExposedFunctionHandler*> AllHandlers;

	auto TransformToPointers = [&](const TArray<FSMExposedFunctionHandler>& InArray)
	{
		Algo::TransformIf(InArray, AllHandlers,
		[](const FSMExposedFunctionHandler& Handler)
		{
			return true;
		},
		[](const FSMExposedFunctionHandler& Handler) -> FSMExposedFunctionHandler*
		{
			return const_cast<FSMExposedFunctionHandler*>(&Handler);
		});
	};

	auto AddRootFunctions = [&](const FSMNode_FunctionHandlers* InNodeFunctionHandlers)
	{
		TransformToPointers(InNodeFunctionHandlers->NodeInitializedGraphEvaluators);
		TransformToPointers(InNodeFunctionHandlers->NodeShutdownGraphEvaluators);
		TransformToPointers(InNodeFunctionHandlers->OnRootStateMachineStartedGraphEvaluator);
		TransformToPointers(InNodeFunctionHandlers->OnRootStateMachineStoppedGraphEvaluator);
	};

	for (::FSMState_FunctionHandlers& ExposedFunctions : FSMState_FunctionHandlers)
	{
		AddRootFunctions(&ExposedFunctions);

		TransformToPointers(ExposedFunctions.BeginStateGraphEvaluator);
		TransformToPointers(ExposedFunctions.UpdateStateGraphEvaluator);
		TransformToPointers(ExposedFunctions.EndStateGraphEvaluator);
	}

	for (::FSMConduit_FunctionHandlers& ExposedFunctions : FSMConduit_FunctionHandlers)
	{
		AddRootFunctions(&ExposedFunctions);

		TransformToPointers(ExposedFunctions.ConduitEnteredGraphEvaluator);
		TransformToPointers(ExposedFunctions.CanEnterConduitGraphEvaluator);
	}

	for (::FSMTransition_FunctionHandlers& ExposedFunctions : FSMTransition_FunctionHandlers)
	{
		AddRootFunctions(&ExposedFunctions);

		TransformToPointers(ExposedFunctions.TransitionEnteredGraphEvaluator);
		TransformToPointers(ExposedFunctions.CanEnterTransitionGraphEvaluator);
		TransformToPointers(ExposedFunctions.TransitionPreEvaluateGraphEvaluator);
		TransformToPointers(ExposedFunctions.TransitionPostEvaluateGraphEvaluator);
	}

	return AllHandlers;
}

FSMNode_FunctionHandlers* FSMExposedNodeFunctions::GetOrAddInitialElement(const UScriptStruct* InStructType)
{
	if (InStructType == FSMConduit::StaticStruct())
	{
		return &LD::ExposedFunctions::GetOrAddInitialArrayElement(FSMConduit_FunctionHandlers);
	}

	if (InStructType == FSMTransition::StaticStruct())
	{
		return &LD::ExposedFunctions::GetOrAddInitialArrayElement(FSMTransition_FunctionHandlers);
	}

	if (InStructType->IsChildOf(FSMState_Base::StaticStruct()))
	{
		return &LD::ExposedFunctions::GetOrAddInitialArrayElement(FSMState_FunctionHandlers);
	}

	return nullptr;
}
