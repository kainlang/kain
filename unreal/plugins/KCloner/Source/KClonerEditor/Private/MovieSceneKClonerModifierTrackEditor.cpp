// Copyright 2026 K-Studio. All Rights Reserved.

#include "MovieSceneKClonerModifierTrackEditor.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISequencer.h"
#include "KClonerActor.h"
#include "KClonerModifier.h"
#include "KClonerSequencer.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "ScopedTransaction.h"
#include "SequencerUtilities.h"

// simple visual section - nothing fancy, just needs to exist for the track to work
class FSimpleSection : public ISequencerSection {
public:
  FSimpleSection(UMovieSceneSection &InSection) : Section(&InSection) {}
  virtual UMovieSceneSection *GetSectionObject() override { return Section; }
  virtual int32
  OnPaintSection(FSequencerSectionPainter &Painter) const override {
    return 0; // invisible section lol
  }

private:
  UMovieSceneSection *Section;
};

FKClonerModifierTrackEditor::FKClonerModifierTrackEditor(
    TSharedRef<ISequencer> InSequencer)
    : Sequencer(InSequencer) {}

TSharedRef<ISequencerTrackEditor>
FKClonerModifierTrackEditor::CreateTrackEditor(
    TSharedRef<ISequencer> InSequencer) {
  return MakeShared<FKClonerModifierTrackEditor>(InSequencer);
}

UMovieSceneTrack *FKClonerModifierTrackEditor::AddTrack(
    UMovieScene *FocusedMovieScene, const FGuid &ObjectHandle,
    TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) {
  if (!FocusedMovieScene ||
      TrackClass != UMovieSceneKClonerModifierTrack::StaticClass()) {
    return nullptr;
  }
  return FocusedMovieScene->AddTrack<UMovieSceneKClonerModifierTrack>(
      ObjectHandle);
}

bool FKClonerModifierTrackEditor::SupportsType(
    TSubclassOf<UMovieSceneTrack> TrackClass) const {
  return TrackClass == UMovieSceneKClonerModifierTrack::StaticClass();
}

