// Copyright 2026 K-Studio. All Rights Reserved.

#include "AssetTypeActions_KClonerData.h"
#include "KClonerEditor.h"
#include "KClonerActor.h"
#include "KClonerBakingUtils.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MeshMergeModule.h"
#include "MeshUtilities.h"
// MeshMergingSettings location changed in UE 5.5
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "MeshMerge/MeshMergingSettings.h"
#else
#include "Engine/MeshMerging.h"
#endif
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "GeometryCache.h"
#include "GeometryCacheTrack.h"
#include "KClonerVATUtils.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_KClonerData"

FText FAssetTypeActions_KClonerData::GetName() const
{
	return LOCTEXT("AssetName", "KCloner Data");
}

FColor FAssetTypeActions_KClonerData::GetTypeColor() const
{
	return FColor(0, 255, 200); // Cyan-ish
}

UClass* FAssetTypeActions_KClonerData::GetSupportedClass() const
{
	return UKClonerData::StaticClass();
}

uint32 FAssetTypeActions_KClonerData::GetCategories()
{
	return MyAssetCategory;
}

void FAssetTypeActions_KClonerData::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto Obj : InObjects)
	{
		UKClonerData* ClonerData = Cast<UKClonerData>(Obj);
		if (ClonerData)
		{
			TSharedRef<FKClonerEditor> Editor(new FKClonerEditor());
			Editor->InitKClonerEditor(Mode, EditWithinLevelEditor, ClonerData);
		}
	}
}

void FAssetTypeActions_KClonerData::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UKClonerData>> ClonerDataObjects;
	for (UObject* Obj : InObjects)
	{
		if (UKClonerData* ClonerData = Cast<UKClonerData>(Obj))
		{
			ClonerDataObjects.Add(ClonerData);
		}
	}

	// Bake to Static Mesh
	Section.AddMenuEntry(
		"KCloner_BakeToStaticMesh",
		LOCTEXT("BakeToStaticMesh", "Bake to Static Mesh"),
		LOCTEXT("BakeToStaticMeshTooltip", "Merge all cloner instances into a single Static Mesh asset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.StaticMesh"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_KClonerData::ExecuteBakeToStaticMesh, ClonerDataObjects),
			FCanExecuteAction()
		)
	);

	// Bake to Alembic
	Section.AddMenuEntry(
		"KCloner_BakeToAlembic",
		LOCTEXT("BakeToAlembic", "Bake to Alembic"),
		LOCTEXT("BakeToAlembicTooltip", "Export cloner animation as Alembic point cache file (.abc)"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SkeletalMesh"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_KClonerData::ExecuteBakeToAlembic, ClonerDataObjects),
			FCanExecuteAction()
		)
	);

	// Bake to Geometry Cache
	Section.AddMenuEntry(
		"KCloner_BakeToGeometryCache",
		LOCTEXT("BakeToGeometryCache", "Bake to Geometry Cache"),
		LOCTEXT("BakeToGeometryCacheTooltip", "Bake cloner animation to UE5 Geometry Cache asset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.MeshComponent"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_KClonerData::ExecuteBakeToGeometryCache, ClonerDataObjects),
			FCanExecuteAction()
		)
	);

	// Separator
	Section.AddSeparator("KCloner_VATSeparator");

	// Bake to VAT (GPU Mode)
	Section.AddMenuEntry(
		"KCloner_BakeToVAT",
		LOCTEXT("BakeToVAT", "Bake to VAT"),
		LOCTEXT("BakeToVATTooltip", "Bake modifier animation to Vertex Animation Textures for GPU instancing (100k+ instances)"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_KClonerData::ExecuteBakeToVAT, ClonerDataObjects),
			FCanExecuteAction()
		)
	);
}

