// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class COMBOGRAPHEDITOR_API UComboGraphTabFactory : public FWorkflowTabFactory {

public:
	UComboGraphTabFactory(TSharedPtr<class FComboGraphEditorApp> App);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class FComboGraphEditorApp> App;
};