// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "SMRuntimeSettings.generated.h"

/**
 * Logic Driver settings for runtime.
 */
UCLASS(Config=Engine, defaultconfig)
class SMSYSTEM_API USMRuntimeSettings : public UObject
{
	GENERATED_BODY()

public:
	USMRuntimeSettings();
	
	/**
	 * Optimize when default node instances are loaded. They are often not needed
	 * unless programatically accessing the node. Leaving off can help memory usage
	 * and initialization times.
	 * 
	 * True - Loads all default node instances when initializing a state machine.
	 * False - Only load default node instances on demand.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Performance")
	bool bPreloadDefaultNodes;
};
