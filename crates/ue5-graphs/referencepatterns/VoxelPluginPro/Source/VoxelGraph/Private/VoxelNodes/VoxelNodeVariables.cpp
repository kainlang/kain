// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelNodeVariables.h"
#include "CppTranslation/VoxelCppUtils.h"

#include "VoxelAssets/VoxelHeightmapAsset.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelGenerators/VoxelGeneratorPicker.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

FVoxelColorTextureVariable::FVoxelColorTextureVariable(const UVoxelExposedNode& Node, UTexture2D* Texture)
	: FVoxelExposedVariable(
		Node,
		"TVoxelTexture<FColor>",
		FVoxelCppUtils::SoftObjectPtrString<UTexture2D>(),
		FVoxelCppUtils::ObjectDefaultString(Texture))
{
}

FString FVoxelColorTextureVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return "FVoxelTextureUtilities::CreateFromTexture_Color(" + FVoxelCppUtils::LoadObjectString(ExposedNameAccessor) + ")";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelFloatTextureVariable::FVoxelFloatTextureVariable(const UVoxelExposedNode& Node)
	: FVoxelExposedVariable(
		Node,
		"TVoxelTexture<float>",
		"FVoxelFloatTexture",
		"")
{
}

FString FVoxelFloatTextureVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return ExposedNameAccessor + ".Texture";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelCurveVariable::FVoxelCurveVariable(const UVoxelExposedNode& Node, UCurveFloat* Curve)
	: FVoxelExposedVariable(
		Node,
		"FVoxelRichCurve",
		FVoxelCppUtils::SoftObjectPtrString<UCurveFloat>(),
		FVoxelCppUtils::ObjectDefaultString(Curve))
{
}

FString FVoxelCurveVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return FString::Printf(TEXT("FVoxelRichCurve(%s)"), *FVoxelCppUtils::LoadObjectString(ExposedNameAccessor));
}

///////////////////////////////////////////////////////////////////////////////

FVoxelColorCurveVariable::FVoxelColorCurveVariable(const UVoxelExposedNode& Node, UCurveLinearColor* Curve)
	: FVoxelExposedVariable(
		Node,
		"FVoxelColorRichCurve",
		FVoxelCppUtils::SoftObjectPtrString<UCurveLinearColor>(),
		FVoxelCppUtils::ObjectDefaultString(Curve))
{
}

FString FVoxelColorCurveVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return FString::Printf(TEXT("FVoxelColorRichCurve(%s)"), *FVoxelCppUtils::LoadObjectString(ExposedNameAccessor));
}

///////////////////////////////////////////////////////////////////////////////

FVoxelHeightmapVariable::FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetFloat* Heightmap)
	: FVoxelExposedVariable(
		Node,
		"TVoxelHeightmapAssetSamplerWrapper<float>",
		FVoxelCppUtils::SoftObjectPtrString<UVoxelHeightmapAssetFloat>(),
		FVoxelCppUtils::ObjectDefaultString(Heightmap))
{
}

FVoxelHeightmapVariable::FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetUINT16* Heightmap)
	: FVoxelExposedVariable(
		Node,
		"TVoxelHeightmapAssetSamplerWrapper<uint16>",
		FVoxelCppUtils::SoftObjectPtrString<UVoxelHeightmapAssetUINT16>(),
		FVoxelCppUtils::ObjectDefaultString(Heightmap))
{
}

FString FVoxelHeightmapVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return Type + "(" + ExposedNameAccessor + ".LoadSynchronous())";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelDataAssetVariable::FVoxelDataAssetVariable(const UVoxelExposedNode & Node, UVoxelDataAsset * Asset)
	: FVoxelExposedVariable(
		Node,
		"TVoxelSharedRef<const FVoxelDataAssetData>",
		FVoxelCppUtils::SoftObjectPtrString<UVoxelDataAsset>(),
		FVoxelCppUtils::ObjectDefaultString(Asset))
{
}

FString FVoxelDataAssetVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return FString::Printf(
		TEXT("%s ? %s->GetData() : MakeVoxelShared<FVoxelDataAssetData>(nullptr)"),
		*FVoxelCppUtils::LoadObjectString(ExposedNameAccessor),
		*FVoxelCppUtils::LoadObjectString(ExposedNameAccessor));
}

///////////////////////////////////////////////////////////////////////////////

FVoxelGeneratorVariable::FVoxelGeneratorVariable(const UVoxelExposedNode& Node, const FVoxelGeneratorPicker& Generator)
	: FVoxelExposedVariable(
		Node,
		"TVoxelSharedRef<FVoxelGeneratorInstance>",
		"FVoxelGeneratorPicker",
		FVoxelCppUtils::PickerDefaultString(Generator))
{
}

FString FVoxelGeneratorVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return ExposedNameAccessor + ".GetInstance(true /*bSilent*/)";
}

///////////////////////////////////////////////////////////////////////////////

inline FString GetGeneratorArraysDefaultValue(const TArray<FVoxelGeneratorPicker>& Pickers)
{
	FString Result = "{\n";
	for (auto& Picker : Pickers)
	{
		Result += "\t\t" + FVoxelCppUtils::PickerDefaultString(Picker) + ",\n";
	}
	Result += "\t}";
	return Result;
}

FVoxelGeneratorArrayVariable::FVoxelGeneratorArrayVariable(const UVoxelExposedNode& Node, const TArray<FVoxelGeneratorPicker>& Generators)
	: FVoxelExposedVariable(
		Node,
		"TArray<TVoxelSharedPtr<FVoxelGeneratorInstance>>",
		"TArray<FVoxelGeneratorPicker>",
		GetGeneratorArraysDefaultValue(Generators))
{
}

FString FVoxelGeneratorArrayVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return "FVoxelNodeFunctions::CreateGeneratorArray(" + ExposedNameAccessor + ")";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelMaterialInterfaceVariable::FVoxelMaterialInterfaceVariable(const UVoxelExposedNode& Node, UMaterialInterface* Material)
	: FVoxelExposedVariable(
		Node,
		FVoxelCppUtils::TypeToString<FName>(),
		FVoxelCppUtils::SoftObjectPtrString<UMaterialInterface>(),
		FVoxelCppUtils::ObjectDefaultString(Material))
{
}

FString FVoxelMaterialInterfaceVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return "*" + ExposedNameAccessor + ".GetAssetName()";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelColorVariable::FVoxelColorVariable(const UVoxelExposedNode& Node, FLinearColor Color)
	: FVoxelExposedVariable(
		Node,
		FVoxelCppUtils::TypeToString<FColor>(),
		FVoxelCppUtils::TypeToString<FLinearColor>(),
		FVoxelCppUtils::LexToCpp(Color))
{
}

FString FVoxelColorVariable::GetLocalVariableFromExposedOne(const FString& ExposedNameAccessor) const
{
	return ExposedNameAccessor + ".ToFColor(false)";
}
