/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "QuestAssetAppMode.h"
#include "QuestAssetEditorApp.h"
#include "QuestAssetPrimaryTabFactory.h"
#include "QuestAssetPropertiesTabFactory.h"

QuestAssetAppMode::QuestAssetAppMode(TSharedPtr<QuestAssetEditorApp> app) : FApplicationMode(TEXT("QuestAssetAppMode")) {
    _app = app;
	QuestAssetPrimaryTabFactory * dialogueEditorTabFactory = new QuestAssetPrimaryTabFactory(app);
    _tabs.RegisterFactory(MakeShareable(dialogueEditorTabFactory));
    _tabs.RegisterFactory(MakeShareable(new QuestAssetPropertiesTabFactory(app)));

    TabLayout = FTabManager::NewLayout("QuestAssetAppMode_Layout_v1")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
					->SetOrientation(Orient_Horizontal)
					->Split
					(
						FTabManager::NewStack()
							->SetSizeCoefficient(0.75)
							->AddTab(FName(TEXT("QuestAssetPrimaryTab")), ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
							->SetSizeCoefficient(0.25)
							->AddTab(FName(TEXT("QuestAssetPropertiesTab")), ETabState::OpenedTab)
					)
			)
	);
}

void QuestAssetAppMode::RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) {
    TSharedPtr<QuestAssetEditorApp> app = _app.Pin();
	app->PushTabFactories(_tabs);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

void QuestAssetAppMode::PreDeactivateMode() {
	FApplicationMode::PreDeactivateMode();
}

void QuestAssetAppMode::PostActivateMode() {
	FApplicationMode::PostActivateMode();
}
