// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelWorldDetails.h"
#include "VoxelWorld.h"
#include "VoxelStaticWorld.h"
#include "VoxelData/VoxelData.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "VoxelRender/VoxelProcMeshBuffers.h"
#include "VoxelRender/VoxelMaterialInterface.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelDataTools.inl"
#include "VoxelEditorDetailsUtilities.h"
#include "VoxelMessages.h"
#include "VoxelFeedbackContext.h"
#include "VoxelScopedTransaction.h"

#include "Modules/ModuleManager.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "RawMesh.h"
#include "PhysicsEngine/BodySetup.h"

// The sort order is being silly, so force set it
#include "Editor/PropertyEditor/Private/DetailCategoryBuilderImpl.h"

inline FString GetVoxelWorldSaveFilePath(AVoxelWorld& World, bool bIsLoad)
{
	if ((bIsLoad && FPaths::FileExists(World.SaveFilePath)) || (!bIsLoad && !World.SaveFilePath.IsEmpty()))
	{
		return World.SaveFilePath;
	}
	else
	{
		TArray<FString> OutFiles;
		if (FDesktopPlatformModule::Get()->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Choose File"),
			FPaths::ProjectSavedDir(),
			"",
			TEXT("Voxel Save (*.voxelsave)|*.voxelsave"),
			EFileDialogFlags::None,
			OutFiles))
		{
			ensure(OutFiles.Num() == 1);
			return OutFiles[0];
		}
		else
		{
			return "";
		}
	}
}

void FVoxelWorldDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	FVoxelEditorUtilities::EnableRealtime();
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	// Disabled as it makes BP compilation crash when calling PostEditChange
	//for (auto& Object : Objects)
	//{
	//	World->UpdateCollisionProfile();
	//	World->PostEditChange();
	//}
		
	// Material config specific setup
	if (Objects.Num() == 1)
	{
		auto* World = CastChecked<AVoxelWorld>(Objects[0]);
		switch (World->MaterialConfig)
		{
		case EVoxelMaterialConfig::RGB:
			DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, MaterialCollection));
			DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, LODMaterialCollections));
			if (World->RGBHardness != EVoxelRGBHardness::FourWayBlend && World->RGBHardness != EVoxelRGBHardness::FiveWayBlend)
			{
				DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, MaterialsHardness));
			}
			break;
		case EVoxelMaterialConfig::SingleIndex:
		case EVoxelMaterialConfig::MultiIndex:
			DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, VoxelMaterial));
			DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, LODMaterials));
			DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, RGBHardness));
			break;
		default:
			ensure(false);
			break;
		}

		switch (World->UVConfig)
		{
		case EVoxelUVConfig::GlobalUVs:
			break;
		case EVoxelUVConfig::PackWorldUpInUVs:
		case EVoxelUVConfig::PerVoxelUVs:
			DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, UVScale));
			break;
		default:
			ensure(false);
			break;
		}
		
		const FSimpleDelegate RefreshDelegate = FSimpleDelegate::CreateLambda([Properties = MakeWeakPtr(DetailLayout.GetPropertyUtilities())]()
		{
			if (Properties.IsValid())
			{
				Properties.Pin()->ForceRefresh();
			}
		});
		DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, MaterialConfig))->SetOnPropertyValueChanged(RefreshDelegate);
		DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, UVConfig))->SetOnPropertyValueChanged(RefreshDelegate);
		DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, RGBHardness))->SetOnPropertyValueChanged(RefreshDelegate);
	}

	DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, EditorOnly_NewScale));

	if (bIsDataAssetEditor)
	{
		DetailLayout.HideCategory("Voxel - Save");
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, Generator));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bCreateWorldAutomatically));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bUseCameraIfNoInvokersFound));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bEnableUndoRedo));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bEnableCustomWorldRebasing));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bMergeAssetActors));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bMergeDisableEditsBoxes));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bCreateGlobalPool));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, ProcMeshClass));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bRenderWorld));
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, bStaticWorld));
		DetailLayout.HideCategory("Voxel - Spawners");
		DetailLayout.HideCategory("Physics");
		DetailLayout.HideCategory("Voxel - Collisions");
		DetailLayout.HideCategory("Voxel - Navmesh");
		DetailLayout.HideCategory("Voxel - Multiplayer");
		DetailLayout.HideCategory("Replication");
		DetailLayout.HideCategory("Input");
		DetailLayout.HideCategory("Actor");
		DetailLayout.HideCategory("Cooking");
		DetailLayout.HideCategory("TransformCommon");
		DetailLayout.HideCategory("ComponentReplication");
		DetailLayout.HideCategory("Variable");
		DetailLayout.HideCategory("Tick");
		DetailLayout.HideCategory("Voxel - Preview");
		DetailLayout.HideCategory("Voxel - Bake");
	}

	{
		// Component settings not affecting the voxel world
		DetailLayout.HideCategory("Lighting");
		DetailLayout.HideCategory("Tags");
		DetailLayout.HideCategory("Activation");
		DetailLayout.HideCategory("Rendering");
		DetailLayout.HideCategory("AssetUserData");
		DetailLayout.HideCategory("Mobile");

		// No HLOD for voxels
		DetailLayout.HideCategory("HLOD");

		// Manually handling those
		DetailLayout.HideCategory("Collision");

		const auto SortCategory = [&](FName Name, uint32 Order, bool bCollapsed, FString NewName = {})
		{
			FText NewNameText;
			if (!NewName.IsEmpty())
			{
				NewNameText = FText::FromString(NewName);
			}
			auto& Builder = static_cast<FDetailCategoryImpl&>(DetailLayout.EditCategory(Name, NewNameText));
			Builder.SetSortOrder(Order);
			Builder.InitiallyCollapsed(bCollapsed);
		};

		uint32 Order = 1000;
		SortCategory("Voxel - Preview", Order++, false);
		SortCategory("Voxel - Save", Order++, true);
		SortCategory("Voxel - General", Order++, false);
		SortCategory("Voxel - World Size", Order++, false);
		SortCategory("Voxel - Rendering", Order++, false);
		SortCategory("Voxel - Materials", Order++, false);
		SortCategory("Voxel - Spawners", Order++, true);
		SortCategory("Physics", Order++, true, "Voxel - Physics");
		SortCategory("Voxel - Collisions", Order++, true);
		SortCategory("Voxel - Navmesh", Order++, true);
		SortCategory("Voxel - LOD Settings", Order++, true);
		SortCategory("Voxel - Performance", Order++, true);
		SortCategory("Voxel - Multiplayer", Order++, true);
		SortCategory("Voxel - Bake", Order++, true);
		SortCategory("Replication", Order++, true);
		SortCategory("Input", Order++, true);
		SortCategory("Actor", Order++, true);
		SortCategory("Cooking", Order++, true);
		SortCategory("VirtualTexture", Order++, true);
	}

	const auto CreateWorldsDelegate = [Objects](auto Lambda)
	{
		return FOnClicked::CreateLambda([=]()
			{
				for (auto& Object : Objects)
				{
					auto* World = Cast<AVoxelWorld>(Object);
					if (World)
					{
						Lambda(*World);
					}
				}
				return FReply::Handled();
			});
	};
	const auto CreateWorldsEnabledDelegate = [Objects](auto Lambda)
	{
		return TAttribute<bool>::Create([=]()
			{
				for (auto& Object : Objects)
				{
					auto* World = Cast<AVoxelWorld>(Object);
					if (World)
					{
						if (!Lambda(*World))
						{
							return false;
						}
					}
				}
				return true;
			});
	};

	bool bIsBPEditor = false;
	bool bIsEditor = false;
	for (auto& Object : Objects)
	{
		bIsBPEditor = Object->GetWorld() == nullptr;
		bIsEditor = !bIsBPEditor && Object->GetWorld()->WorldType == EWorldType::Editor;
	}

	if (!bIsBPEditor && !bIsDataAssetEditor)
	{
		if (bIsEditor)
		{
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Preview",
				VOXEL_LOCTEXT("Toggle"),
				VOXEL_LOCTEXT("Toggle World Preview"),
				VOXEL_LOCTEXT("Toggle"),
				false,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					World.Toggle();
					GEditor->SelectActor(&World, true, true, true, true);
				}));
			
			FVoxelEditorUtilities::AddPropertyToCategory(
				DetailLayout,
				"Voxel - Preview",
				GET_MEMBER_NAME_STATIC(AVoxelWorld, EditorOnly_NewScale),
				true);
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Preview",
				VOXEL_LOCTEXT("Scale"),
				VOXEL_LOCTEXT("Scale World Data"),
				VOXEL_LOCTEXT("Scale"),
				true,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					if (World.IsCreated())
					{
						if (EAppReturnType::Yes == FMessageDialog::Open(
							EAppMsgType::YesNoCancel,
							VOXEL_LOCTEXT("Scaling data might take a while/crash your PC! Do you want to continue?")))
						{
							FVoxelScopedSlowTask Scope(1.f, VOXEL_LOCTEXT("Scaling data"));
							Scope.MakeDialog();
							Scope.EnterProgressFrame();

							FVoxelScopedTransaction Transaction(&World, "Scaling data", EVoxelChangeType::DataSwap);
							UVoxelBlueprintLibrary::ScaleData(&World, World.EditorOnly_NewScale);
						}
					}
				}));
			
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Preview",
				VOXEL_LOCTEXT("Clear All"),
				VOXEL_LOCTEXT("Clear World Data"),
				VOXEL_LOCTEXT("Clear All"),
				true,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					if (World.IsCreated())
					{
						if (EAppReturnType::Yes == FMessageDialog::Open(
							EAppMsgType::YesNoCancel, 
							VOXEL_LOCTEXT("This will clear all the voxel world edits! Do you want to continue?")))
						{
							World.GetData().ClearData();
							World.Toggle();
							World.Toggle();
						}
					}
				}));
			
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Preview",
				VOXEL_LOCTEXT("Clear Values"),
				VOXEL_LOCTEXT("Clear Value Data"),
				VOXEL_LOCTEXT("Clear Values"),
				true,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					if (World.IsCreated())
					{
						if (EAppReturnType::Yes == FMessageDialog::Open(
							EAppMsgType::YesNoCancel, 
							VOXEL_LOCTEXT("This will clear all the voxel world value edits! Do you want to continue?")))
						{
							FVoxelScopedTransaction Transaction(&World, "Clear values", EVoxelChangeType::DataSwap);
							UVoxelBlueprintLibrary::ClearValueData(&World);
						}
					}
				}));
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Preview",
				VOXEL_LOCTEXT("Clear Materials"),
				VOXEL_LOCTEXT("Clear Material Data"),
				VOXEL_LOCTEXT("Clear Materials"),
				true,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					if (World.IsCreated())
					{
						if (EAppReturnType::Yes == FMessageDialog::Open(
							EAppMsgType::YesNoCancel, 
							VOXEL_LOCTEXT("This will clear all the voxel world material edits! Do you want to continue?")))
						{
							FVoxelScopedTransaction Transaction(&World, "Clear materials", EVoxelChangeType::DataSwap);
							UVoxelBlueprintLibrary::ClearMaterialData(&World);
						}
					}
				}));
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Preview",
				VOXEL_LOCTEXT("Set Values Dirty"),
				VOXEL_LOCTEXT("Set Values as Dirty"),
				VOXEL_LOCTEXT("Set Values Dirty"),
				true,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					if (World.IsCreated())
					{
						if (EAppReturnType::Yes == FMessageDialog::Open(
							EAppMsgType::YesNoCancel,
							VOXEL_LOCTEXT("Setting values as dirty might take a while/crash your PC! Make sure your World Size is as small as possible. Do you want to continue?")))
						{
							FVoxelScopedSlowTask Scope(1.f, VOXEL_LOCTEXT("Setting values as dirty"));
							Scope.MakeDialog();
							Scope.EnterProgressFrame();

							FVoxelScopedTransaction Transaction(&World, "Setting values as dirty", EVoxelChangeType::DataSwap);
							UVoxelDataTools::SetBoxAsDirty(&World, FVoxelIntBox::Infinite, true, false);
							World.GetData().MarkAsDirty();
						}
					}
				}));
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Preview",
				VOXEL_LOCTEXT("Set Materials Dirty"),
				VOXEL_LOCTEXT("Set Materials as Dirty"),
				VOXEL_LOCTEXT("Set Materials Dirty"),
				true,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					if (World.IsCreated())
					{
						if (EAppReturnType::Yes == FMessageDialog::Open(
							EAppMsgType::YesNoCancel,
							VOXEL_LOCTEXT("Setting materials as dirty might take a while/crash your PC! Make sure your World Size is as small as possible. Do you want to continue?")))
						{
							FVoxelScopedSlowTask Scope(1.f, VOXEL_LOCTEXT("Setting materials as dirty"));
							Scope.MakeDialog();
							Scope.EnterProgressFrame();

							FVoxelScopedTransaction Transaction(&World, "Setting materials as dirty", EVoxelChangeType::DataSwap);
							UVoxelDataTools::SetBoxAsDirty(&World, FVoxelIntBox::Infinite, false, true);
							World.GetData().MarkAsDirty();
						}
					}
				}));
			FVoxelEditorUtilities::AddButtonToCategory(
				DetailLayout,
				"Voxel - Bake",
				VOXEL_LOCTEXT("Bake"),
				VOXEL_LOCTEXT("Bake World To Static Meshes"),
				VOXEL_LOCTEXT("Bake"),
				false,
				CreateWorldsDelegate([](AVoxelWorld& World)
				{
					if (World.IsCreated())
					{
						BakeWorld(World);
					}
				}),
				CreateWorldsEnabledDelegate([](AVoxelWorld& World)
				{
					return World.IsCreated();
				}));
		}

		FVoxelEditorUtilities::AddButtonToCategory(
			DetailLayout,
			"Voxel - Save",
			VOXEL_LOCTEXT("Load"),
			VOXEL_LOCTEXT("Load from Save Object"),
			VOXEL_LOCTEXT("Load"),
			false,
			CreateWorldsDelegate([](AVoxelWorld& World)
			{
				if (ensure(World.IsCreated()) && ensure(World.SaveObject))
				{
					World.LoadFromSaveObjectEditor();
				}
			}),
			CreateWorldsEnabledDelegate([](AVoxelWorld& World)
			{
				return World.IsCreated() && World.SaveObject;
			}));
		
		DetailLayout.HideProperty(GET_MEMBER_NAME_STATIC(AVoxelWorld, SaveObject));
		FVoxelEditorUtilities::AddPropertyToCategory(
			DetailLayout,
			"Voxel - Save",
			GET_MEMBER_NAME_STATIC(AVoxelWorld, SaveObject),
			false);

		FVoxelEditorUtilities::AddButtonToCategory(
			DetailLayout,
			"Voxel - Save",
			VOXEL_LOCTEXT("Save File"),
			VOXEL_LOCTEXT("Save to File"),
			VOXEL_LOCTEXT("Save"),
			true,
			CreateWorldsDelegate([](AVoxelWorld& World)
			{
				if (!ensure(World.IsCreated())) return;

				const FString Path = GetVoxelWorldSaveFilePath(World, false);
				if (Path.IsEmpty()) return;

				FText Error;
				if (World.SaveToFile(Path, Error))
				{
					World.SaveFilePath = Path;
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok, Error);
				}
			}),
			CreateWorldsEnabledDelegate([](AVoxelWorld& World)
			{
				return World.IsCreated();
			}));

		FVoxelEditorUtilities::AddButtonToCategory(
			DetailLayout,
			"Voxel - Save",
			VOXEL_LOCTEXT("Load File"),
			VOXEL_LOCTEXT("Load from File"),
			VOXEL_LOCTEXT("Load"),
			true,
			CreateWorldsDelegate([](AVoxelWorld& World)
			{
				if (!ensure(World.IsCreated())) return;

				const FString Path = GetVoxelWorldSaveFilePath(World, true);
				if (Path.IsEmpty()) return;

				FText Error;
				if (World.LoadFromFile(Path, Error))
				{
					World.SaveFilePath = Path;
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok, Error);
				}

			}),
			CreateWorldsEnabledDelegate([](AVoxelWorld& World)
			{
				return World.IsCreated();
			}));
}
}

