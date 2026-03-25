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
#include "DialogueLine.h"

class DialogAssetPrimaryTabFactory : public FWorkflowTabFactory {
public:
	DialogAssetPrimaryTabFactory(TSharedPtr<class DialogAssetEditorApp> app);
	

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class DialogAssetEditorApp> _app;

	TSharedPtr<FUICommandList> GraphEditorCommands;
	bool _bEditorCommandsCreated = false;

	void Initialize();
	void CreateEditorCommands();

	void DeleteSelectedNodes();
	bool CanDeleteNodes();

	void PasteDialogueText();
	bool CanPasteDialogueText();
	static TArray<FDialogueLine> ParseDialogue(const FString& InputText);
};
