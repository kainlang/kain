// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMEditorCustomization.h"

#include "Blueprints/SMBlueprintGeneratedClass.h"

/**
 * Nested state machine customization including references and parents.
 */
class FSMStateMachineStateCustomization : public FSMNodeCustomization
{
public:
	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// ~IDetailCustomization

	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	void CustomizeParentSelection(IDetailLayoutBuilder& DetailBuilder);
	void CustomizeReferenceDynamicClassSelection(IDetailLayoutBuilder& DetailBuilder);
	void OnUseTemplateChange();

private:
	TArray<TSharedPtr<FName>> AvailableParentClasses;
	TMap<FName, USMBlueprintGeneratedClass*> MappedParentClasses;

	TArray<TSharedPtr<FText>> AvailableVariables;
	TMap<FName, FText> MappedNamesToDisplayNames;

	TSharedPtr<FText> SelectedVariable;
};