inline UStaticMesh* BakeProcMesh(UVoxelProceduralMeshComponent* ProcMeshComp, const FString& PackageName, UStaticMesh* StaticMeshTemplate, bool bRecomputeNormals)
{
	const FName MeshName(*FPackageName::GetLongPackageAssetName(PackageName));

	// Raw mesh data we are filling in
	FRawMesh RawMesh;
	// Materials to apply to new mesh
	TArray<UMaterialInterface*> MeshMaterials;

	int32 VertexBase = 0;
	
	ProcMeshComp->IterateSections([&](const FVoxelProcMeshSectionSettings& SectionSettings, const FVoxelProcMeshBuffers& Buffers)
	{
		// Copy verts
		RawMesh.VertexPositions.Reserve(RawMesh.VertexPositions.Num() + Buffers.GetNumVertices());
		auto& PositionBuffer = Buffers.VertexBuffers.PositionVertexBuffer;
		for (int32 Index = 0; Index < Buffers.GetNumVertices(); Index++)
		{
			RawMesh.VertexPositions.Add(PositionBuffer.VertexPosition(Index));
		}

		auto& IndexBuffer = Buffers.IndexBuffer;
		auto& StaticMeshBuffer = Buffers.VertexBuffers.StaticMeshVertexBuffer;
		auto& ColorBuffer = Buffers.VertexBuffers.ColorVertexBuffer;

		// Copy 'wedge' info
		check(StaticMeshBuffer.GetNumTexCoords() <= MAX_MESH_TEXTURE_COORDS);
		for (int32 IndexIterator = 0; IndexIterator < IndexBuffer.GetNumIndices(); IndexIterator++)
		{
			const uint32 VertexIndex = IndexBuffer.GetIndex(IndexIterator);

			RawMesh.WedgeIndices.Add(VertexIndex + VertexBase);

			RawMesh.WedgeTangentX.Add(StaticMeshBuffer.VertexTangentX(VertexIndex));
			RawMesh.WedgeTangentY.Add(StaticMeshBuffer.VertexTangentY(VertexIndex));
			RawMesh.WedgeTangentZ.Add(StaticMeshBuffer.VertexTangentZ(VertexIndex));

			for (uint32 Tex = 0; Tex < StaticMeshBuffer.GetNumTexCoords(); Tex++)
			{
				RawMesh.WedgeTexCoords[Tex].Add(StaticMeshBuffer.GetVertexUV(VertexIndex, Tex));
			}
			RawMesh.WedgeColors.Add(ColorBuffer.VertexColor(VertexIndex));
		}

		// copy face info
		for (int32 Index = 0; Index < IndexBuffer.GetNumIndices() / 3; Index++)
		{
			RawMesh.FaceMaterialIndices.Add(MeshMaterials.Num());
			RawMesh.FaceSmoothingMasks.Add(0); // Assume this is ignored as bRecomputeNormals is false
		}

		UMaterialInterface* Material = nullptr;
		if (SectionSettings.Material.IsValid())
		{
			Material = SectionSettings.Material->GetMaterial();
		}
		if (auto* Instance = Cast<UMaterialInstanceDynamic>(Material))
		{
			Material = Instance->Parent;
		}

		// Remember material
		MeshMaterials.Add(Material);

		// Update offset for creating one big index/vertex buffer
		VertexBase += Buffers.GetNumVertices();
	});

	// If we got some valid data.
	if (RawMesh.VertexPositions.Num() >= 3 && RawMesh.WedgeIndices.Num() >= 3)
	{
		// Then find/create it.
		UPackage* Package = CreatePackage(*PackageName);
		check(Package);

		// Create StaticMesh object
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, MeshName, RF_Public | RF_Standalone, StaticMeshTemplate);
		StaticMesh->InitResources();

		// Add source to new StaticMesh
		FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
		SrcModel.BuildSettings.bRecomputeNormals = bRecomputeNormals;
		SrcModel.BuildSettings.bRecomputeTangents = bRecomputeNormals;
		SrcModel.BuildSettings.bRemoveDegenerates = false;
		SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
		SrcModel.BuildSettings.bGenerateLightmapUVs = true;
		SrcModel.BuildSettings.SrcLightmapIndex = 0;
		SrcModel.BuildSettings.DstLightmapIndex = 1;
		SrcModel.SaveRawMesh(RawMesh);

		// Copy materials to new mesh
		for (UMaterialInterface* Material : MeshMaterials)
		{
			StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
		}

		// Configure collision
		StaticMesh->CreateBodySetup();
		StaticMesh->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
		
		// Set the Imported version before calling the build
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		// Build mesh from source
		StaticMesh->Build(false);
		StaticMesh->PostEditChange();

		// Notify asset registry of new asset
		FAssetRegistryModule::AssetCreated(StaticMesh);
		Package->MarkPackageDirty();

		return StaticMesh;
	}
	else
	{
		return nullptr;
	}
}