void FKClonerModifierTrackEditor::BuildAddTrackMenu(FMenuBuilder &MenuBuilder) {
  if (!Sequencer.IsValid())
    return;

  // UE 5.7 is inconsistent with how it finds sequences
  // usually GetFocusedMovieSceneSequence works but sometimes it returns null
  // dum api
  UMovieSceneSequence *FocusedSequence =
      Sequencer.Pin()->GetFocusedMovieSceneSequence();
  if (!FocusedSequence)
    return;

  // Get selected objects from editor selection
  // FSelectionIterator requires Engine/Selection.h
  TArray<TWeakObjectPtr<AActor>> SelectedActors;
  for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It) {
    if (AActor *Actor = Cast<AActor>(*It)) {
      SelectedActors.Add(Actor);
    }
  }

  // Find K-Cloner actors in selection
  AKClonerActor *Cloner = nullptr;
  for (TWeakObjectPtr<AActor> ActorPtr : SelectedActors) {
    if (AKClonerActor *C = Cast<AKClonerActor>(ActorPtr.Get())) {
      Cloner = C;
      break;
    }
  }

  if (!Cloner)
    return;

  MenuBuilder.AddSubMenu(
      NSLOCTEXT("KCloner", "AddModifierTrack", "K-Cloner Modifier Track"),
      NSLOCTEXT("KCloner", "AddModifierTrackTooltip",
                "Add keyframe channels for modifier properties"),
      FNewMenuDelegate::CreateLambda([this, Cloner](FMenuBuilder &SubMenu) {
        for (UKClonerModifier *Mod : Cloner->Modifiers) {
          if (!Mod)
            continue;

          FString ModDisplayName = Mod->GetClass()->GetDisplayNameText().ToString();
          FGuid Guid = Mod->ModifierGuid;

          SubMenu.AddSubMenu(
              FText::FromString(ModDisplayName),
              NSLOCTEXT("KCloner", "ModifierProps", "Keyframeable properties"),
              FNewMenuDelegate::CreateLambda([this, Cloner, Guid,
                                              Mod](FMenuBuilder &PropMenu) {
                // EXPANDED FILTER: Collect float, bool, AND vector properties
                TArray<FProperty*> InterpProps;
                for (TFieldIterator<FProperty> It(Mod->GetClass()); It; ++It) {
                  FProperty *Prop = *It;
                  if (!Prop->HasAnyPropertyFlags(CPF_Edit) || !Prop->HasMetaData(TEXT("Interp")))
                    continue;

                  bool bIsFloat = Prop->IsA<FFloatProperty>() || Prop->IsA<FDoubleProperty>();
                  bool bIsBool = Prop->IsA<FBoolProperty>();
                  bool bIsVector = false;

                  if (FStructProperty* StructProp = CastField<FStructProperty>(Prop)) {
                    if (StructProp->Struct->GetFName() == NAME_Vector) {
                      bIsVector = true;
                    }
                  }

                  if (bIsFloat || bIsBool || bIsVector) {
                    InterpProps.Add(Prop);
                  }
                }

                // Add "Key All Interp Props" at the top
                if (InterpProps.Num() > 0) {
                  PropMenu.AddMenuEntry(
                      NSLOCTEXT("KCloner", "KeyAllInterp", "Key All Interp Properties"),
                      NSLOCTEXT("KCloner", "KeyAllInterpTooltip",
                                "Add keyframes for all animatable properties at current time"),
                      FSlateIcon(),
                      FUIAction(FExecuteAction::CreateLambda([this, Cloner, Guid, InterpProps]() {
                        for (FProperty* Prop : InterpProps) {
                          FName PName = Prop->GetFName();
                          // Route to correct handler based on type
                          if (Prop->IsA<FFloatProperty>() || Prop->IsA<FDoubleProperty>()) {
                            AddModifierFloatChannel(Cloner, Guid, PName);
                          } else if (Prop->IsA<FBoolProperty>()) {
                            AddModifierBoolChannel(Cloner, Guid, PName);
                          } else if (FStructProperty* SP = CastField<FStructProperty>(Prop)) {
                            if (SP->Struct->GetFName() == NAME_Vector) {
                              AddModifierVectorChannel(Cloner, Guid, PName);
                            }
                          }
                        }
                      })));
                  PropMenu.AddSeparator();
                }

                // Add individual property entries with correct action dispatcher
                for (FProperty* Prop : InterpProps) {
                  FName PName = Prop->GetFName();
                  FString PropDisplayName = Prop->GetDisplayNameText().ToString();

                  FUIAction Action;
                  FText TypeLabel;

                  if (Prop->IsA<FFloatProperty>() || Prop->IsA<FDoubleProperty>()) {
                    Action = FUIAction(FExecuteAction::CreateRaw(
                        this, &FKClonerModifierTrackEditor::AddModifierFloatChannel,
                        Cloner, Guid, PName));
                    TypeLabel = NSLOCTEXT("KCloner", "FloatType", "(Float)");
                  }
                  else if (Prop->IsA<FBoolProperty>()) {
                    Action = FUIAction(FExecuteAction::CreateRaw(
                        this, &FKClonerModifierTrackEditor::AddModifierBoolChannel,
                        Cloner, Guid, PName));
                    TypeLabel = NSLOCTEXT("KCloner", "BoolType", "(Bool)");
                  }
                  else if (FStructProperty* SP = CastField<FStructProperty>(Prop)) {
                    if (SP->Struct->GetFName() == NAME_Vector) {
                      Action = FUIAction(FExecuteAction::CreateRaw(
                          this, &FKClonerModifierTrackEditor::AddModifierVectorChannel,
                          Cloner, Guid, PName));
                      TypeLabel = NSLOCTEXT("KCloner", "VectorType", "(Vector)");
                    }
                  }

                  PropMenu.AddMenuEntry(
                      FText::Format(NSLOCTEXT("KCloner", "PropWithType", "{0} {1}"),
                                    FText::FromString(PropDisplayName), TypeLabel),
                      FText::Format(NSLOCTEXT("KCloner", "AddChannel", "Add keyframe channel for {0}"),
                                    FText::FromName(PName)),
                      FSlateIcon(),
                      Action);
                }
              }));
        }
      }));
}

