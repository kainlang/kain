// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMEditorCustomization.h"

/**
 * Customize the details panel for node widget configuration.
 */
class FSMNodeWidgetInfoCustomization : public FSMStructCustomization
{
public:
	SMSYSTEMEDITOR_API static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	// ~IPropertyTypeCustomization

private:
	/** Find all owner fields, handling arrays. */
	TArray<const FField*> GetOwnerFields() const;
	
private:
	/** The UProperty this widget info represents. */
	FField* OwnerField = nullptr;
};
