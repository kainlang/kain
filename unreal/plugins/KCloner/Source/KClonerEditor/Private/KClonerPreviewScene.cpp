// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerPreviewScene.h"
#include "KClonerActor.h"
#include "KClonerData.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

FKClonerPreviewScene::FKClonerPreviewScene(ConstructionValues CVS)
	: FAdvancedPreviewScene(CVS)
	, ClonerActor(nullptr)
{
	// Disable default floor mesh, we just want the grid
	SetFloorVisibility(false, false);
}

FKClonerPreviewScene::~FKClonerPreviewScene()
{
}

void FKClonerPreviewScene::Tick(float InDeltaTime)
{
	FAdvancedPreviewScene::Tick(InDeltaTime);
	
	if (bIsPlaying)
	{
		CurrentTime += InDeltaTime;
		// Loop for demo purposes
		if (CurrentTime > 10.0f)
		{
			CurrentTime = 0.0f;
		}
	}

	if (ClonerActor)
	{
		ClonerActor->bUseOverrideTime = true;
		ClonerActor->OverrideTime = CurrentTime;
		// Actor will be ticked by the world automatically via FKClonerViewportClient::Tick -> World->Tick
	}
}

void FKClonerPreviewScene::SetCurrentTime(float InTime)
{
	CurrentTime = InTime;
}

void FKClonerPreviewScene::SetClonerData(UKClonerData* InData)
{
	if (!InData) return;

	if (ClonerActor)
	{
		GetWorld()->DestroyActor(ClonerActor);
		ClonerActor = nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ClonerActor = GetWorld()->SpawnActor<AKClonerActor>(AKClonerActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (ClonerActor)
	{
		// Apply preset to actor (handles all fields including skeletal mesh)
		ClonerActor->ApplyPreset(InData);
		
		// Force rebuild
		ClonerActor->ForceRebuild();
	}
}
