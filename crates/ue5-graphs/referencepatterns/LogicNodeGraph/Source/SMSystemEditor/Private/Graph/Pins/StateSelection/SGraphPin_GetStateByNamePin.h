// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SSMStateTreeView.h"

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "SGraphPin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SMenuAnchor.h"

class SButton;
class USMBlueprintGeneratedClass;

struct FSMGetStateByNamePinFactory : FGraphPanelPinFactory
{
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* InPin) const override;
	static void RegisterFactory();
};

class SGraphPin_GetStateByNamePin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_GetStateByNamePin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	static USMBlueprintGeneratedClass* GetBlueprintGeneratedClass(const UEdGraphPin* InGraphPin);
	
protected:
	// SGraphPin
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;
	virtual bool DoesWidgetHandleSettingEditingEnabled() const override { return true; }
	// ~SGraphPin

	/** The primary content of the drop down. */
	TSharedRef<SWidget> OnGetMenuContent();
	
	/** Get default text for the picker combo */
	FText OnGetDefaultComboText() const;
	
	/** Combo Button Color and Opacity delegate */
	FSlateColor OnGetComboForeground() const;
	
	/** Button Color and Opacity delegate */
	FSlateColor OnGetWidgetBackground() const;
	
	/** If widget is displayed. */
	EVisibility OnGetWidgetVisibility() const;

	/** User selected a state. */
	void OnStateSelected(FSMStateTreeItemPtr SelectedState);

	/**
	* Closes the combo button for the asset name.
	*/
	void CloseComboButton();

private:
	/** Object manipulator buttons. */
	TSharedPtr<SButton> BrowseButton;

	/** Menu anchor for opening and closing the asset picker */
	TSharedPtr<SMenuAnchor> AssetPickerAnchor;

	/** Cached AssetData of object selected */
	mutable FAssetData CachedAssetData;

	// Class of the Soft Reference.
	//
	// Guaranteed to be subclass of AActor.
	UClass* PinObjectClass = nullptr;

	TSharedPtr<SSMStateTreeSelectionView> StateTreeView;
};
