// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
 
class COMBOGRAPHEDITOR_API UComboGraphEditorAppMode : public FApplicationMode
{
public:
	UComboGraphEditorAppMode(TSharedPtr<class FComboGraphEditorApp> app);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<class FComboGraphEditorApp> App;
	FWorkflowAllowedTabSet Tabs;
};