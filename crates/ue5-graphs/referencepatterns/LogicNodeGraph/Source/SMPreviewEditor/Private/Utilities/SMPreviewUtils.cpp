// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMPreviewUtils.h"

#include "SMPreviewObject.h"
#include "Views/Viewport/SMPreviewModeViewportClient.h"

#include "SMInstance.h"
#include "Blueprints/SMBlueprint.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "ISceneOutliner.h"
#include "ISceneOutlinerMode.h"
#include "LevelEditor.h"
#include "ActorMode.h"
#include "SceneOutlinerPublicTypes.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/Docking/SDockTab.h"

#include "Components/ModelComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameEngine.h"

#define LOCTEXT_NAMESPACE "SMPreviewUtils"

const FString FSMPreviewUtils::PreviewPackagePrefix = "LogicDriverPreviewPackage_";
const FString FSMPreviewUtils::PreviewPackageSimulationPrefix = "LogicDriverSimulationWorld_";

static FDelegateHandle OnPackagedDirtyFlagChangedHandle;
static FDelegateHandle MapChangedHandle;
static TSet<USMBlueprint*> SimulatingBlueprints;

USMInstance* FSMPreviewUtils::StartSimulation(USMBlueprint* Blueprint)
{
	check(Blueprint);
	
	StopSimulation(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	SimulatingBlueprints.Add(Blueprint);
	
	USMInstance* PreviewInstance = nullptr;

	if (USMPreviewObject* PreviewObject = Blueprint->GetPreviewObject(false))
	{
		if (PreviewObject->GetStateMachineTemplate())
		{
			if (UObject* Context = GetContextForPreview(Blueprint))
			{
				PreviewInstance = PreviewObject->InitializeStateMachine(Context);

				const TWeakPtr<FSMPreviewModeViewportClient> PreviewClient = GetViewportClient(Blueprint);
				if (PreviewClient.IsValid())
				{
					//  Clone the world.
					if (UWorld* SimulatedWorld = PreparePreviewWorld(Blueprint))
					{
						// Best to clear the selection since the world is changing.
						PreviewClient.Pin()->ResetSelection();
						
						// The context should have been cloned to the new world.
						ASMPreviewStateMachineActor* ClonedActor = FindObjectFast<ASMPreviewStateMachineActor>(SimulatedWorld->PersistentLevel, *PreviewObject->GetPreviewStateMachineActor()->GetName());
						check(ClonedActor);

						UObject* ClonedContext = FindObjectFast<UObject>(SimulatedWorld->PersistentLevel, *Context->GetName());
						check(ClonedContext);

						// Enable input for pawns if configured.
						if (PreviewObject->ShouldPossessPawnContext())
						{
							if (APawn* ClonedPawnContext = Cast<APawn>(ClonedContext))
							{
								if (APlayerController* Controller = SimulatedWorld->GetFirstPlayerController())
								{
									Controller->Possess(ClonedPawnContext);
								}
							}
						}
						
						PreviewInstance = ClonedActor->StateMachineInstance;
						PreviewObject->SetSimulatedStateMachineInstance(PreviewInstance);
						PreviewInstance->Initialize(ClonedContext); // Needs to reinitialize after a clone.

						PreviewObject->SetCurrentWorld(SimulatedWorld);
					}
				}
			}
		}

		PreviewObject->NotifySimulationStarted();
	}

	Blueprint->SetObjectBeingDebugged(PreviewInstance);

	if (PreviewInstance)
	{
		PreviewInstance->Start();
	}
	
	return PreviewInstance;
}

void FSMPreviewUtils::StopSimulation(USMBlueprint* Blueprint)
{
	check(Blueprint);

	SimulatingBlueprints.Remove(Blueprint);
	
	if (USMPreviewObject* PreviewObject = Blueprint->GetPreviewObject(false))
	{
		PreviewObject->NotifySimulationEnded();
		PreviewObject->ShutdownStateMachine();
	}
	
	TWeakPtr<FSMPreviewModeViewportClient> PreviewClient = GetViewportClient(Blueprint);
	if (PreviewClient.IsValid())
	{
		// Best to clear the selection since the world is changing.
		PreviewClient.Pin()->ResetSelection();
		PreviewClient.Pin()->GetOurPreviewScene()->RestoreOriginalWorld();
	}

	if (USMPreviewObject* PreviewObject = Blueprint->GetPreviewObject(false))
	{
		PreviewObject->SetCurrentWorld(PreviewObject->GetPreviewWorld());
	}

	if (FSMBlueprintEditor* BlueprintEditor = FSMBlueprintEditorUtils::GetStateMachineEditor(Blueprint))
	{
		if (!BlueprintEditor->IsShuttingDown())
		{
			// Private member standalone host will be null and crash if shutting down,
			// otherwise we want to regenerate toolbars to update the Simulate/Stop button.
			BlueprintEditor->RegenerateMenusAndToolbars();
		}
	}
}

void FSMPreviewUtils::StopAllSimulations()
{
	TArray<USMBlueprint*> BlueprintsSimulatingArr(SimulatingBlueprints.Array());
	for (USMBlueprint* Blueprint : BlueprintsSimulatingArr)
	{
		StopSimulation(Blueprint);
	}
}

UObject* FSMPreviewUtils::GetContextForPreview(USMBlueprint* Blueprint)
{
	UObject* Context = nullptr;
	
	TWeakPtr<FSMPreviewModeViewportClient> PreviewClient = GetViewportClient(Blueprint);
	if (PreviewClient.IsValid())
	{
		// Fist check if there is an actor assigned.
		Context = PreviewClient.Pin()->GetOurPreviewScene()->GetContextActorForCurrentWorld();
	}

	if (!Context)
	{
		// Use the object model instead.
		Context = Blueprint->GetPreviewObject()->GetContextActor();
	}

	return Context;
}

USMPreviewObject* FSMPreviewUtils::GetPreviewObject(TWeakPtr<FSMBlueprintEditor> BlueprintEditor)
{
	if (BlueprintEditor.IsValid())
	{
		if (USMBlueprint* Blueprint = BlueprintEditor.Pin()->GetStateMachineBlueprint())
		{
			return Blueprint->GetPreviewObject();
		}
	}

	return nullptr;
}

UWorld* FSMPreviewUtils::DuplicateWorldForSimulation(const FString& PackageName, UWorld* OwningWorld)
{
	// See DuplicateWorldForPIE
	
	// Find the original (non-PIE) level package
	UPackage* EditorLevelPackage = FindObjectFast<UPackage>(nullptr, FName(*PackageName));
	if (!EditorLevelPackage)
	{
		return nullptr;
	}
	
	// Find world object and use its PersistentLevel pointer.
	UWorld* EditorLevelWorld = OwningWorld;// UWorld::FindWorldInPackage(EditorLevelPackage);

	// If the world was not found, try to follow a redirector, if there is one
	if (!EditorLevelWorld)
	{
		EditorLevelWorld = UWorld::FollowWorldRedirectorInPackage(EditorLevelPackage);
		if (EditorLevelWorld)
		{
			EditorLevelPackage = EditorLevelWorld->GetPackage();
		}
	}

	if (!EditorLevelWorld)
	{
		return nullptr;
	}

	const FString PackageAssetPath = FPackageName::GetLongPackagePath(PackageName);
	const FString SimulationName = PreviewPackageSimulationPrefix + FGuid::NewGuid().ToString();
	const FString PrefixedLevelName = FString::Printf(TEXT("%s/%s%s"), *PackageAssetPath, *SimulationName, *EditorLevelWorld->GetName());

	const FName PrefixedLevelFName = FName(*PrefixedLevelName);
	//FSoftObjectPath::AddPIEPackageName(PrefixedLevelFName);

	UWorld::WorldTypePreLoadMap.FindOrAdd(PrefixedLevelFName) = EWorldType::PIE;
	UPackage* SimluationLevelPackage = CreatePackage(*PrefixedLevelName);
	//SimluationLevelPackage->SetPackageFlags(PKG_PlayInEditor);
	SimluationLevelPackage->SetFlags(RF_Transient);
	SimluationLevelPackage->MarkAsFullyLoaded();

	//ULevel::StreamedLevelsOwningWorld.Add(PIELevelPackage->GetFName(), OwningWorld);
	UWorld* SimulationLevelWorld = CastChecked<UWorld>(StaticDuplicateObject(EditorLevelWorld, SimluationLevelPackage, EditorLevelWorld->GetFName(), RF_AllFlags, nullptr, EDuplicateMode::PIE));

	SimulationLevelWorld->Scene = EditorLevelWorld->Scene;
	
	// Ensure the feature level matches the editor's, this is required as FeatureLevel is not a UPROPERTY and is not duplicated from EditorLevelWorld.
	SimulationLevelWorld->SetFeatureLevel(EditorLevelWorld->GetFeatureLevel());

	// Clean up the world type list and owning world list now that PostLoad has occurred
	UWorld::WorldTypePreLoadMap.Remove(PrefixedLevelFName);
	ULevel::StreamedLevelsOwningWorld.Remove(SimluationLevelPackage->GetFName());

	{
		ULevel* EditorLevel = EditorLevelWorld->PersistentLevel;
		ULevel* SimulationLevel = SimulationLevelWorld->PersistentLevel;

		// If editor has run construction scripts or applied level offset, we dont do it again
		SimulationLevel->bAlreadyMovedActors = EditorLevel->bAlreadyMovedActors;
		SimulationLevel->bHasRerunConstructionScripts = EditorLevel->bHasRerunConstructionScripts;

		// Fixup model components. The index buffers have been created for the components in the EditorWorld and the order
		// in which components were post-loaded matters. So don't try to guarantee a particular order here, just copy the
		// elements over.
		if (SimulationLevel->Model != nullptr
			&& SimulationLevel->Model == EditorLevel->Model
			&& SimulationLevel->ModelComponents.Num() == EditorLevel->ModelComponents.Num())
		{
			SimulationLevel->Model->ClearLocalMaterialIndexBuffersData();
			for (int32 ComponentIndex = 0; ComponentIndex < SimulationLevel->ModelComponents.Num(); ++ComponentIndex)
			{
				UModelComponent* SrcComponent = EditorLevel->ModelComponents[ComponentIndex];
				UModelComponent* DestComponent = SimulationLevel->ModelComponents[ComponentIndex];
				DestComponent->CopyElementsFrom(SrcComponent);
			}
		}
	}

	SimulationLevelWorld->ClearFlags(RF_Standalone | RF_Public | RF_Transactional); /* Transactions can result in a crash after an undo/compile. Not needed anyway. */
	SimulationLevelWorld->SetFlags(RF_Transient);
	SimulationLevelWorld->PersistentLevel->ClearFlags(RF_Transactional);
	SimulationLevelWorld->PersistentLevel->SetFlags(RF_Transient);
	//EditorLevelWorld->TransferBlueprintDebugReferences(SimulationLevelWorld);
	
	SimulationLevelWorld->AddToRoot();

	if (UGameInstance* GameInstance = EditorLevelWorld->GetGameInstance())
	{
		USMPreviewGameInstance* ClonedGameInstance = NewObject<USMPreviewGameInstance>(GameInstance->GetOuter() /* Should be GEngine */,
			USMPreviewGameInstance::StaticClass());

		FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(EditorLevelWorld);
		ClonedGameInstance->SetWorldContext(WorldContext);

		SimulationLevelWorld->SetGameInstance(ClonedGameInstance);
		if (ensureAlways(SimulationLevelWorld->GetGameInstance()))
		{
			// Game mode requires instance.
			SimulationLevelWorld->SetGameMode(FURL());
		}
	}

	SimulationLevelWorld->InitWorld();
	
	return SimulationLevelWorld;
}

TWeakPtr<FSMPreviewModeViewportClient> FSMPreviewUtils::GetViewportClient(USMBlueprint* Blueprint)
{
	// TODO: It would be nice if the preview client wasn't stored directly on the editor, but
	// could be retrieved from the preview mode widget.
	if (FSMBlueprintEditor* Editor = FSMBlueprintEditorUtils::GetStateMachineEditor(Blueprint))
	{
		return StaticCastSharedPtr<FSMPreviewModeViewportClient>(Editor->GetPreviewClient().Pin());
	}
	return nullptr;
}

bool FSMPreviewUtils::DoesWorldContainActor(UWorld* WorldToCheck, const AActor* CompareActor, bool bCheckName)
{
	if (ULevel* CurrentLevel = WorldToCheck->GetCurrentLevel())
	{
		if (CurrentLevel->Actors.Contains(CompareActor))
		{
			return true;
		}
		
		if (bCheckName)
		{
			return CurrentLevel->Actors.ContainsByPredicate([CompareActor](AActor* Actor)
			{
				return Actor && CompareActor && Actor->GetFName() == CompareActor->GetFName();
			});
		}
	}

	return false;
}

FString FSMPreviewUtils::MakeFullObjectPropertyName(UObject* InObject, FProperty* InProperty)
{
	check(InObject);
	check(InProperty);

	return InObject->GetName() + "_" + InProperty->GetFullName();
}

void FSMPreviewUtils::BindDelegates()
{
	UnbindDelegates();
	MapChangedHandle = FEditorDelegates::OnMapOpened.AddStatic(&FSMPreviewUtils::OnMapChanged);
	OnPackagedDirtyFlagChangedHandle = UPackage::PackageMarkedDirtyEvent.AddStatic(&FSMPreviewUtils::OnPackageDirtyFlagChanged);
}

void FSMPreviewUtils::UnbindDelegates()
{
	if (OnPackagedDirtyFlagChangedHandle.IsValid())
	{
		UPackage::PackageMarkedDirtyEvent.Remove(OnPackagedDirtyFlagChangedHandle);
	}
	if (MapChangedHandle.IsValid())
	{
		FEditorDelegates::OnMapOpened.Remove(MapChangedHandle);
	}
}

UWorld* FSMPreviewUtils::PreparePreviewWorld(USMBlueprint* Blueprint)
{
	const TWeakPtr<FSMPreviewModeViewportClient> PreviewClient = GetViewportClient(Blueprint);
	if (PreviewClient.IsValid())
	{
		PreviewClient.Pin()->GetOurPreviewScene()->CloneOriginalWorldToPreviewWorld();
		return PreviewClient.Pin()->GetOurPreviewScene()->GetWorld();
	}

	return nullptr;
}

void FSMPreviewUtils::OnPackageDirtyFlagChanged(UPackage* Package, bool bWasDirty)
{
	if (Package && Package->IsDirty())
	{
		const FString PackageName = Package->GetName();
		if (PackageName.Contains(FSMPreviewUtils::PreviewPackagePrefix) || PackageName.Contains(FSMPreviewUtils::PreviewPackageSimulationPrefix))
		{
			// Hack: Our packages should never be considered dirty as they do not save and can cause warnings to popup when trying to change levels if they are dirty.
			Package->ClearDirtyFlag();
		}
	}
}

void FSMPreviewUtils::OnMapChanged(const FString& MapName, bool bAsTemplate)
{
	StopAllSimulations();
}

/*
 * Preview Outliner Utils
 */

bool FSMPreviewOutlinerUtils::RefreshLevelEditorOutliner(FSMAdvancedPreviewScene* PreviewOwner)
{
	class FSMActorMode : public FActorMode
	{
	public:
		explicit FSMActorMode(const FActorModeParams& Params)
			: FActorMode(Params)
		{
		}

		// HACK: Retrieve the protected RepresentingWorld property.
		static TWeakObjectPtr<UWorld> GetWorld(FActorMode* InActorMode)
		{
			if (InActorMode == nullptr)
			{
				return nullptr;
			}
			return ((FSMActorMode*)InActorMode)->RepresentingWorld;
		}
	};
	
	const FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	const TWeakPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetLevelEditorInstance();
	if (LevelEditor.IsValid())
	{
		TArray<TWeakPtr<ISceneOutliner>> SceneOutliners = LevelEditor.Pin()->GetAllSceneOutliners();
		for (const TWeakPtr<ISceneOutliner>& SceneOutlinerPtr : SceneOutliners)
		{
			if (SceneOutlinerPtr.IsValid())
			{
				// Find the world the main level scene outliner is running for.
				// If it is ours we need to refresh the outliner.
			
				FActorMode* ActorMode = (FActorMode*)SceneOutlinerPtr.Pin()->GetMode();
				const TWeakObjectPtr<UWorld> World = FSMActorMode::GetWorld(ActorMode);

				UWorld* RepresentingWorld = World.Get();
				if (IsValid(RepresentingWorld))
				{
					if (UPackage* Package = RepresentingWorld->GetPackage())
					{
						const FString PackageName = Package->GetName();
						if (PackageName.Contains(FSMPreviewUtils::GetPreviewPackagePrefix()) || PackageName.Contains(FSMPreviewUtils::GetPreviewSimulationPrefix()))
						{
							SceneOutlinerPtr.Pin()->FullRefresh();
							return true;
						}
					}
				}
			}
		}
	}
	
	return false;
}

bool FSMPreviewOutlinerUtils::DoesTabBelongToPreview(TSharedPtr<FTabManager> InTabManager, USMBlueprint* SMBlueprint)
{
	check(InTabManager.IsValid());
	check(SMBlueprint);
	return InTabManager->HasTabSpawner(TEXT("SMBlueprintEditorPreviewTab_DetailsView")) && SMBlueprint->GetName() == InTabManager->GetOwnerTab()->GetTabLabel().ToString();
}

#undef LOCTEXT_NAMESPACE
