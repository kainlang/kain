// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"
#include "IPropertyTypeCustomization.h"

class FSMKeyStructCustomization;

/**
 * Customization to produce a list of readonly keys in a display similar to FEnhancedActionMappingCustomization and FKeyStructCustomization.
 * The built-in customization doesn't properly support readonly displays and looks like it may change its API.
 */
class FSMKeyNameCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FSMKeyNameCustomization());
	}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	TOptional<FKey> GetCurrentKey(TSharedRef<IPropertyHandle> PropertyHandle) const;

};