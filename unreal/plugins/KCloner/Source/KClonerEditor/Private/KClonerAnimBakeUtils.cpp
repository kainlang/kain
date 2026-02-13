// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerAnimBakeUtils.cpp

#include "KClonerAnimBakeUtils.h"
#include "KClonerActor.h"
#include "KClonerModifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AssetToolsModule.h"
#include "Factories/AnimSequenceFactory.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "KClonerAnimBakeUtils"

UAnimSequence* UKClonerAnimBakeUtils::BakeAnimSequence(AKClonerActor* ClonerActor, UAnimSequence* SourceAnim, FString OutputPath, FString OutputName)
{
	if (!ClonerActor || !SourceAnim)
	{
		UE_LOG(LogTemp, Error, TEXT("KClonerAnimBakeUtils: Invalid Cloner or Source Anim"));
		return nullptr;
	}

	// Ensure output path is valid
	if (OutputPath.IsEmpty()) OutputPath = TEXT("/Game");
	if (OutputName.IsEmpty()) OutputName = TEXT("NewAnim");

	FString PackageName = OutputPath / OutputName;
	UPackage* Package = CreatePackage(*PackageName);
	
	// copy the source anim so we don't mess up the original
	UAnimSequence* NewAnim = DuplicateObject<UAnimSequence>(SourceAnim, Package, *OutputName);
	if (!NewAnim)
	{
		UE_LOG(LogTemp, Error, TEXT("KClonerAnimBakeUtils: Failed to duplicate animation"));
		return nullptr;
	}

	// Apply modifications
	ApplyModifiersToRootTrack(NewAnim, ClonerActor);

	// Mark dirty and notify
	NewAnim->MarkPackageDirty();
	NewAnim->PostEditChange();

	// we'll let the user save it manually if they want
	// returning it is enough for the editor to show it in content browser 
	// (usually lol)

	UE_LOG(LogTemp, Log, TEXT("KClonerAnimBakeUtils: Baked new animation to %s"), *PackageName);
	return NewAnim;
}

UAnimSequence* UKClonerAnimBakeUtils::BakeAnimSequenceFromData(UKClonerData* Data, UAnimSequence* OverrideAnim)
{
	if (!Data) return nullptr;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("KClonerAnimBakeUtils: No Editor World Context"));
		return nullptr;
	}

	// spawn a hidden actor to do the baking math for us
	FActorSpawnParameters SpawnParams;
	AKClonerActor* TempActor = World->SpawnActor<AKClonerActor>(AKClonerActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TempActor) return nullptr;

	TempActor->SetFlags(RF_Transient); // Mark as transient

	TempActor->ApplyPreset(Data);
	TempActor->bAnimTweakMode = true; // Enforce Tweak Mode

	UAnimSequence* Source = OverrideAnim ? OverrideAnim : Data->SourceAnimSequence;
	if (!Source) Source = TempActor->SourceAnimSequence;

	FString OutPath = Data->OutputFolder.Path;
	if (OutPath.IsEmpty()) OutPath = TEXT("/Game");

	UAnimSequence* Result = BakeAnimSequence(TempActor, Source, OutPath, Data->OutputName);

	World->DestroyActor(TempActor);
	return Result;
}

void UKClonerAnimBakeUtils::ApplyModifiersToRootTrack(UAnimSequence* TargetAnim, AKClonerActor* ClonerActor)
{
	if (!TargetAnim || !ClonerActor) return;

	IAnimationDataController& Controller = TargetAnim->GetController();
	IAnimationDataModel* Model = TargetAnim->GetDataModel();
	if (!Model) return;

	// Use Reference Skeleton to find Root Bone
	const FReferenceSkeleton& RefSkeleton = TargetAnim->GetSkeleton()->GetReferenceSkeleton();
	if (RefSkeleton.GetNum() == 0) return;

	// Assume Root is index 0
	FName RootBoneName = RefSkeleton.GetBoneName(0);
	
	// undo/redo bracket so we don't spam the history
	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("BakeModifiers", "Bake K-Cloner Modifiers"));

	// Get existing keys
	TArray<FTransform> BoneTransforms;
	Model->GetBoneTrackTransforms(RootBoneName, BoneTransforms);

	int32 NumKeys = Model->GetNumberOfKeys();
	double FrameRate = Model->GetFrameRate().AsDecimal();
	double PlayLength = Model->GetPlayLength();

	// If no keys exist on root, generate them (identity)
	if (BoneTransforms.Num() != NumKeys)
	{
		BoneTransforms.SetNum(NumKeys);
		for (int32 i = 0; i < NumKeys; i++)
		{
			BoneTransforms[i] = FTransform::Identity;
		}
	}

	// Prepare output arrays
	TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
	TArray<FVector> ScaleKeys;
	PosKeys.SetNum(NumKeys);
	RotKeys.SetNum(NumKeys);
	ScaleKeys.SetNum(NumKeys);

	// loop through every single frame and apply the modifier stack
	for (int32 i = 0; i < NumKeys; i++)
	{
		double Time = Model->GetFrameRate().AsSeconds(i);
		float FloatTime = (float)Time;

		// Get original transform
		FTransform OriginalTransform = BoneTransforms[i];

		// Apply K-Cloner Modifiers
		// for anim sequence baking there's only one "instance"
		// which is the root bone itself
		int32 Index = 0;
		int32 Count = 1;
		
		// Setup custom data (empty for now)
		TArray<float> CustomData;

		// Iterate modifiers
		for (UKClonerModifier* Mod : ClonerActor->Modifiers)
		{
			if (Mod && Mod->bEnabled)
			{
				// ApplyModifier handles Influence and other base checks
				Mod->ApplyModifier(OriginalTransform, Index, Count, FloatTime, CustomData);
			}
		}

		// Store result
		PosKeys[i] = OriginalTransform.GetLocation();
		RotKeys[i] = OriginalTransform.GetRotation();
		ScaleKeys[i] = OriginalTransform.GetScale3D();
	}

	// Write back to track
	Controller.SetBoneTrackKeys(RootBoneName, PosKeys, RotKeys, ScaleKeys);
}

#undef LOCTEXT_NAMESPACE