TSharedRef<ISequencerSection> FKClonerModifierTrackEditor::MakeSectionInterface(
    UMovieSceneSection &SectionObject, UMovieSceneTrack &Track,
    FGuid ObjectBinding) {
  return MakeShared<FSimpleSection>(SectionObject);
}

void FKClonerModifierTrackEditor::AddModifierFloatChannel(AKClonerActor *Cloner,
                                                          FGuid ModifierGuid,
                                                          FName PropertyName) {
  if (!Sequencer.IsValid() || !Cloner)
    return;

  TSharedPtr<ISequencer> Seq = Sequencer.Pin();
  UMovieSceneSequence *FocusedSequence = Seq->GetFocusedMovieSceneSequence();
  if (!FocusedSequence)
    return;

  UMovieScene *MovieScene = FocusedSequence->GetMovieScene();
  if (!MovieScene)
    return;

  // --- FIX: Use GetHandleToObject first, then fallback to AddActors ---
  FGuid BindingGuid = Seq->GetHandleToObject(Cloner, false);
  
  if (!BindingGuid.IsValid()) {
    // Cloner not yet bound - add it as a possessable
    TArray<TWeakObjectPtr<AActor>> Actors;
    Actors.Add(Cloner);
    TArray<FGuid> Guids = Seq->AddActors(Actors, false);
    if (Guids.Num() > 0) {
      BindingGuid = Guids[0];
    }
  }

  if (!BindingGuid.IsValid()) {
    UE_LOG(LogTemp, Warning,
           TEXT("K-Cloner Sequencer: Failed to acquire object binding for '%s'"),
           *Cloner->GetActorLabel());
    return;
  }

  // undo/redo support - mandatory or users will rage
  FScopedTransaction Transaction(
      NSLOCTEXT("KCloner", "AddModifierKey", "Add K-Cloner Modifier Keyframe"));

  // Find or create track
  UMovieSceneKClonerModifierTrack *KTrack = nullptr;
  for (UMovieSceneTrack *T : MovieScene->FindTracks(
           UMovieSceneKClonerModifierTrack::StaticClass(), BindingGuid)) {
    KTrack = Cast<UMovieSceneKClonerModifierTrack>(T);
    if (KTrack)
      break;
  }

  if (!KTrack) {
    MovieScene->Modify();
    KTrack = MovieScene->AddTrack<UMovieSceneKClonerModifierTrack>(BindingGuid);
  }

  if (!KTrack)
    return;

  KTrack->Modify();

  // Find or create section
  UMovieSceneKClonerModifierSection *Section = nullptr;
  for (UMovieSceneSection *S : KTrack->GetAllSections()) {
    Section = Cast<UMovieSceneKClonerModifierSection>(S);
    if (Section)
      break;
  }

  if (!Section) {
    Section =
        Cast<UMovieSceneKClonerModifierSection>(KTrack->CreateNewSection());
    if (Section) {
      Section->SetFlags(RF_Transactional);
      KTrack->AddSection(*Section);
      Section->SetRange(TRange<FFrameNumber>::All());
    }
  }

  if (!Section)
    return;

  Section->Modify();

  // Check if channel already exists for this modifier+property
  int32 ExistingIndex = Section->FloatParams.IndexOfByPredicate(
      [&](const FKClonerFloatParam& P) {
        return P.ModifierGuid == ModifierGuid && P.PropertyName == PropertyName;
      });

  // Find the modifier
  UKClonerModifier *Mod = nullptr;
  for (UKClonerModifier *M : Cloner->Modifiers) {
    if (M && M->ModifierGuid == ModifierGuid) {
      Mod = M;
      break;
    }
  }

  // Get current property value
  float CurrentValue = 0.f;
  if (Mod) {
    if (FFloatProperty *FP = CastField<FFloatProperty>(
            Mod->GetClass()->FindPropertyByName(PropertyName))) {
      CurrentValue = FP->GetFloatingPointPropertyValue(
          FP->ContainerPtrToValuePtr<void>(Mod));
    } else if (FDoubleProperty *DP = CastField<FDoubleProperty>(
            Mod->GetClass()->FindPropertyByName(PropertyName))) {
      CurrentValue = (float)DP->GetFloatingPointPropertyValue(
          DP->ContainerPtrToValuePtr<void>(Mod));
    }
  }

  FFrameRate Rate = MovieScene->GetTickResolution();
  FFrameNumber Time = Seq->GetLocalTime().ConvertTo(Rate).FloorToFrame();

  if (ExistingIndex != INDEX_NONE) {
    // Channel already exists - just add a key at current time
    TArray<FFrameNumber> Times;
    TArray<FMovieSceneFloatValue> Values;
    Times.Add(Time);
    Values.Add(FMovieSceneFloatValue(CurrentValue));
    Section->FloatParams[ExistingIndex].Channel.AddKeys(Times, Values);

    Seq->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
    UE_LOG(LogTemp, Log, TEXT("K-Cloner: Added key to existing channel %s.%s at frame %d (value: %f) - easy update"),
           Mod ? *Mod->GetClass()->GetName() : TEXT("Unknown"), *PropertyName.ToString(), Time.Value, CurrentValue);
    return;
  }

  // Create new parameter
  FKClonerFloatParam Param;
  Param.ModifierGuid = ModifierGuid;
  Param.PropertyName = PropertyName;
  Param.Channel.SetDefault(CurrentValue);

  // Add initial key
  TArray<FFrameNumber> Times;
  TArray<FMovieSceneFloatValue> Values;
  Times.Add(Time);
  Values.Add(FMovieSceneFloatValue(CurrentValue));
  Param.Channel.AddKeys(Times, Values);

  Section->FloatParams.Add(Param);

  // Notify Sequencer of data change
  Seq->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

  UE_LOG(LogTemp, Log, TEXT("K-Cloner: Created channel for %s.%s at frame %d (value: %f)"),
         Mod ? *Mod->GetClass()->GetName() : TEXT("Unknown"), *PropertyName.ToString(), Time.Value, CurrentValue);
}

