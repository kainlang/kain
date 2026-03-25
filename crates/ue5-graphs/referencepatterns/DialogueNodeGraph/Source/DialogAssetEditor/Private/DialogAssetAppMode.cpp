/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#include "DialogAssetAppMode.h"
#include "DialogAssetEditorApp.h"
#include "DialogAssetPrimaryTabFactory.h"
#include "DialogAssetPropertiesTabFactory.h"

DialogAssetAppMode::DialogAssetAppMode(TSharedPtr<DialogAssetEditorApp> app) : FApplicationMode(TEXT("DialogAssetAppMode")) {
    _app = app;
	DialogAssetPrimaryTabFactory * dialogueEditorTabFactory = new DialogAssetPrimaryTabFactory(app);
    _tabs.RegisterFactory(MakeShareable(dialogueEditorTabFactory));
    _tabs.RegisterFactory(MakeShareable(new DialogAssetPropertiesTabFactory(app)));

    TabLayout = FTabManager::NewLayout("DialogAssetAppMode_Layout_v1")
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
							->AddTab(FName(TEXT("DialogAssetPrimaryTab")), ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
							->SetSizeCoefficient(0.25)
							->AddTab(FName(TEXT("DialogAssetPropertiesTab")), ETabState::OpenedTab)
					)
			)
	);
}

void DialogAssetAppMode::RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) {
    TSharedPtr<DialogAssetEditorApp> app = _app.Pin();
	app->PushTabFactories(_tabs);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

void DialogAssetAppMode::PreDeactivateMode() {
	FApplicationMode::PreDeactivateMode();
}

void DialogAssetAppMode::PostActivateMode() {
	FApplicationMode::PostActivateMode();
}
