// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "KismetPins/SGraphPinObject.h"

class FAssetThumbnail;
class SButton;
class USMGraphK2Node_PropertyNode_Base;

struct FSMObjectPinFactory : FGraphPanelPinFactory
{
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* InPin) const override;
	static void RegisterFactory();
};

/**
 * Logic Driver pin override for object type pins allowing a thumbnail to be displayed in blueprints
 * and increased asset text width.
 */
class SGraphPin_ObjectPin : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_ObjectPin) {}
		/** Optional meta class for the pin */
		SLATE_ARGUMENT(const UClass*, MetaClass)
	SLATE_END_ARGS()

	SGraphPin_ObjectPin() : bCanDisplayThumbnail(true) {}
	
	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
	
protected:
	// SGraphPin
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;
	virtual void OnAssetSelectedFromPicker(const FAssetData& AssetData) override;
	virtual FReply OnClickUse() override;
	// ~SGraphPin

	/** True if the property and project is configured to display thumbnails. */
	bool CanDisplayThumbnail() const { return bCanDisplayThumbnail; }

	/** Create a thumbnail widget for our asset. */
	virtual TSharedPtr<SWidget> CreateThumbnailWidget();

	/** Creates an overlay for our thumbnail. */
	virtual TSharedPtr<SWidget> CreateThumbnailOverlayWidget() { return nullptr; }
	
	/** The tooltip to use for the thumbnail. */
	virtual TSharedPtr<SToolTip> CreateThumbnailTooltipWidget();

	/** Creates buttons in a horizontal box which will always be used regardless of thumbnail setting. */
	virtual TSharedPtr<SHorizontalBox> CreateButtonsWidget();
	
private:
	/** Our customized selection combo box/button for use with the thumbnail. */
	TSharedPtr<SWidget> CreateVerticalSelectionWidget();

	/** Update the thumbnail with our selected asset. */
	void UpdateThumbnail();

protected:
	/** Our owning graph property node. */
	TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base> OwningPropertyNode;

	/** The meta class of our asset. */
	const UClass* MetaClass = nullptr;
	
private:
	/** The main thumbnail. */
	TSharedPtr<FAssetThumbnail> Thumbnail;
	
	/** The thumbnail used in a tooltip. */
	TSharedPtr<FAssetThumbnail> ThumbnailTooltip;
	
	/** The buttons displayed next to the thumbnail. */
	TSharedPtr<SHorizontalBox> Buttons;

	/** If the widget is allowed to display a thumbnail. */
	uint8 bCanDisplayThumbnail: 1;
};

/**
 * Logic Driver pin override for sound base object pins.
 */
class SGraphPin_SoundObjectPin : public SGraphPin_ObjectPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_SoundObjectPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const UClass* InClass);

protected:
	// SGraphPin_ObjectPin
	virtual TSharedPtr<SWidget> CreateThumbnailOverlayWidget() override;
	virtual TSharedPtr<SToolTip> CreateThumbnailTooltipWidget() override;
	virtual TSharedPtr<SHorizontalBox> CreateButtonsWidget() override;
	// ~SGraphPin_ObjectPin

private:
	/** Creates a play sound button for this asset. */
	TSharedPtr<SWidget> CreatePlaySoundButton(bool bUseLargeButton = false);

	const FSlateBrush* GetPlaySoundDisplayBrush(bool bUseLargeButton) const;
	FText GetPlaySoundTooltip() const;
	FReply OnPlaySoundClicked();
	bool IsPlaySoundEnabled() const;
};