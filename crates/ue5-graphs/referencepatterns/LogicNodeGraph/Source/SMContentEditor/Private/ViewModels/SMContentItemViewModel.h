// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Assets/SMContentAsset.h"

#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"

#include "SMContentItemViewModel.generated.h"

class UInputMappingContext;

DECLARE_DELEGATE_OneParam(FOnGetSelectedAssetsFromPicker, TArray<FAssetData>& /* OutSelectedAssets */);
DECLARE_DELEGATE(FOnSelectionConfirmed);

UCLASS()
class USMContentProjectConfigurationViewModel : public UObject
{
	GENERATED_BODY()

public:
	USMContentProjectConfigurationViewModel();

	/**
	 * The default enhanced input action mapping to be applied only within the context of this content.
	 * This can be changed after installing the content by adjusting the input mapping context asset.
	 */
	UPROPERTY(VisibleAnywhere, Category = Input)
	TMap<FName, FSMInputActionWrapper> ActionMapping;

	/** The files contained in this content which will be installed to the project. */
	UPROPERTY(VisibleAnywhere, Category = Content)
	int32 FileCount;

	/** If the user has modified the input settings. Currently unsupported! */
	bool WasInputModified() const { return bInputModified; }

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	bool bInputModified = false;
};

class FSMContentItemViewModel
{
public:
	FText Name;
	FText Author;
	FText Description;
	FText DetailedDescription;
	FText ContentType;
	FOnGetSelectedAssetsFromPicker OnGetSelectedAssetsFromPicker;
	FOnSelectionConfirmed OnSelectionConfirmed;

	FString InstallPath;
	FString DestinationFolder;

	bool bOpenMapOnInstall = true;
	
	FSMContentItemViewModel() {}
	~FSMContentItemViewModel() { ProjectConfigurationViewModel.Reset(); }

	explicit FSMContentItemViewModel(USMContentAsset* InContentAsset);

	TWeakObjectPtr<USMContentAsset> GetContentAsset() const { return ContentAsset; }
	USMContentProjectConfigurationViewModel* GetProjectConfigurationViewModel() const { return ProjectConfigurationViewModel.Get(); }
	const FSlateBrush* GetTitleBrush() const;
	const FSlateBrush* GetDescriptionBrush() const;
	FLinearColor GetTypeColor() const;

	bool IsInstalledToPluginContent() const;
	
private:
	TWeakObjectPtr<USMContentAsset> ContentAsset;
	TStrongObjectPtr<USMContentProjectConfigurationViewModel> ProjectConfigurationViewModel;
	mutable FSlateBrush TitleBrush;
	mutable FSlateBrush DescriptionBrush;
};