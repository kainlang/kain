// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CppTranslation/VoxelVariables.h"
#include "VoxelNodes/VoxelExposedNodes.h"
#include "CppTranslation/VoxelCppIds.h"

FString FVoxelVariable::SanitizeName(const FString& InName)
{
	static const FString ValidChars = TEXT("_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	static const FString ValidStartChars = TEXT("_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	FString SanitizedName;

	for (int32 CharIdx = 0; CharIdx < InName.Len(); ++CharIdx)
	{
		FString Char = InName.Mid(CharIdx, 1);

		if (!ValidChars.Contains(*Char))
		{
			SanitizedName += TEXT("_");
		}
		else
		{
			SanitizedName += Char;
		}
	}

	if (SanitizedName == "None")
	{
		return "None1";
	}

	if (SanitizedName.IsEmpty() || SanitizedName == TEXT("_"))
	{
		return TEXT("Name");
	}

	if (ValidStartChars.Contains(*SanitizedName.Mid(0, 1)))
	{
		return SanitizedName;
	}
	else
	{
		return TEXT("_") + SanitizedName;
	}
}


FVoxelVariable::FVoxelVariable(const FString& Type, const FString& Name)
	: Type(Type)
	, ExposedName(SanitizeName(Name))
	, CppName(ExposedName)
{

}

FVoxelVariable::FVoxelVariable(const FString& Type, const FString& Name, const FString& CppPrefix)
	: Type(Type)
	, ExposedName(SanitizeName(Name))
	, CppName(CppPrefix + ExposedName)
{
}

FVoxelExposedVariable::FVoxelExposedVariable(
	const UVoxelExposedNode& Node,
	const FString& Type,
	const FString& ExposedType,
	const FString& DefaultValue)
	: FVoxelVariable(Type,  Node.UniqueName.ToString(), FVoxelCppIds::ExposedVariablesStruct + ".")
	, Node(&Node)
	, DefaultValue(DefaultValue)
	, ExposedType(ExposedType.IsEmpty() ? Type : ExposedType)
	, Category(Node.Category)
	, Tooltip(Node.Tooltip)
	, Priority(Node.Priority)
	, CustomMetaData(Node.GetMetaData())
{
}

inline bool operator==(const TMap<FName, FString>& A, const TMap<FName, FString>& B)
{
	TArray<FName> KeysA;
	TArray<FName> KeysB;
	A.GetKeys(KeysA);
	B.GetKeys(KeysB);
	KeysA.Sort([](auto& InA, auto& InB) {return InA.FastLess(InB); });
	KeysB.Sort([](auto& InA, auto& InB) {return InA.FastLess(InB); });

	if (KeysA != KeysB)
	{
		return false;
	}
	for (auto& Key : KeysA)
	{
		if (A[Key] != B[Key])
		{
			return false;
		}
	}
	return true;
}

bool FVoxelExposedVariable::IsSameAs(const FVoxelExposedVariable& Other, bool bCheckNode) const
{
	return
		Type == Other.Type &&
		ExposedName == Other.ExposedName &&
		(!bCheckNode || Node == Other.Node) &&
		Priority == Other.Priority &&
		DefaultValue == Other.DefaultValue &&
		Category == Other.Category &&
		Tooltip == Other.Tooltip &&
		ExposedType == Other.ExposedType &&
		CustomMetaData == Other.CustomMetaData &&
		GetLocalVariableFromExposedOne(ExposedName) == Other.GetLocalVariableFromExposedOne(ExposedName) &&
		GetExposedVariableDefaultMetadata() == Other.GetExposedVariableDefaultMetadata();
}

FString FVoxelExposedVariable::GetMetadataString() const
{
	auto Metadata = GetExposedVariableDefaultMetadata();
	Metadata.Append(CustomMetaData);
	Metadata.KeySort([](auto& A, auto& B)
	{
		return A.FastLess(B);
	});

	FString Result;
	for (auto& It : Metadata)
	{
		if (It.Key == STATIC_FNAME("UIMin") ||
			It.Key == STATIC_FNAME("UIMax"))
		{
			if (It.Value.IsEmpty())
			{
				continue;
			}
		}
		if (!Result.IsEmpty())
		{
			Result += ", ";
		}
		Result += It.Key.ToString();
		if (!It.Value.IsEmpty())
		{
			Result += "=";
			Result += "\"";
			Result += It.Value;
			Result += "\"";
		}
	}
	return Result;
}

