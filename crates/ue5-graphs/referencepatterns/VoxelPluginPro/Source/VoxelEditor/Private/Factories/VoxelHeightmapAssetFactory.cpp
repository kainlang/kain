// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Factories/VoxelHeightmapAssetFactory.h"
#include "VoxelAssets/VoxelHeightmapAssetData.inl"
#include "VoxelUtilities/VoxelMathUtilities.h"
#include "VoxelMessages.h"
#include "VoxelFeedbackContext.h"
#include "VoxelEditorDetailsUtilities.h"

#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"

#include "Editor.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"

#include "Modules/ModuleManager.h"

#include "LandscapeEditorModule.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"

namespace HeightmapHelpers
{
	template<typename TLandscapeMapFileFormat>
	const TLandscapeMapFileFormat* GetFormat(const TCHAR* Extension, ILandscapeEditorModule& LandscapeEditorModule);

	template<>
	inline const ILandscapeHeightmapFileFormat* GetFormat(const TCHAR* Extension, ILandscapeEditorModule& LandscapeEditorModule)
	{
		return LandscapeEditorModule.GetHeightmapFormatByExtension(Extension);
	}
	template<>
	inline const ILandscapeWeightmapFileFormat* GetFormat(const TCHAR* Extension, ILandscapeEditorModule& LandscapeEditorModule)
	{
		return LandscapeEditorModule.GetWeightmapFormatByExtension(Extension);
	}

	inline FLandscapeHeightmapInfo Validate(const TCHAR* Filename, const ILandscapeHeightmapFileFormat* Format)
	{
		return Format->Validate(Filename);
	}
	inline FLandscapeWeightmapInfo Validate(const TCHAR* Filename, const ILandscapeWeightmapFileFormat* Format)
	{
		return Format->Validate(Filename, "");
	}

	inline FLandscapeHeightmapImportData Import(const TCHAR* Filename, FLandscapeFileResolution ExpectedResolution, const ILandscapeHeightmapFileFormat* Format)
	{
		return Format->Import(Filename, ExpectedResolution);
	}
	inline FLandscapeWeightmapImportData Import(const TCHAR* Filename, FLandscapeFileResolution ExpectedResolution, const ILandscapeWeightmapFileFormat* Format)
	{
		return Format->Import(Filename, "", ExpectedResolution);
	}

	template<typename TLandscapeMapFileFormat, typename TLandscapeMapImportData>
	bool GetMap(const FString& Filename, int32& OutWidth, int32& OutHeight, TLandscapeMapImportData& OutMapImportData)
	{
		if (Filename.IsEmpty())
		{
			FVoxelEditorUtilities::ShowError(VOXEL_LOCTEXT("Error: Empty filename!"));
			return false;
		}

		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		FString Extension = FPaths::GetExtension(Filename, true);
		const TLandscapeMapFileFormat* Format = HeightmapHelpers::GetFormat<TLandscapeMapFileFormat>(*Extension, LandscapeEditorModule);

		if (!Format)
		{
			FVoxelEditorUtilities::ShowError(FText::FromString("Error: Unknown extension " + Extension));
			return false;
		}

		auto Info = HeightmapHelpers::Validate(*Filename, Format);
		switch (Info.ResultCode)
		{
		case ELandscapeImportResult::Success:
			break;
		case ELandscapeImportResult::Warning:
		{
			if (!FVoxelEditorUtilities::ShowWarning(Info.ErrorMessage))
			{
				return false;
			}
			break;
		}
		case ELandscapeImportResult::Error:
		{
			FVoxelEditorUtilities::ShowError(Info.ErrorMessage);
			return false;
		}
		default:
			check(false);
		}

		const int32 Index = Info.PossibleResolutions.Num() / 2;
		OutWidth = Info.PossibleResolutions[Index].Width;
		OutHeight = Info.PossibleResolutions[Index].Height;
		OutMapImportData = HeightmapHelpers::Import(*Filename, Info.PossibleResolutions[0], Format);

		switch (OutMapImportData.ResultCode)
		{
		case ELandscapeImportResult::Success:
			return true;
		case ELandscapeImportResult::Warning:
		{
			return FVoxelEditorUtilities::ShowWarning(OutMapImportData.ErrorMessage);
		}
		case ELandscapeImportResult::Error:
		{
			FVoxelEditorUtilities::ShowError(OutMapImportData.ErrorMessage);
			return false;
		}
		default:
			check(false);
			return false;
		}
	}

