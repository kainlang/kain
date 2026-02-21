// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "SGraphPin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SButton;

struct FSMActorSoftReferencePinFactory : FGraphPanelPinFactory
{
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* InPin) const override;
	static void RegisterFactory();
};

class SGraphPin_ActorSoftReferencePin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_ActorSoftReferencePin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	// SGraphPin
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;
	virtual bool DoesWidgetHandleSettingEditingEnabled() const override { return true; }
	// ~SGraphPin

	/** Get default text for the picker combo */
	virtual FText GetDefaultComboText() const;

	TSharedRef<SWidget> OnGetMenuContent();

	/** Get text tooltip for object */
	FText GetObjectToolTip() const;
	/** Get string value for object */
	FText GetValue() const;

	/** Used to update the combo button text */
	FText OnGetComboTextValue() const;
	/** Combo Button Color and Opacity delegate */
	FSlateColor OnGetComboForeground() const;
	/** Button Color and Opacity delegate */
	FSlateColor OnGetWidgetBackground() const;

	/**
	* Use the selected object (replaces the referenced object if valid)
	*/
	void OnUse();

	/**
	* Returns whether the actor should be filtered out from selection.
	*/
	bool IsFilteredActor(const AActor* const Actor) const;

	/**
	* Closes the combo button for the asset name.
	*/
	void CloseComboButton();

	/**
	* Delegate for handling classes of objects that can be picked.
	* @param	AllowedClasses	The array of classes we allow
	*/
	void OnGetAllowedClasses(TArray<const UClass*>& AllowedClasses);

	/**
	* Delegate for handling selection in the scene outliner.
	* @param	InActor	The chosen actor
	*/
	void OnActorSelected(AActor* InActor);

	/**
	 * When the magnifier is selected.
	 */
	void OnBrowseToSelected();

	/** Returns asset data of currently selected object, if bRuntimePath is true
	* this will include _C for blueprint classes, for false it will point to
	* UBlueprint instead */
	virtual const FAssetData& GetAssetData(bool bRuntimePath) const;

	/** Return the actor object from the world. */
	AActor* GetActorFromAssetData() const;
	
private:
	/** Object manipulator buttons. */
	TSharedPtr<SButton> BrowseButton;

	/** Menu anchor for opening and closing the asset picker */
	TSharedPtr<class SMenuAnchor> AssetPickerAnchor;

	/** Cached AssetData of object selected */
	mutable FAssetData CachedAssetData;

	// Class of the Soft Reference.
	//
	// Guaranteed to be subclass of AActor.
	UClass* PinObjectClass = nullptr;
};
