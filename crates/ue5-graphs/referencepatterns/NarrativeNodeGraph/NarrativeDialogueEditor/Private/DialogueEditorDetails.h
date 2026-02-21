// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Widgets/Input/SComboBox.h"
#include "IPropertyTypeCustomization.h"

/**
 * Customization for the dialogue editor
 */
class FDialogueEditorDetails : public IDetailCustomization
{

public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/**We cache a reference to this so we can do refreshes*/
	IDetailLayoutBuilder* LayoutBuilder;

	TArray<TSharedPtr<FText>> SpeakersList;
	TSharedPtr<FText> SelectedItem;

	FText GetSpeakerText() const;
	TSharedRef<SWidget> MakeWidgetForOption(TSharedPtr<FText> InOption);

	void OnSelectionChanged(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo);

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	FReply SetTransformsFromActorSelection();

};
//
//class FSpeakerSelectorCustomization : public IPropertyTypeCustomization
//{
//public:
//
//	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
//
//	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils);
//	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils);
//
//protected:
//
//	FText GetSpeakerText() const;
//	TSharedRef<SWidget> MakeWidgetForOption(TSharedPtr<FText> InOption);
//	virtual void OnSelectionChanged(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo);
//
//protected:
//
//	void UpdateProperty();
//
//	TSharedPtr<IPropertyHandle> SpeakerProperty;
//
//	TSharedPtr<SComboBox<TSharedPtr<FName>>> ComboBox;
//	TArray<TSharedPtr<FName>> SpeakersList;
//	TSharedPtr<FName> SelectedItem;
//
//	UDialogue* Dialogue;
//
//};