	bool GetHeightmap(const FString& Filename, int32& OutWidth, int32& OutHeight, FLandscapeHeightmapImportData& OutHeightmapImportData)
	{
		return HeightmapHelpers::GetMap<ILandscapeHeightmapFileFormat>(Filename, OutWidth, OutHeight, OutHeightmapImportData);
	}

	bool GetWeightmap(const FString& Filename, int32& OutWidth, int32& OutHeight, FLandscapeWeightmapImportData& OutWeightmapImportData)
	{
		return HeightmapHelpers::GetMap<ILandscapeWeightmapFileFormat>(Filename, OutWidth, OutHeight, OutWeightmapImportData);
	}
}

template<int32 NumChannels>
void ImportMaterialAsXWayBlend(
	TVoxelStaticArray<uint8, NumChannels>& OutMaterialIndices,
	TVoxelStaticArray<float, NumChannels>& OutMaterialStrengths,
	const TArray<FVoxelHeightmapImportersHelpers::FWeightmap>& Weightmaps, const uint32 WeightmapDataIndex)
{
	OutMaterialIndices.Memzero();
	OutMaterialStrengths.Memzero();

	if (Weightmaps.Num() <= NumChannels)
	{
		for (int32 Index = 0; Index < Weightmaps.Num(); Index++)
		{
			OutMaterialIndices[Index] = Weightmaps[Index].Index;
			OutMaterialStrengths[Index] = FVoxelUtilities::UINT8ToFloat(Weightmaps[Index].Data[WeightmapDataIndex]);
		}
	}
	else
	{
		TArray<int32, TInlineAllocator<16>> WeightmapIndices;
		for (int32 Index = 0; Index < Weightmaps.Num(); Index++)
		{
			WeightmapIndices.Add(Index);
		}

		const TVoxelStaticArray<TTuple<int32, int32>, NumChannels> TopElements = FVoxelUtilities::FindTopXElements<NumChannels>(
			WeightmapIndices, [&](int32 A, int32 B)
			{
				return Weightmaps[A].Data[WeightmapDataIndex] < Weightmaps[B].Data[WeightmapDataIndex];
			});

		for (int32 Index = 0; Index < NumChannels; Index++)
		{
			auto& Weightmap = Weightmaps[TopElements[Index].template Get<0>()];
			OutMaterialIndices[Index] = Weightmap.Index;
			OutMaterialStrengths[Index] = FVoxelUtilities::UINT8ToFloat(Weightmap.Data[WeightmapDataIndex]);
		}
	}
}

