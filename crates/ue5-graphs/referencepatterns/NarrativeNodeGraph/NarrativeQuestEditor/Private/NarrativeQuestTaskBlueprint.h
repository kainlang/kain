//  Copyright Narrative Tools 2022.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "NarrativeQuestTaskBlueprint.generated.h"

/**
 * Tasks are blueprints that manage a task for the player to complete. This is mostly used by the quest editor.
 * This is different from Data Tasks which are just data assets: a TaskName and Argument, ie "FindItem:Sword",
 *
 * A good example is the GoToLocation task that comes with narrative, which checks if the player has reached a goal location. This type
 * of task wouldn't be possible with a data task, which doesn't have any logic or variables.
 */
UCLASS()
class UNarrativeTaskBlueprint : public UBlueprint
{
	GENERATED_BODY()
	
};
