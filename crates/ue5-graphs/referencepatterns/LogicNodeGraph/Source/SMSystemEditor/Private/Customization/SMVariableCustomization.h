// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FKismetCompilerContext;
class IPropertyHandle;
class IPropertyUtilities;
class ISinglePropertyView;
class IDetailLayoutBuilder;
class IBlueprintEditor;
class UBlueprint;
class USMNodeInstance;

class FSMVariableCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedPtr<IDetailCustomization> MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor);

	FSMVariableCustomization(const TSharedPtr<IBlueprintEditor>& InBlueprintEditor, UBlueprint* Blueprint)
		: BlueprintEditorPtr(InBlueprintEditor)
		, BlueprintPtr(Blueprint)
	{}

	virtual ~FSMVariableCustomization() override;
	
	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// ~IDetailCustomization

private:
	void OnStructContentsPreChanged(USMNodeInstance* InNodeInstance);

	bool IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> InProperty) const;
	void OnResetToDefaultClicked(TSharedPtr<IPropertyHandle> InProperty);

	void OnNodeCompiled(const FKismetCompilerContext& InCompilerContext, TSharedPtr<IPropertyUtilities> InPropertyUtilities);
	
private:
	/** The Blueprint editor we are embedded in. */
	TWeakPtr<IBlueprintEditor> BlueprintEditorPtr;

	/** The blueprint we are editing. */
	TWeakObjectPtr<UBlueprint> BlueprintPtr;

	/** Stores a handle to exposed property overrides. */
	TSharedPtr<ISinglePropertyView> ExposedPropertyOverridePropertyView;
};