template<typename T>
void ImportMaterialFromWeightmaps(
	EVoxelHeightmapImporterMaterialConfig MaterialConfig,
	const TArray<FVoxelHeightmapImportersHelpers::FWeightmap>& Weightmaps, const uint32 WeightmapDataIndex,
	TVoxelHeightmapAssetData<T>& HeightmapAssetData, const int32 X, const int32 Y)
{
	if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::RGB)
	{
		FColor Color(ForceInit);
		for (auto& Weightmap : Weightmaps)
		{
			const uint8 Value = Weightmap.Data[WeightmapDataIndex];
			switch (Weightmap.Layer)
			{
			case EVoxelRGBA::R:
				Color.R = Value;
				break;
			case EVoxelRGBA::G:
				Color.G = Value;
				break;
			case EVoxelRGBA::B:
				Color.B = Value;
				break;
			case EVoxelRGBA::A:
				Color.A = Value;
				break;
			}
		}
		HeightmapAssetData.SetMaterial_RGB(X, Y, Color);
	}
	else if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::SingleIndex)
	{
		uint8 MaxValue = Weightmaps[0].Data[WeightmapDataIndex];
		uint8 MaxIndex = Weightmaps[0].Index;
		for (int32 WeightmapIndex = 1; WeightmapIndex < Weightmaps.Num(); WeightmapIndex++)
		{
			const auto& Weightmap = Weightmaps[WeightmapIndex];
			const uint8 Value = Weightmap.Data[WeightmapDataIndex];
			if (Value > MaxValue)
			{
				MaxValue = Value;
				MaxIndex = Weightmap.Index;
			}
		}
		HeightmapAssetData.SetMaterial_SingleIndex(X, Y, MaxIndex);
	}
	else if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::FourWayBlend)
	{
		TVoxelStaticArray<uint8, 4> MaterialIndices;
		TVoxelStaticArray<float, 4> MaterialStrengths;
		ImportMaterialAsXWayBlend<4>(MaterialIndices, MaterialStrengths, Weightmaps, WeightmapDataIndex);

		TVoxelStaticArray<float, 4> FourWayBlendStrengths{ ForceInit };
		for (int32 Index = 0; Index < 4; Index++)
		{
			if (ensure(MaterialIndices[Index] < 4))
			{
				FourWayBlendStrengths[MaterialIndices[Index]] += MaterialStrengths[Index];
			}
		}
		const auto Alphas = FVoxelUtilities::XWayBlend_StrengthsToAlphas_Static<4>(FourWayBlendStrengths);
		
		FColor Color{ ForceInit };
		Color.R = FVoxelUtilities::FloatToUINT8(Alphas[0]);
		Color.G = FVoxelUtilities::FloatToUINT8(Alphas[1]);
		Color.B = FVoxelUtilities::FloatToUINT8(Alphas[2]);
		
		HeightmapAssetData.SetMaterial_RGB(X, Y, Color);
	}
	else if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::FiveWayBlend)
	{
		TVoxelStaticArray<uint8, 5> MaterialIndices;
		TVoxelStaticArray<float, 5> MaterialStrengths;
		ImportMaterialAsXWayBlend<5>(MaterialIndices, MaterialStrengths, Weightmaps, WeightmapDataIndex);

		TVoxelStaticArray<float, 5> FiveWayBlendStrengths{ ForceInit };
		for (int32 Index = 0; Index < 5; Index++)
		{
			if (ensure(MaterialIndices[Index] < 5))
			{
				FiveWayBlendStrengths[MaterialIndices[Index]] += MaterialStrengths[Index];
			}
		}
		const auto Alphas = FVoxelUtilities::XWayBlend_StrengthsToAlphas_Static<5>(FiveWayBlendStrengths);
		
		FColor Color;
		Color.R = FVoxelUtilities::FloatToUINT8(Alphas[0]);
		Color.G = FVoxelUtilities::FloatToUINT8(Alphas[1]);
		Color.B = FVoxelUtilities::FloatToUINT8(Alphas[2]);
		Color.A = FVoxelUtilities::FloatToUINT8(Alphas[3]);
		
		HeightmapAssetData.SetMaterial_RGB(X, Y, Color);
	}
	else if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::MultiIndex)
	{
		TVoxelStaticArray<uint8, 4> MaterialIndices;
		TVoxelStaticArray<float, 4> MaterialStrengths;
		ImportMaterialAsXWayBlend<4>(MaterialIndices, MaterialStrengths, Weightmaps, WeightmapDataIndex);
		
		const auto Alphas = FVoxelUtilities::XWayBlend_StrengthsToAlphas_Static<4>(MaterialStrengths);
		
		FVoxelMaterial Material;
		
		Material.SetMultiIndex_Index0(MaterialIndices[0]);
		Material.SetMultiIndex_Index1(MaterialIndices[1]);
		Material.SetMultiIndex_Index2(MaterialIndices[2]);
		Material.SetMultiIndex_Index3(MaterialIndices[3]);

		Material.SetMultiIndex_Blend0_AsFloat(Alphas[0]);
		Material.SetMultiIndex_Blend1_AsFloat(Alphas[1]);
		Material.SetMultiIndex_Blend2_AsFloat(Alphas[2]);
		
		HeightmapAssetData.SetMaterial_MultiIndex(X, Y, Material);
	}
	else
	{
		check(false);
	}
}

