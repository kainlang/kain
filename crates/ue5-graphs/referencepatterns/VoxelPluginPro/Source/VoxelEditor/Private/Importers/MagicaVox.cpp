// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Importers/MagicaVox.h"
#include "VoxelAssets/VoxelDataAssetData.inl"

#define OGT_VOX_IMPLEMENTATION
#include "ogt_vox.h"

#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"

bool MagicaVox::ImportToAsset(const FString& Filename, FVoxelDataAssetData& Asset, bool bUsePalette)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Error when opening the file")));
		return false;
	}

	const ogt_vox_scene* Scene = ogt_vox_read_scene(Bytes.GetData(), Bytes.Num());
	if (!Scene)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Error when decoding the scene")));
		return false;
	}

	ON_SCOPE_EXIT
	{
		ogt_vox_destroy_scene(Scene);
	};

	const auto Models = TArrayView<const ogt_vox_model*>(Scene->models, Scene->num_models);
	if (Models.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("No models in the file")));
		return false;
	}
	if (Models.Num() > 1)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("More than one model in the file. Only the first one will be imported.")));
	}
	if (!ensure(Models[0]))
	{
		return false;
	}
	auto& Model = *Models[0];
	check(Model.voxel_data);
	
	Asset.SetSize(FIntVector(Model.size_y, Model.size_x, Model.size_z), true);

	for (uint32 Z = 0; Z < Model.size_z; Z++)
	{
		for (uint32 Y = 0; Y < Model.size_y; Y++)
		{
			for (uint32 X = 0; X < Model.size_x; X++)
			{
				const uint32 Index = X + Model.size_x * Y + Model.size_x * Model.size_y * Z;
				const uint8 Voxel = Model.voxel_data[Index];

				Asset.SetValue(Y, X, Z, Voxel > 0 ? FVoxelValue::Full() : FVoxelValue::Empty());

				FVoxelMaterial Material(ForceInit);
				if (bUsePalette)
				{
					const auto MagicaColor = Scene->palette.color[Voxel];
					// Store the color as a linear color, so edits can be in linear space
					const FColor Color = FLinearColor(FColor(MagicaColor.r, MagicaColor.g, MagicaColor.b, MagicaColor.a)).ToFColor(false);
					Material.SetColor(Color);
				}
				else
				{
					Material.SetSingleIndex(Voxel);
				}

				Asset.SetMaterial(Y, X, Z, Material);
			}
		}
	}
	
	return true;
}