// ============================================================================
// BOOL CHANNEL - for toggling stuff on/off over time
// ============================================================================
void FKClonerModifierTrackEditor::AddModifierBoolChannel(AKClonerActor *Cloner,
                                                          FGuid ModifierGuid,
                                                          FName PropertyName) {
  if (!Sequencer.IsValid() || !Cloner)
    return;

  TSharedPtr<ISequencer> Seq = Sequencer.Pin();
  UMovieSceneSequence *FocusedSequence = Seq->GetFocusedMovieSceneSequence();
  if (!FocusedSequence)
    return;

  UMovieScene *MovieScene = FocusedSequence->GetMovieScene();
  if (!MovieScene)
    return;

  FGuid BindingGuid = Seq->GetHandleToObject(Cloner, false);
  if (!BindingGuid.IsValid()) {
    TArray<TWeakObjectPtr<AActor>> Actors;
    Actors.Add(Cloner);
    TArray<FGuid> Guids = Seq->AddActors(Actors, false);
    if (Guids.Num() > 0) {
      BindingGuid = Guids[0];
    }
  }

  if (!BindingGuid.IsValid())
    return;

  FScopedTransaction Transaction(
      NSLOCTEXT("KCloner", "AddBoolKey", "Add K-Cloner Bool Keyframe"));

  // Find or create track
  UMovieSceneKClonerModifierTrack *KTrack = nullptr;
  for (UMovieSceneTrack *T : MovieScene->FindTracks(
           UMovieSceneKClonerModifierTrack::StaticClass(), BindingGuid)) {
    KTrack = Cast<UMovieSceneKClonerModifierTrack>(T);
    if (KTrack)
      break;
  }

  if (!KTrack) {
    MovieScene->Modify();
    KTrack = MovieScene->AddTrack<UMovieSceneKClonerModifierTrack>(BindingGuid);
  }

  if (!KTrack)
    return;

  KTrack->Modify();

  // Find or create section
  UMovieSceneKClonerModifierSection *Section = nullptr;
  for (UMovieSceneSection *S : KTrack->GetAllSections()) {
    Section = Cast<UMovieSceneKClonerModifierSection>(S);
    if (Section)
      break;
  }

  if (!Section) {
    Section = Cast<UMovieSceneKClonerModifierSection>(KTrack->CreateNewSection());
    if (Section) {
      Section->SetFlags(RF_Transactional);
      KTrack->AddSection(*Section);
      Section->SetRange(TRange<FFrameNumber>::All());
    }
  }

  if (!Section)
    return;

  Section->Modify();

  // Find the modifier
  UKClonerModifier *Mod = nullptr;
  for (UKClonerModifier *M : Cloner->Modifiers) {
    if (M && M->ModifierGuid == ModifierGuid) {
      Mod = M;
      break;
    }
  }

  // Get current value
  bool bCurrentValue = false;
  if (Mod) {
    if (FBoolProperty *BP = CastField<FBoolProperty>(
            Mod->GetClass()->FindPropertyByName(PropertyName))) {
      bCurrentValue = BP->GetPropertyValue_InContainer(Mod);
    }
  }

  // Check existence
  int32 ExistingIndex = Section->BoolParams.IndexOfByPredicate(
      [&](const FKClonerBoolParam& P) {
        return P.ModifierGuid == ModifierGuid && P.PropertyName == PropertyName;
      });

  FFrameRate Rate = MovieScene->GetTickResolution();
  FFrameNumber Time = Seq->GetLocalTime().ConvertTo(Rate).FloorToFrame();
  TArray<FFrameNumber> Times = { Time };
  TArray<bool> Values = { bCurrentValue };

  if (ExistingIndex != INDEX_NONE) {
    Section->BoolParams[ExistingIndex].Channel.AddKeys(Times, Values);
    Seq->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
    UE_LOG(LogTemp, Log, TEXT("K-Cloner: Added bool key to %s.%s at frame %d (value: %s)"),
           Mod ? *Mod->GetClass()->GetName() : TEXT("Unknown"), *PropertyName.ToString(), 
           Time.Value, bCurrentValue ? TEXT("true") : TEXT("false"));
    return;
  }

  // Create new param
  FKClonerBoolParam Param;
  Param.ModifierGuid = ModifierGuid;
  Param.PropertyName = PropertyName;
  Param.Channel.SetDefault(bCurrentValue);
  Param.Channel.AddKeys(Times, Values);

  Section->BoolParams.Add(Param);
  Seq->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

  UE_LOG(LogTemp, Log, TEXT("K-Cloner: Created bool channel for %s.%s (value: %s)"),
         Mod ? *Mod->GetClass()->GetName() : TEXT("Unknown"), *PropertyName.ToString(),
         bCurrentValue ? TEXT("true") : TEXT("false"));
}

