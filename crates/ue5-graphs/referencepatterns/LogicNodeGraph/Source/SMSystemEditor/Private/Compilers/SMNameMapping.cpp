// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNameMapping.h"

FString FSMNameMapping::MakeBaseName(const UEdGraphPin* Net)
{
	UEdGraphNode* Owner = Net->GetOwningNode();
	FString Part1 = Owner->GetDescriptiveCompiledName();

	return FString::Printf(TEXT("%s_%s"), *Part1, *Net->PinName.ToString());
}

FString FSMNameMapping::MakeBaseName(const UEdGraphNode* Net)
{
	return FString::Printf(TEXT("%s"), *Net->GetDescriptiveCompiledName());
}

FString FSMNameMapping::MakeBaseName(const UObject* Net)
{
	return FString::Printf(TEXT("%s"), *Net->GetFName().GetPlainNameString());
}