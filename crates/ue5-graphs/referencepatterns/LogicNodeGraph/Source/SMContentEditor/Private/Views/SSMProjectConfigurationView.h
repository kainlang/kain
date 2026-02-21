// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/NotifyHook.h"

class USMContentProjectConfigurationViewModel;
class IDetailsView;

/**
 * View for project configuration of a content sample.
 */
class SSMProjectConfigurationView : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SSMProjectConfigurationView) {}
	SLATE_ATTRIBUTE(USMContentProjectConfigurationViewModel*, ViewModel)

	SLATE_END_ARGS()

	virtual ~SSMProjectConfigurationView() override;
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Update the details object for this view. */
	void SetDetailsObject(UObject* InObject);

private:
	TWeakObjectPtr<USMContentProjectConfigurationViewModel> ViewModel;
	TSharedPtr<IDetailsView> PropertyView;
};