void FAssetTypeActions_KClonerData::ExecuteBakeToStaticMesh(TArray<TWeakObjectPtr<UKClonerData>> Objects)
{
	for (TWeakObjectPtr<UKClonerData>& ObjPtr : Objects)
	{
		if (UKClonerData* ClonerData = ObjPtr.Get())
		{
			if (!ClonerData->SourceMesh)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSourceMesh", 
					"Cannot bake: No Source Mesh assigned to KCloner Data.\n\n"
					"Note: Static Mesh baking requires a Static Mesh source.\n"
					"Skeletal Mesh sources are not supported for this bake type."));
				continue;
			}

			// Create temporary actor to get transforms
			UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (!World)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorld", "Cannot bake: No editor world available."));
				return;
			}

			// Auto-add a default layer if none exist (common issue)
			if (ClonerData->Layers.Num() == 0)
			{
				FKClonerDistributionLayer DefaultLayer;
				DefaultLayer.bEnabled = true;
				DefaultLayer.Mode = EKClonerMode::Single;
				ClonerData->Layers.Add(DefaultLayer);
				ClonerData->MarkPackageDirty();
			}

			// Spawn temp cloner actor
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKClonerActor* TempCloner = World->SpawnActor<AKClonerActor>(AKClonerActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			
			if (TempCloner)
			{
				TempCloner->ApplyPreset(ClonerData);
				
				int32 InstanceCount = TempCloner->InstancedMesh ? TempCloner->InstancedMesh->GetInstanceCount() : 0;
				
				if (InstanceCount == 0)
				{
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoInstances", 
						"Cannot bake: No instances were generated.\n\n"
						"This usually means:\n"
						"• Distribution Layers array is empty or all layers disabled\n"
						"• Grid/Radial/Linear count is set to 0\n\n"
						"Please add at least one enabled distribution layer to generate instances."));
					World->DestroyActor(TempCloner);
					continue;
				}

				// Use MergeStaticMeshComponents for proper merging
				FString BasePackageName = ClonerData->GetOutermost()->GetName() + TEXT("_BakedMesh");
				
				// Ensure unique asset name
				FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
				FString UniquePackageName;
				FString UniqueAssetName;
				AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), UniquePackageName, UniqueAssetName);

				const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

				FMeshMergingSettings MergeSettings;
				MergeSettings.bMergePhysicsData = true;
				MergeSettings.bPivotPointAtZero = true; // Use Actor Filter (0,0,0) as pivot
				MergeSettings.LODSelectionType = EMeshLODSelectionType::SpecificLOD;
				MergeSettings.SpecificLOD = 0; // Bake LOD0

				TArray<UPrimitiveComponent*> ComponentsToMerge;
				ComponentsToMerge.Add(TempCloner->InstancedMesh);

				TArray<UObject*> AssetsToSync;
				FVector MergedLocation = FVector::ZeroVector;

				MeshUtilities.MergeComponentsToStaticMesh(
					ComponentsToMerge, 
					World, 
					MergeSettings, 
					nullptr, 
					nullptr, 
					UniquePackageName, 
					AssetsToSync, 
					MergedLocation, 
					0.0f, 
					true // bSilent: false = Show dialogs if needed? Standard is true usually for tools, but let's keep it typical.
				);

				// Notify user
				FNotificationInfo Info(LOCTEXT("BakeSuccess", "Bake Complete"));
				Info.ExpireDuration = 3.0f;
				Info.SubText = FText::Format(LOCTEXT("BakeSuccessSub", "Created {0}"), FText::FromString(UniqueAssetName));
				FSlateNotificationManager::Get().AddNotification(Info);
				
				World->DestroyActor(TempCloner);
			}
		}
	}
}

void FAssetTypeActions_KClonerData::ExecuteBakeToAlembic(TArray<TWeakObjectPtr<UKClonerData>> Objects)
{
	FMessageDialog::Open(EAppMsgType::Ok, 
		LOCTEXT("AlembicNotYet", "Alembic Export\n\nThis feature requires the AlembicExporter plugin.\n\nWorkflow:\n1. Place K-Cloner Actor in level\n2. Select the actor\n3. File → Export Selected → Alembic (.abc)\n\nFull integration coming in a future update.")
	);
}

