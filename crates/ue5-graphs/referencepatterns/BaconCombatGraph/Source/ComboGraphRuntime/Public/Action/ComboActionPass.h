// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "ComboActionPass.generated.h"

class UComboGraphInstance;

UCLASS(Abstract, Within = ComboGraphNodeData, DefaultToInstanced, Blueprintable, EditInlineNew, CollapseCategories)
class COMBOGRAPHRUNTIME_API UComboActionPass : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Implement this to display the preview assets on the node thumbnail
	 * @return The list of asset that will be displayed on the node thumbnail
	 */
	virtual TArray<FSoftObjectPath> GetPreviewAssets() const;

	/**
	 * Implement this on blueprint override custom node thumbnail
	 * @return The list of asset that will be displayed on the node thumbnail
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action Pass")
	TArray<FSoftObjectPath> K2_GetPreviewAssets() const;
	
public:
	/**
	 * Execute the action pass
	 * @param GraphInstance The graph instance that is executing the action pass
	 * @return True if the action pass was executed successfully, false otherwise
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Action Pass")
	bool ExecutePass(UComboGraphInstance* GraphInstance) const;

	/**
	 * Display the pass information on the node
	 * @return String that will be displayed on the node
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Action Pass")
	FString GetPassInformation() const; 
};
