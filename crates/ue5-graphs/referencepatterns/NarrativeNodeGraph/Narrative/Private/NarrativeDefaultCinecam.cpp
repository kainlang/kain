// Fill out your copyright notice in the Description page of Project Settings.


#include "NarrativeDefaultCinecam.h"
#include "CineCameraComponent.h"

ANarrativeDefaultCinecam::ANarrativeDefaultCinecam(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
	//Set some good defaults for a cinematic camera to use in dialogue 
	if (UCineCameraComponent* CineCamComp = GetCineCameraComponent())
	{
		CineCamComp->bConstrainAspectRatio = false;
		CineCamComp->FocusSettings.ManualFocusDistance = 300.f;
		CineCamComp->CurrentFocalLength = 25.f;
		CineCamComp->CurrentAperture = 1.2f;
	}


}
