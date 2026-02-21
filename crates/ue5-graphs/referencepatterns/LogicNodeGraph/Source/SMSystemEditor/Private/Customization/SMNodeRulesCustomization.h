// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMEditorCustomization.h"

struct FSMNodeClassRule;

/**
 * Customize the details panel for node rule configuration.
 */
class FSMNodeRulesCustomization : public FSMStructCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	// ~IPropertyTypeCustomization

protected:
	/** Retrieve the friendly display name for the rule contained in the current property handle. */
	virtual FText GetRuleDisplayName() const;

	/** Return a properly formatted class name of a node class rule. */
	static FString GetFormattedClassName(const FSMNodeClassRule* InNodeClassRule);
};

class FSMConnectionRulesCustomization : public FSMNodeRulesCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual FText GetRuleDisplayName() const override;
};