void FAssetTypeActions_KClonerData::ExecuteBakeToGeometryCache(TArray<TWeakObjectPtr<UKClonerData>> Objects)
{
	FMessageDialog::Open(EAppMsgType::Ok, 
		LOCTEXT("GeometryCacheNotYet", "Geometry Cache Export\n\nThis feature requires recording your K-Cloner animation:\n\n1. Place K-Cloner Actor in level\n2. Open Level Sequence\n3. Add the K-Cloner Actor\n4. Right-click → Bake to Geometry Cache\n\nFull integration coming in a future update.")
	);
}

void FAssetTypeActions_KClonerData::ExecuteBakeToVAT(TArray<TWeakObjectPtr<UKClonerData>> Objects)
{
	for (TWeakObjectPtr<UKClonerData>& ObjPtr : Objects)
	{
		if (UKClonerData* ClonerData = ObjPtr.Get())
		{
			// Accept either Static Mesh or Skeletal Mesh as source
			bool bHasValidSource = (ClonerData->SourceMesh != nullptr) || (ClonerData->SourceSkeletalMesh != nullptr);
			if (!bHasValidSource)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSourceMeshVAT", 
					"Cannot bake VAT: No Source Mesh assigned to KCloner Data.\n\n"
					"Please assign either:\n"
					"• Source Mesh (Static Mesh), or\n"
					"• Source Skeletal Mesh"));
				continue;
			}

			// Create temporary actor to generate instances
			UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (!World)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorldVAT", "Cannot bake VAT: No editor world available."));
				return;
			}

			// Auto-add a default layer if none exist (common issue)
			if (ClonerData->Layers.Num() == 0)
			{
				FKClonerDistributionLayer DefaultLayer;
				DefaultLayer.bEnabled = true;
				DefaultLayer.Mode = EKClonerMode::Single;
				ClonerData->Layers.Add(DefaultLayer);
				ClonerData->MarkPackageDirty();
			}

			// Spawn temp cloner actor
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKClonerActor* TempCloner = World->SpawnActor<AKClonerActor>(AKClonerActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			
			if (TempCloner)
			{
				TempCloner->ApplyPreset(ClonerData);
				
				// For skeletal mesh sources, ensure the cloner uses static mesh representation for VAT
				// VAT bakes instance transforms, not skeletal bone animation
				if (!TempCloner->SourceMesh && TempCloner->SourceSkeletalMesh)
				{
					// Skeletal mesh mode - need to get a static mesh representation
					// For now, show a helpful message explaining the current limitation
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SkeletalVATInfo", 
						"Note: K-Cloner VAT bakes instance TRANSFORMS into textures.\n\n"
						"This is different from the VertexAnimationManager plugin which bakes\n"
						"skeletal bone ANIMATION into textures.\n\n"
						"Current workflow for skeletal mesh cloners:\n"
						"1. Set up your cloner with distribution layers\n"
						"2. Add modifiers (Noise, Wave, etc.) to animate transforms\n"
						"3. Bake to VAT to capture that transform animation\n\n"
						"To bake actual skeletal animation into VAT textures,\n"
						"use the VertexAnimationManager plugin or similar."));
					World->DestroyActor(TempCloner);
					continue;
				}
				
				int32 InstanceCount = TempCloner->InstancedMesh ? TempCloner->InstancedMesh->GetInstanceCount() : 0;
				
				if (InstanceCount == 0)
				{
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoInstancesVAT", 
						"Cannot bake VAT: No instances were generated.\n\n"
						"This usually means:\n"
						"• Distribution Layers array is empty or all layers disabled\n"
						"• Grid/Radial/Linear count is set to 0\n\n"
						"Please add at least one enabled distribution layer to generate instances."));
					World->DestroyActor(TempCloner);
					continue;
				}

				// Set up VAT options
				FKClonerVATOptions Options;
				Options.Duration = 5.0f; // 5 seconds of animation
				Options.FrameRate = 30.0f;
				Options.Precision = EKClonerVATPrecision::High;
				Options.PackagePath = ClonerData->GetOutermost()->GetName() + TEXT("_VAT");
				
				// Bake!
				FKClonerVATResult Result = FKClonerVATUtils::BakeToVAT(TempCloner, Options);
				
				World->DestroyActor(TempCloner);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
