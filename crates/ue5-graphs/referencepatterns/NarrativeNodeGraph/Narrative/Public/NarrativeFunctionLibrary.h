// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NarrativeFunctionLibrary.generated.h"

/**
 * General functions used by narrative 
 */
UCLASS()
class NARRATIVE_API UNarrativeFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	/**
	* Grab the narrative component from the local pawn or player controller, whichever it exists on. 
	* 
	* @return The narrative component.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static class UNarrativeComponent* GetNarrativeComponent(const UObject* WorldContextObject);

	/**
	* Find the narrative component from the supplied target object. 
	*
	* @return The narrative component.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (DefaultToSelf = "Target"))
	static class UNarrativeComponent* GetNarrativeComponentFromTarget(AActor* Target);

	/**
	* Calls CompleteNarrativeTask on the narrative component
	*
	* @return Whether the task updated a quest 
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative", meta = (DisplayName = "Complete Narrative Data Task", BlueprintInternalUseOnly = "true"))
	static bool CompleteNarrativeDataTask(class UNarrativeComponent* Target, const class UNarrativeDataTask* Task, const FString& Argument, const int32 Quantity = 1);

	/**
	Use this when you want to log a data task, but don't need a data task asset. For example if you tracked player finding items you'd create a "FindItem" data task asset,
	but sometimes you just want to track something super random and creating a whole task asset is overkill and just storing the argument is good enough */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative", meta = (DisplayName = "Complete Loose Narrative Data Task"))
	static bool CompleteLooseNarrativeDataTask(class UNarrativeComponent* Target, const FString & Argument, const int32 Quantity = 1);

	//Grab a narrative task by its name. Try use asset references instead of this if possible, since an task being renamed will break your code
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static class UNarrativeDataTask* GetTaskByName(const UObject* WorldContextObject, const FString& EventName);

	//Just used by narrative UI, BP exposed FName::NameToDisplayString
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative")
	static FString MakeDisplayString(const FString& String);

};
