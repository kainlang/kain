// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NarrativeTaskManager.generated.h"

/**
 * Manages a map of UNarrativeTasks. Makes it really easy to grab a task asset using its string Name at runtime. 
 */
UCLASS()
class UNarrativeTaskManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
protected:

	UNarrativeTaskManager();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void CacheNarrativeTasks();

	//Allows for efficiently grabbing a narrative event asset by its name 
	UPROPERTY()
	TMap<FString, class UNarrativeDataTask*> NarrativeTaskMap;

public:

	class UNarrativeDataTask* GetTask(const FString& EventName) const;

};