inline EVoxelMaterialConfig GetMaterialConfigFromHeightmapConfig(EVoxelHeightmapImporterMaterialConfig MaterialConfig)
{
	switch (MaterialConfig)
	{
	default: ensure(false);
	case EVoxelHeightmapImporterMaterialConfig::RGB: return EVoxelMaterialConfig::RGB;
	case EVoxelHeightmapImporterMaterialConfig::FiveWayBlend: return EVoxelMaterialConfig::RGB;
	case EVoxelHeightmapImporterMaterialConfig::FourWayBlend: return EVoxelMaterialConfig::RGB;
	case EVoxelHeightmapImporterMaterialConfig::SingleIndex: return EVoxelMaterialConfig::SingleIndex;
	case EVoxelHeightmapImporterMaterialConfig::MultiIndex: return EVoxelMaterialConfig::MultiIndex;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelHeightmapAssetFloatFactory::UVoxelHeightmapAssetFloatFactory()
{
	bCreateNew = false;
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = UVoxelHeightmapAssetFloat::StaticClass();
}

UObject* UVoxelHeightmapAssetFloatFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	for (auto& Layer : LayerInfos)
	{
		if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::FourWayBlend && Layer.Index > 3)
		{
			FVoxelMessages::Error("In Four Way blend, all indices must be between 0 and 3!");
			return nullptr;
		}
		if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::FiveWayBlend && Layer.Index > 4)
		{
			FVoxelMessages::Error("In Five Way blend, all indices must be between 0 and 4!");
			return nullptr;
		}
	}
	
	auto* Asset = NewObject<UVoxelHeightmapAssetFloat>(InParent, Class, Name, Flags | RF_Transactional);

	int32 Width = 0;
	int32 Height = 0;
	for (auto& Component : Components)
	{
		Width = FMath::Max(Width, Component->SectionBaseX + Component->ComponentSizeQuads);
		Height = FMath::Max(Height, Component->SectionBaseY + Component->ComponentSizeQuads);
	}

	// Account for additional vertex row/column at the end
	Width++;
	Height++;

	auto& Data = Asset->GetData();
	Data.SetSize(Width, Height, LayerInfos.Num() > 0, GetMaterialConfigFromHeightmapConfig(MaterialConfig));

	for (auto& Component : Components)
	{
		FLandscapeComponentDataInterface DataInterface(Component);

		// + 1 : account for additional vertex row/column at the end of each components
		// This will result in some data being written twice, but that's fine
		const int32 ComponentSize = Component->ComponentSizeQuads + 1;
		
		if (Data.HasMaterials())
		{
			TArray<FVoxelHeightmapImportersHelpers::FWeightmap> Weightmaps;
			Weightmaps.SetNum(LayerInfos.Num());

			for (int32 Index = 0; Index < Weightmaps.Num(); Index++)
			{
				auto& Weightmap = Weightmaps[Index];
				auto& WeightmapInfo = LayerInfos[Index];
				DataInterface.GetWeightmapTextureData(WeightmapInfo.LayerInfo, Weightmap.Data);
				Weightmap.Layer = WeightmapInfo.Layer;
				Weightmap.Index = WeightmapInfo.Index;
			}
			Weightmaps.RemoveAll([&](auto& Weightmap) { return Weightmap.Data.Num() == 0; });

			const int32 WeightmapSize = (Component->SubsectionSizeQuads + 1) * Component->NumSubsections;

			for (int32 X = 0; X < ComponentSize; X++)
			{
				for (int32 Y = 0; Y < ComponentSize; Y++)
				{
					ImportMaterialFromWeightmaps(
						MaterialConfig, 
						Weightmaps, 
						X + WeightmapSize * Y, 
						Data, 
						Component->SectionBaseX + X, 
						Component->SectionBaseY + Y);
				}
			}
		}

		for (int32 X = 0; X < ComponentSize; X++)
		{
			for (int32 Y = 0; Y < ComponentSize; Y++)
			{
				const FVector Vertex = DataInterface.GetWorldVertex(X, Y);
				const FVector LocalVertex = (Vertex - ActorLocation) / Component->GetComponentTransform().GetScale3D();
				if (ensure(Data.IsValidIndex(LocalVertex.X, LocalVertex.Y)))
				{
					Data.SetHeight(LocalVertex.X, LocalVertex.Y, Vertex.Z);
				}
			}
		}
	}

	Asset->Save();
	
	return Asset;
}

