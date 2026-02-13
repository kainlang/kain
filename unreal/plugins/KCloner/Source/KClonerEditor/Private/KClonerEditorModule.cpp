// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerEditorModule.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_KClonerData.h"
#include "AssetTypeActions_KClonerModifierPreset.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISequencerModule.h"
#include "KClonerActor.h"
#include "KClonerActorDetails.h"
#include "KClonerDataDetails.h"
#include "KClonerValidation.h"
#include "LevelEditor.h"
#include "MovieSceneKClonerModifierTrackEditor.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FKClonerEditorModule"

void FKClonerEditorModule::StartupModule() {
  RegisterAssetTools();
  RegisterLevelEditorExtensions();
  if (FModuleManager::Get().IsModuleLoaded("Sequencer")) {
    ISequencerModule &SequencerModule =
        FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
    SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(
        &FKClonerModifierTrackEditor::CreateTrackEditor));
  }

  // Register Detail Customization
  FPropertyEditorModule &PropertyModule =
      FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
          "PropertyEditor");
  PropertyModule.RegisterCustomClassLayout(
      "KClonerActor", FOnGetDetailCustomizationInstance::CreateStatic(
                          &FKClonerActorDetails::MakeInstance));
  PropertyModule.RegisterCustomClassLayout(
      "KClonerData", FOnGetDetailCustomizationInstance::CreateStatic(
                         &FKClonerDataDetails::MakeInstance));
}

void FKClonerEditorModule::ShutdownModule() {
  UnregisterAssetTools();
  UnregisterLevelEditorExtensions();

  // Unregister Detail Customization
  if (FModuleManager::Get().IsModuleLoaded("PropertyEditor")) {
    FPropertyEditorModule &PropertyModule =
        FModuleManager::GetModuleChecked<FPropertyEditorModule>(
            "PropertyEditor");
    PropertyModule.UnregisterCustomClassLayout("KClonerActor");
    PropertyModule.UnregisterCustomClassLayout("KClonerData");
  }

  if (FModuleManager::Get().IsModuleLoaded("Sequencer")) {
    ISequencerModule &SequencerModule =
        FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
    // todo: verify unregistration logic
  }
}

void FKClonerEditorModule::RegisterAssetTools() {
  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  // Register custom category "K-Studio"
  EAssetTypeCategories::Type KStudioCategory =
      AssetTools.RegisterAdvancedAssetCategory(
          FName(TEXT("KStudio")), LOCTEXT("KStudioCategory", "K-Studio"));

  // Register Asset Actions for KClonerData
  TSharedRef<IAssetTypeActions> DataAction =
      MakeShareable(new FAssetTypeActions_KClonerData(KStudioCategory));
  AssetTools.RegisterAssetTypeActions(DataAction);
  RegisteredAssetTypeActions.Add(DataAction);

  // Register Asset Actions for KClonerModifierPreset
  TSharedRef<IAssetTypeActions> PresetAction = MakeShareable(
      new FAssetTypeActions_KClonerModifierPreset(KStudioCategory));
  AssetTools.RegisterAssetTypeActions(PresetAction);
  RegisteredAssetTypeActions.Add(PresetAction);
}

void FKClonerEditorModule::UnregisterAssetTools() {
  if (FModuleManager::Get().IsModuleLoaded("AssetTools")) {
    IAssetTools &AssetTools =
        FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
    for (auto Action : RegisteredAssetTypeActions) {
      AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
    }
  }
  RegisteredAssetTypeActions.Empty();
}

void FKClonerEditorModule::RegisterLevelEditorExtensions() {
  FLevelEditorModule &LevelEditorModule =
      FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

  // Create and store the delegate
  FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors Delegate =
      FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(
          this, &FKClonerEditorModule::OnExtendLevelEditorMenu);

  // Get the handle BEFORE adding
  LevelEditorMenuExtenderHandle = Delegate.GetHandle();

  // Add to the array
  LevelEditorModule.GetAllLevelViewportContextMenuExtenders().Add(Delegate);
}

void FKClonerEditorModule::UnregisterLevelEditorExtensions() {
  if (FModuleManager::Get().IsModuleLoaded("LevelEditor")) {
    FLevelEditorModule &LevelEditorModule =
        FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");

    // Remove by matching the stored handle
    LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll(
        [this](
            const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors
                &InDelegate) {
          return InDelegate.GetHandle() == LevelEditorMenuExtenderHandle;
        });
  }
}

TSharedRef<FExtender> FKClonerEditorModule::OnExtendLevelEditorMenu(
    const TSharedRef<FUICommandList> CommandList,
    const TArray<AActor *> SelectedActors) {
  TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

  Extender->AddMenuExtension(
      "ActorControl", EExtensionHook::After, CommandList,
      FMenuExtensionDelegate::CreateLambda([](FMenuBuilder &MenuBuilder) {
        MenuBuilder.BeginSection("KStudio",
                                 LOCTEXT("KStudioSection", "K-Studio"));
        {
          MenuBuilder.AddMenuEntry(
              LOCTEXT("SpawnKCloner", "Spawn K-Cloner"),
              LOCTEXT("SpawnKClonerTooltip",
                      "Creates a new K-Cloner Actor at the current location"),
              FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]() {
                if (GEditor && GEditor->GetEditorWorldContext().World()) {
                  UWorld *World = GEditor->GetEditorWorldContext().World();
                  FVector Location = FVector::ZeroVector;

                  // Try to get cursor location
                  if (GEditor->ClickLocation != FVector::ZeroVector) {
                    Location = GEditor->ClickLocation;
                  }

                  FActorSpawnParameters SpawnParams;
                  SpawnParams.SpawnCollisionHandlingOverride =
                      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                  AKClonerActor *NewCloner = World->SpawnActor<AKClonerActor>(
                      AKClonerActor::StaticClass(), Location,
                      FRotator::ZeroRotator, SpawnParams);
                  if (NewCloner) {
                    GEditor->SelectNone(false, true);
                    GEditor->SelectActor(NewCloner, true, true);
                  }
                }
              })));
          MenuBuilder.AddMenuEntry(
              LOCTEXT("ValidateVATNormals", "Validate VAT Normals"),
              LOCTEXT("ValidateVATNormalsTip",
                      "Runs VAT normal rotation validation"),
              FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]() {
                if (GEditor && GEditor->GetEditorWorldContext().World()) {
                  KClonerValidation::ValidateVATNormals(
                      GEditor->GetEditorWorldContext().World());
                }
              })));
          MenuBuilder.AddMenuEntry(
              LOCTEXT("ValidateTextureSampling", "Validate Texture Sampling"),
              LOCTEXT("ValidateTextureSamplingTip",
                      "Runs texture sampling validation"),
              FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]() {
                if (GEditor && GEditor->GetEditorWorldContext().World()) {
                  KClonerValidation::ValidateTextureSampling(
                      GEditor->GetEditorWorldContext().World());
                }
              })));
        }
        MenuBuilder.EndSection();
      }));

  return Extender;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKClonerEditorModule, KClonerEditor)