void FVoxelWorldDetails::BakeWorld(AVoxelWorld& World)
{
	if (World.MaterialConfig != EVoxelMaterialConfig::RGB)
	{
		FMessageDialog::Open(EAppMsgType::Ok, VOXEL_LOCTEXT("Baking to static mesh requires to use RGB as material config."));
		return;
	}

	int32 ProcMeshCount = 0;
	const FString Path = World.BakedDataPath.FilePath
		+ "/"
		+ World.GetName()
		+ "_"
		+ FString::Printf(TEXT("%d_%d_%d_%d_%d_%d"),
			FDateTime::Now().GetSecond(),
			FDateTime::Now().GetMinute(),
			FDateTime::Now().GetHour(),
			FDateTime::Now().GetDay(),
			FDateTime::Now().GetMonth(),
			FDateTime::Now().GetYear());

	UClass* const StaticComponentClass = World.BakedMeshComponentTemplate ? World.BakedMeshComponentTemplate.Get() : UStaticMeshComponent::StaticClass();

	AVoxelStaticWorld* const StaticWorld = World.GetWorld()->SpawnActor<AVoxelStaticWorld>(AVoxelStaticWorld::StaticClass());
	StaticWorld->BaseMesh = NewObject<UStaticMeshComponent>(StaticWorld, StaticComponentClass);
	StaticWorld->BaseMesh->SetWorldTransform(World.GetTransform());
	StaticWorld->SetRootComponent(StaticWorld->BaseMesh);

	FVoxelScopedSlowTask Progress(World.GetComponents().Num(), VOXEL_LOCTEXT("Baking the voxel world to static meshes"));
	Progress.MakeDialog(true, true);
	for (auto* Component : World.GetComponents())
	{
		Progress.EnterProgressFrame();
		if (Progress.ShouldCancel())
		{
			StaticWorld->Destroy();
			return;
		}
		if (auto* ProcMesh = Cast<UVoxelProceduralMeshComponent>(Component))
		{
			auto* StaticMesh = BakeProcMesh(ProcMesh, Path + "/" + FString::Printf(TEXT("Mesh_%d"), ProcMeshCount), World.BakedMeshTemplate, World.bRecomputeNormalsBeforeBaking);
			if (StaticMesh)
			{
				auto* StaticMeshComp = NewObject<UStaticMeshComponent>(StaticWorld, StaticComponentClass);
				StaticMeshComp->AttachToComponent(StaticWorld->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				StaticMeshComp->SetStaticMesh(StaticMesh);
				StaticMeshComp->SetRelativeTransform(ProcMesh->GetRelativeTransform());
				StaticMeshComp->RegisterComponent();
				StaticWorld->Meshes.Add(StaticMeshComp);
				ProcMeshCount++;
			}
		}
	}
}
