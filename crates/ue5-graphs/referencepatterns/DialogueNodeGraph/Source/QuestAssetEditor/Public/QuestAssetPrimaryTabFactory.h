/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class QuestAssetPrimaryTabFactory : public FWorkflowTabFactory {
public:
	QuestAssetPrimaryTabFactory(TSharedPtr<class QuestAssetEditorApp> app);
	

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class QuestAssetEditorApp> _app;

	TSharedPtr<FUICommandList> GraphEditorCommands;
	bool _bEditorCommandsCreated = false;

	void Initialize();
	void CreateEditorCommands();

	void DeleteSelectedNodes();
	bool CanDeleteNodes();
};
