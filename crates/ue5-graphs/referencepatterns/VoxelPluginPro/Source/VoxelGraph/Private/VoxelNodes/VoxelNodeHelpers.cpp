// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelNodeHelpers.h"
#include "CppTranslation/VoxelCppIds.h"

void FVoxelNodeHelpers::ReplaceInputsOutputs(FString& S, const TArray<FString>& Inputs, const TArray<FString>& Outputs)
{
	TArray<TPair<FString, FString>> Args;
	for (int32 I = 0; I < Inputs.Num(); I++)
	{
		Args.Emplace(TEXT("_I") + FString::FromInt(I), Inputs[I]);
		Args.Emplace(TEXT("Input") + FString::FromInt(I), Inputs[I]);
	}
	for (int32 I = 0; I < Outputs.Num(); I++)
	{
		Args.Emplace(TEXT("_O") + FString::FromInt(I), Outputs[I]);
		Args.Emplace(TEXT("Output") + FString::FromInt(I), Outputs[I]);
	}
	Args.Emplace(TEXT("_C0"), FVoxelCppIds::Context);

	// Iterate in REVERSE order
	// so that Index10 is replaced before Index1 (else collision)
	for (int32 Index = Args.Num() - 1; Index >=0; Index--)
	{
		const auto& It = Args[Index];
		while (S.Contains(It.Key, ESearchCase::CaseSensitive))
		{
			S = S.Replace(*It.Key, *It.Value, ESearchCase::CaseSensitive);
		}
	}
}

FString FVoxelNodeHelpers::GetPrefixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op)
{
	check(InputCount > 0);

	FString Line;

	Line += Outputs[0] + " = ";
	for (int32 I = 0; I < InputCount - 1; I++)
	{
		Line += Op + "(" + Inputs[I] + ", ";
	}

	Line += Inputs[InputCount - 1];

	for (int32 I = 0; I < InputCount - 1; I++)
	{
		Line += ")";
	}

	Line += ";";

	return Line;
}

FString FVoxelNodeHelpers::GetInfixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op)
{
	check(InputCount > 0);

	FString Line;

	Line += Outputs[0] + " = ";
	for (int32 I = 0; I < InputCount - 1; I++)
	{
		Line += Inputs[I] + " " + Op + " ";
	}
	Line += Inputs[InputCount - 1];
	Line += ";";

	return Line;
}
