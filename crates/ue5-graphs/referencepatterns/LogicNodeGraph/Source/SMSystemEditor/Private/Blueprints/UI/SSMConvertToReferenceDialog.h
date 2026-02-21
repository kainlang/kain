// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"

class USMBlueprintFactory;
class USMBlueprint;

/**
 * Shown when converting a state machine to a reference in-place.
 */
class SSMConvertToReferenceDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSMConvertToReferenceDialog)
	{
	}
	/** The owning sm instance class of original state machine node. */
	SLATE_ARGUMENT(UClass*, OwningInstanceClass)
	/** The default asset name to use when opening the save dialog. */
	SLATE_ARGUMENT(FString, AssetName)
	/** The default asset path to use when opening the save as dialog. */
	SLATE_ARGUMENT(FString, AssetPath)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USMBlueprintFactory* InFactory);

	/** Retrieve the newly created blueprint. May be nullptr. */
	USMBlueprint* GetNewlyCreatedBlueprint() const { return NewBlueprint; }
	
private:
	void CreateContent();
	void OnSelectedClassChanged(UClass* InNewClass);

	FReply OnSaveAsClicked();
	
private:
	UClass* OriginalParentClass = nullptr;
	UClass* SelectedParentClass = nullptr;

	USMBlueprint* NewBlueprint = nullptr;
	USMBlueprintFactory* Factory = nullptr;
	FString AssetName;
	FString AssetPath;
};