FString UVoxelHeightmapAssetFloatFactory::GetDefaultNewAssetName() const
{
	return AssetName;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelHeightmapAssetUINT16Factory::UVoxelHeightmapAssetUINT16Factory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = UVoxelHeightmapAssetUINT16::StaticClass();
}

bool UVoxelHeightmapAssetUINT16Factory::ConfigureProperties()
{
	// Load from default
	Heightmap = GetDefault<UVoxelHeightmapAssetUINT16Factory>()->Heightmap;
	MaterialConfig = GetDefault<UVoxelHeightmapAssetUINT16Factory>()->MaterialConfig;
	WeightmapsInfos = GetDefault<UVoxelHeightmapAssetUINT16Factory>()->WeightmapsInfos;

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(VOXEL_LOCTEXT("Import Heightmap"))
		.SizingRule(ESizingRule::Autosized);

	bool bSuccess = false;

	auto OnOkClicked = FOnClicked::CreateLambda([&]() 
	{
		if (TryLoad())
		{
			bSuccess = true;
			PickerWindow->RequestDestroyWindow();
		}
		return FReply::Handled();
	});
	auto OnCancelClicked = FOnClicked::CreateLambda([&]() 
	{
		bSuccess = false;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	});

	class FVoxelHeightmapFactoryDetails : public IDetailCustomization
	{
	public:
		FVoxelHeightmapFactoryDetails() = default;

	private:
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
		{
			FSimpleDelegate RefreshDelegate = FSimpleDelegate::CreateLambda([&DetailLayout]()
			{
				DetailLayout.ForceRefreshDetails();
			});
			DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(UVoxelHeightmapAssetUINT16Factory, MaterialConfig))->SetOnPropertyValueChanged(RefreshDelegate);
		}
	};

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	auto DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	FOnGetDetailCustomizationInstance LayoutDelegateDetails = FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FVoxelHeightmapFactoryDetails>(); });
	DetailsPanel->RegisterInstancedCustomPropertyLayout(UVoxelHeightmapAssetUINT16Factory::StaticClass(), LayoutDelegateDetails);
	DetailsPanel->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([&](const FPropertyAndParent& Property)
	{
		FName Name = Property.Property.GetFName();
		if (Name == GET_MEMBER_NAME_STATIC(FVoxelHeightmapImporterWeightmapInfos, Layer))
		{
			return MaterialConfig == EVoxelHeightmapImporterMaterialConfig::RGB;
		}
		else if (Name == GET_MEMBER_NAME_STATIC(FVoxelHeightmapImporterWeightmapInfos, Index))
		{
			return MaterialConfig != EVoxelHeightmapImporterMaterialConfig::RGB;
		}
		else
		{
			return true;
		}
	}));
	DetailsPanel->SetObject(this);

	auto Widget =
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
			.WidthOverride(520.0f)
		[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(500)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						DetailsPanel
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(8)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.Text(VOXEL_LOCTEXT("Create"))
						.HAlign(HAlign_Center)
						.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([&]()
						{
							if (Heightmap.FilePath.IsEmpty())
							{
								return EVisibility::Hidden;
							}
							for (auto& Weightmap : WeightmapsInfos)
							{
								if (Weightmap.File.FilePath.IsEmpty())
								{
									return EVisibility::Hidden;
								}
							}
							return EVisibility::Visible;
						})))
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(OnOkClicked)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
						.TextStyle(FAppStyle::Get(), "FlatButton.DefaultTextStyle")
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SNew(SButton)
						.Text(VOXEL_LOCTEXT("Cancel"))
						.HAlign(HAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(OnCancelClicked)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
						.TextStyle(FAppStyle::Get(), "FlatButton.DefaultTextStyle")
					]
				]
			]
		];

	PickerWindow->SetContent(Widget);

	GEditor->EditorAddModalWindow(PickerWindow);

	// Save to default
	GetMutableDefault<UVoxelHeightmapAssetUINT16Factory>()->Heightmap = Heightmap;
	GetMutableDefault<UVoxelHeightmapAssetUINT16Factory>()->MaterialConfig = MaterialConfig;
	GetMutableDefault<UVoxelHeightmapAssetUINT16Factory>()->WeightmapsInfos = WeightmapsInfos;

	return bSuccess;
}

