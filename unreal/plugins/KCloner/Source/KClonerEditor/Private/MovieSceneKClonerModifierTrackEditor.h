// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISequencerTrackEditor.h"
#include "Runtime/Launch/Resources/Version.h"

class UMovieSceneKClonerModifierTrack;
class UMovieSceneKClonerModifierSection;
class AKClonerActor;

/**
 * Sequencer track editor for K-Cloner modifier keyframes.
 * UE 5.7 compatible.
 */
class FKClonerModifierTrackEditor : public ISequencerTrackEditor {
public:
  FKClonerModifierTrackEditor(TSharedRef<ISequencer> InSequencer);
  virtual ~FKClonerModifierTrackEditor() {}

  static TSharedRef<ISequencerTrackEditor>
  CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

  // ISequencerTrackEditor interface - UE 5.7 signatures
  virtual void BindCommands(TSharedRef<FUICommandList> InCommandList) override {
  }
  virtual void BuildAddTrackMenu(FMenuBuilder &MenuBuilder) override;

  // UE 5.7: MakeSectionInterface now takes UMovieSceneTrack& (reference) and
  // FGuid
  virtual TSharedRef<ISequencerSection>
  MakeSectionInterface(UMovieSceneSection &SectionObject,
                       UMovieSceneTrack &Track, FGuid ObjectBinding) override;

  virtual bool
  SupportsSequence(UMovieSceneSequence *InSequence) const override {
    return true;
  }
  virtual void Tick(float DeltaTime) override {}
  virtual const FSlateBrush *GetIconBrush() const override { return nullptr; }
// UE 5.7+ only methods
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  virtual FText GetDisplayName() const override {
    return NSLOCTEXT("KCloner", "KClonerModifierTrack", "K-Cloner Modifiers");
  }
#endif
  virtual UMovieSceneTrack *
  AddTrack(UMovieScene *FocusedMovieScene, const FGuid &ObjectHandle,
           TSubclassOf<class UMovieSceneTrack> TrackClass,
           FName UniqueTypeName) override;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  virtual void BuildPinnedAddTrackMenu(FMenuBuilder &MenuBuilder) override {}
#endif
  virtual void BuildObjectBindingColumnWidgets(
      TFunctionRef<TSharedRef<SHorizontalBox>()> GetEditBox,
      const UE::Sequencer::TViewModelPtr<UE::Sequencer::FObjectBindingModel>
          &ObjectBinding,
      const UE::Sequencer::FCreateOutlinerViewParams &InParams,
      const FName &InColumnName) override {}
  virtual void BuildObjectBindingTrackMenu(FMenuBuilder &MenuBuilder,
                                           const TArray<FGuid> &ObjectBindings,
                                           const UClass *ObjectClass) override {
  }
  virtual TSharedPtr<SWidget>
  BuildOutlinerEditWidget(const FGuid &ObjectBinding, UMovieSceneTrack *Track,
                          const FBuildEditWidgetParams &Params) override {
    return nullptr;
  }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  virtual TSharedPtr<SWidget>
  BuildOutlinerEditColumnWidget(const FGuid &ObjectBinding,
                                UMovieSceneTrack *Track,
                                const FBuildEditWidgetParams &Params) override {
    return nullptr;
  }
#endif
  virtual TSharedPtr<SWidget>
  BuildOutlinerColumnWidget(const FBuildColumnWidgetParams &Params,
                            const FName &ColumnName) override {
    return nullptr;
  }
  virtual void BuildTrackContextMenu(FMenuBuilder &MenuBuilder,
                                     UMovieSceneTrack *Track) override {}
// BuildTrackSidebarMenu is pure virtual in 5.5+, doesn't exist in 5.4
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
  virtual void BuildTrackSidebarMenu(FMenuBuilder &MenuBuilder,
                                     UMovieSceneTrack *Track) override {}
#endif
  virtual bool HandleAssetAdded(UObject *Asset,
                                const FGuid &TargetObjectGuid) override {
    return false;
  }
  virtual bool OnAllowDrop(const FDragDropEvent &DragDropEvent,
                           FSequencerDragDropParams &DragDropParams) override {
    return false;
  }
  virtual FReply
  OnDrop(const FDragDropEvent &DragDropEvent,
         const FSequencerDragDropParams &DragDropParams) override {
    return FReply::Unhandled();
  }
  virtual void OnInitialize() override {}
  virtual void OnRelease() override {}
  virtual bool
  SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;

private:
  TWeakPtr<ISequencer> Sequencer;
  void AddModifierFloatChannel(AKClonerActor *Cloner, FGuid ModifierGuid,
                               FName PropertyName);
  void AddModifierBoolChannel(AKClonerActor *Cloner, FGuid ModifierGuid,
                              FName PropertyName);
  void AddModifierVectorChannel(AKClonerActor *Cloner, FGuid ModifierGuid,
                                FName PropertyName);
};
