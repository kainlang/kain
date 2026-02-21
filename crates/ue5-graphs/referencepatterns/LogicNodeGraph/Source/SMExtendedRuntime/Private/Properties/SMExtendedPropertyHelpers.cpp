// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMExtendedPropertyHelpers.h"

#include "SMTextGraphLogging.h"

void USMExtendedGraphPropertyHelpers::BreakTextGraphProperty(const FSMTextGraphProperty& GraphProperty, FText& Result)
{
	const_cast<FSMTextGraphProperty&>(GraphProperty).Execute();
	Result = GraphProperty.Result;
}

FText USMExtendedGraphPropertyHelpers::ObjectToText(UObject* InObject, const FName InFunctionName)
{
	if (InObject)
	{
		if (UFunction* Function = InObject->FindFunction(InFunctionName))
		{
			struct FParams
			{
				FText Result;
			};

			if (!Function->HasAnyFunctionFlags(FUNC_Native) && !Function->HasAnyFunctionFlags(FUNC_HasOutParms))
			{
				// Native functions may not have OutParams flag.
				LD_TEXTGRAPH_LOG_ERROR(TEXT("No out text parameter on text conversion function '%s'."), *InFunctionName.ToString());
				return FText::GetEmpty();
			}
			
			const int32 NumParams = Function->NumParms;
			if (NumParams != 1)
			{
				LD_TEXTGRAPH_LOG_ERROR(TEXT("Incorrect number of parameters on function '%s'. There should be 1 out parameter only but there are '%s' total."), *InFunctionName.ToString(), *FString::FromInt(NumParams));
				return FText::GetEmpty();
			}
			
			FParams Params;
			InObject->ProcessEvent(Function, &Params);

			return Params.Result;
		}
		
		LD_TEXTGRAPH_LOG_ERROR(TEXT("Could not find text serialization function '%s' for object '%s'."), *InFunctionName.ToString(), *InObject->GetName());
	}

	return FText::GetEmpty();
}