UObject* UVoxelHeightmapAssetUINT16Factory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto* Asset = NewObject<UVoxelHeightmapAssetUINT16>(InParent, Class, Name, Flags | RF_Transactional);
	if (DoImport(Asset))
	{
		return Asset;
	}
	else
	{
		return nullptr;
	}
}

FString UVoxelHeightmapAssetUINT16Factory::GetDefaultNewAssetName() const
{
	return FPaths::GetBaseFilename(Heightmap.FilePath);
}

bool UVoxelHeightmapAssetUINT16Factory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (auto* Asset = Cast<UVoxelHeightmapAssetUINT16>(Obj))
	{
		OutFilenames.Add(Asset->Heightmap);
		for (auto& Weightmap : Asset->WeightmapsInfos)
		{
			OutFilenames.Add(Weightmap.File.FilePath);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void UVoxelHeightmapAssetUINT16Factory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (auto* Asset = Cast<UVoxelHeightmapAssetUINT16>(Obj))
	{
		for (int32 Index = 0; Index < NewReimportPaths.Num(); Index++)
		{
			if (Index == 0)
			{
				Asset->Heightmap = NewReimportPaths[0];
			}
			else if (ensure(Index - 1 < Asset->WeightmapsInfos.Num()))
			{
				Asset->WeightmapsInfos[Index - 1].File.FilePath = NewReimportPaths[Index];
			}
		}
	}
}

EReimportResult::Type UVoxelHeightmapAssetUINT16Factory::Reimport(UObject* Obj)
{
	if (auto* Asset = Cast<UVoxelHeightmapAssetUINT16>(Obj))
	{
		Heightmap.FilePath = Asset->Heightmap;
		MaterialConfig = Asset->MaterialConfig;
		WeightmapsInfos = Asset->WeightmapsInfos;
		if (!TryLoad())
		{
			return EReimportResult::Failed;
		}
		return DoImport(Asset) ? EReimportResult::Succeeded : EReimportResult::Cancelled;
	}
	else
	{
		return EReimportResult::Failed;
	}
}

int32 UVoxelHeightmapAssetUINT16Factory::GetPriority() const
{
	return ImportPriority;
}

bool UVoxelHeightmapAssetUINT16Factory::TryLoad()
{
	FVoxelScopedSlowTask Progress(1 + WeightmapsInfos.Num(), VOXEL_LOCTEXT("Creating heightmap asset..."));
	Progress.MakeDialog();

	Progress.EnterProgressFrame(1, VOXEL_LOCTEXT("Processing heightmap"));
	if (!HeightmapHelpers::GetHeightmap(Heightmap.FilePath, Width, Height, HeightmapImportData))
	{
		return false;
	}

	Weightmaps.SetNum(WeightmapsInfos.Num());
	for (int32 Index = 0; Index < Weightmaps.Num(); Index++)
	{
		Progress.EnterProgressFrame(1, VOXEL_LOCTEXT("Processing Weightmaps"));

		auto& Weightmap = Weightmaps[Index];
		auto& WeightmapInfo = WeightmapsInfos[Index];

		int32 WeightmapWidth;
		int32 WeightmapHeight;
		FLandscapeWeightmapImportData Result;
		if (!HeightmapHelpers::GetWeightmap(WeightmapInfo.File.FilePath, WeightmapWidth, WeightmapHeight, Result))
		{
			return false;
		}
		if (WeightmapWidth != Width || WeightmapHeight != Height)
		{
			FVoxelEditorUtilities::ShowError(FText::Format(VOXEL_LOCTEXT("Weightmap resolution is not the same as Heightmap ({0})"), FText::FromString(WeightmapInfo.File.FilePath)));
			return false;
		}
		Weightmap.Data = MoveTemp(Result.Data);
		Weightmap.Layer = WeightmapInfo.Layer;
		Weightmap.Index = WeightmapInfo.Index;
	}

	return true;
}

bool UVoxelHeightmapAssetUINT16Factory::DoImport(UVoxelHeightmapAssetUINT16* Asset)
{
	for (auto& Layer : WeightmapsInfos)
	{
		if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::FourWayBlend && Layer.Index > 3)
		{
			FVoxelMessages::Error("In Four Way blend, all indices must be between 0 and 3!");
			return false;
		}
		if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::FiveWayBlend && Layer.Index > 4)
		{
			FVoxelMessages::Error("In Five Way blend, all indices must be between 0 and 4!");
			return false;
		}
	}
	