// ============================================================================
// VECTOR CHANNEL - for animating position, direction, scale, etc.
// Uses 3 float channels (X, Y, Z)
// ============================================================================
void FKClonerModifierTrackEditor::AddModifierVectorChannel(AKClonerActor *Cloner,
                                                            FGuid ModifierGuid,
                                                            FName PropertyName) {
  if (!Sequencer.IsValid() || !Cloner)
    return;

  TSharedPtr<ISequencer> Seq = Sequencer.Pin();
  UMovieSceneSequence *FocusedSequence = Seq->GetFocusedMovieSceneSequence();
  if (!FocusedSequence)
    return;

  UMovieScene *MovieScene = FocusedSequence->GetMovieScene();
  if (!MovieScene)
    return;

  FGuid BindingGuid = Seq->GetHandleToObject(Cloner, false);
  if (!BindingGuid.IsValid()) {
    TArray<TWeakObjectPtr<AActor>> Actors;
    Actors.Add(Cloner);
    TArray<FGuid> Guids = Seq->AddActors(Actors, false);
    if (Guids.Num() > 0) {
      BindingGuid = Guids[0];
    }
  }

  if (!BindingGuid.IsValid())
    return;

  FScopedTransaction Transaction(
      NSLOCTEXT("KCloner", "AddVectorKey", "Add K-Cloner Vector Keyframe"));

  // Find or create track
  UMovieSceneKClonerModifierTrack *KTrack = nullptr;
  for (UMovieSceneTrack *T : MovieScene->FindTracks(
           UMovieSceneKClonerModifierTrack::StaticClass(), BindingGuid)) {
    KTrack = Cast<UMovieSceneKClonerModifierTrack>(T);
    if (KTrack)
      break;
  }

  if (!KTrack) {
    MovieScene->Modify();
    KTrack = MovieScene->AddTrack<UMovieSceneKClonerModifierTrack>(BindingGuid);
  }

  if (!KTrack)
    return;

  KTrack->Modify();

  // Find or create section
  UMovieSceneKClonerModifierSection *Section = nullptr;
  for (UMovieSceneSection *S : KTrack->GetAllSections()) {
    Section = Cast<UMovieSceneKClonerModifierSection>(S);
    if (Section)
      break;
  }

  if (!Section) {
    Section = Cast<UMovieSceneKClonerModifierSection>(KTrack->CreateNewSection());
    if (Section) {
      Section->SetFlags(RF_Transactional);
      KTrack->AddSection(*Section);
      Section->SetRange(TRange<FFrameNumber>::All());
    }
  }

  if (!Section)
    return;

  Section->Modify();

  // Find the modifier
  UKClonerModifier *Mod = nullptr;
  for (UKClonerModifier *M : Cloner->Modifiers) {
    if (M && M->ModifierGuid == ModifierGuid) {
      Mod = M;
      break;
    }
  }

  // Get current vector value
  FVector CurrentVec = FVector::ZeroVector;
  if (Mod) {
    if (FStructProperty *SP = CastField<FStructProperty>(
            Mod->GetClass()->FindPropertyByName(PropertyName))) {
      if (SP->Struct->GetFName() == NAME_Vector) {
        // Get the value - need to read from the container
        const void* ValuePtr = SP->ContainerPtrToValuePtr<void>(Mod);
        CurrentVec = *static_cast<const FVector*>(ValuePtr);
      }
    }
  }

  // Check existence
  int32 ExistingIndex = Section->VectorParams.IndexOfByPredicate(
      [&](const FKClonerVectorParam& P) {
        return P.ModifierGuid == ModifierGuid && P.PropertyName == PropertyName;
      });

  FFrameRate Rate = MovieScene->GetTickResolution();
  FFrameNumber Time = Seq->GetLocalTime().ConvertTo(Rate).FloorToFrame();
  TArray<FFrameNumber> Times = { Time };
  TArray<FMovieSceneFloatValue> XVal = { FMovieSceneFloatValue((float)CurrentVec.X) };
  TArray<FMovieSceneFloatValue> YVal = { FMovieSceneFloatValue((float)CurrentVec.Y) };
  TArray<FMovieSceneFloatValue> ZVal = { FMovieSceneFloatValue((float)CurrentVec.Z) };

  if (ExistingIndex != INDEX_NONE) {
    Section->VectorParams[ExistingIndex].X.AddKeys(Times, XVal);
    Section->VectorParams[ExistingIndex].Y.AddKeys(Times, YVal);
    Section->VectorParams[ExistingIndex].Z.AddKeys(Times, ZVal);
    Seq->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
    UE_LOG(LogTemp, Log, TEXT("K-Cloner: Added vector key to %s.%s at frame %d (%.2f, %.2f, %.2f)"),
           Mod ? *Mod->GetClass()->GetName() : TEXT("Unknown"), *PropertyName.ToString(),
           Time.Value, CurrentVec.X, CurrentVec.Y, CurrentVec.Z);
    return;
  }

  // Create new param with 3 channels
  FKClonerVectorParam Param;
  Param.ModifierGuid = ModifierGuid;
  Param.PropertyName = PropertyName;

  Param.X.SetDefault((float)CurrentVec.X);
  Param.Y.SetDefault((float)CurrentVec.Y);
  Param.Z.SetDefault((float)CurrentVec.Z);

  Param.X.AddKeys(Times, XVal);
  Param.Y.AddKeys(Times, YVal);
  Param.Z.AddKeys(Times, ZVal);

  Section->VectorParams.Add(Param);
  Seq->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

  UE_LOG(LogTemp, Log, TEXT("K-Cloner: Created vector channel for %s.%s (%.2f, %.2f, %.2f)"),
         Mod ? *Mod->GetClass()->GetName() : TEXT("Unknown"), *PropertyName.ToString(),
         CurrentVec.X, CurrentVec.Y, CurrentVec.Z);
}