#define RETURN_IF_CANCEL() { if (Progress.ShouldCancel()) { FVoxelEditorUtilities::ShowError(VOXEL_LOCTEXT("Canceled!")); return false; } }

	FVoxelScopedSlowTask Progress(3.f, VOXEL_LOCTEXT("Creating heightmap asset..."));
	Progress.MakeDialog(true, true);

	RETURN_IF_CANCEL();

	auto& Data = Asset->GetData();
	Data.SetSize(Width, Height, Weightmaps.Num() > 0, GetMaterialConfigFromHeightmapConfig(MaterialConfig));

	Progress.EnterProgressFrame(1.f, VOXEL_LOCTEXT("Copying heightmap"));
	{
		const int64 Total = int64(Width) * int64(Height);
		FVoxelScopedSlowTask HeightmapProgress(Total);
		for (int32 X = 0; X < Width; X++)
		{
			for (int32 Y = 0; Y < Height; Y++)
			{
				const int64 Index = Data.GetIndex(X, Y);
				
				if ((Index & 0x0000FFFF) == 0)
				{
					HeightmapProgress.EnterProgressFrame(FMath::Min<int64>(0x0000FFFF, Total - Index));
					RETURN_IF_CANCEL();
				}
				
				Data.SetHeight(X, Y, HeightmapImportData.Data[Index]);
			}
		}
	}

	RETURN_IF_CANCEL();

	Progress.EnterProgressFrame(1.f, VOXEL_LOCTEXT("Copying weightmaps"));
	if (Data.HasMaterials())
	{
		FVoxelScopedSlowTask WeightmapProgress(int64(Width) * int64(Height));
		for (int32 X = 0; X < Width; X++)
		{
			for (int32 Y = 0; Y < Height; Y++)
			{
				const int64 Index = Data.GetIndex(X, Y);

				if ((Index & 0x0000FFFF) == 0)
				{
					WeightmapProgress.EnterProgressFrame(0x0000FFFF);
					RETURN_IF_CANCEL();
				}

				ImportMaterialFromWeightmaps(
					MaterialConfig,
					Weightmaps,
					Index,
					Data,
					X, Y);
			}
		}
	}

	RETURN_IF_CANCEL();
	
	// Copy config
	Asset->Heightmap = Heightmap.FilePath;
	Asset->MaterialConfig = MaterialConfig;
	Asset->WeightmapsInfos = WeightmapsInfos;
	Asset->Weightmaps.Reset();
	for (auto& Weightmap : WeightmapsInfos)
	{
		FString Path;
		if (MaterialConfig == EVoxelHeightmapImporterMaterialConfig::RGB)
		{
			switch (Weightmap.Layer)
			{
			case EVoxelRGBA::R:
				Path += "Channel = R";
				break;
			case EVoxelRGBA::G:
				Path += "Channel = G";
				break;
			case EVoxelRGBA::B:
				Path += "Channel = B";
				break;
			case EVoxelRGBA::A:
				Path += "Channel = A";
				break;
			default:
				check(false);
				break;
			}
		}
		else
		{
			Path += "Index = " + FString::FromInt(Weightmap.Index);
		}
		Path += "; Path = " + Weightmap.File.FilePath;
		Asset->Weightmaps.Add(Path);
	}

	Progress.EnterProgressFrame(1.f, VOXEL_LOCTEXT("Compressing"));

	Asset->Save();

	return true;
#undef RETURN_IF_CANCEL
}