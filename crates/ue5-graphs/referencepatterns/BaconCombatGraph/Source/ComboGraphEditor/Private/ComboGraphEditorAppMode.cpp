// © 2025 mrbaconvn. All Rights Reserved.


#include "ComboGraphEditorAppMode.h"
#include "ComboGraphEditorApp.h"
#include "ComboGraphInspectorTabFactory.h"
#include "ComboGraphTabFactory.h"

#define MODE_NAME "ComboGraphEditorAppMode"
#define LAYOUT_NAME "ComboGraphEditor_Layout_v1"

UComboGraphEditorAppMode::UComboGraphEditorAppMode(TSharedPtr<FComboGraphEditorApp> app) : FApplicationMode(MODE_NAME) {
	App = app;

	UComboGraphInspectorTabFactory* DetailTabFactory = new UComboGraphInspectorTabFactory(app);
	UComboGraphTabFactory* GraphTabFactory = new UComboGraphTabFactory(app);

	Tabs.RegisterFactory(MakeShareable(new UComboGraphInspectorTabFactory(app)));
	Tabs.RegisterFactory(MakeShareable(new UComboGraphTabFactory(app)));
	
	TabLayout = FTabManager::NewLayout(LAYOUT_NAME)
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
				->AddTab(GraphTabFactory->GetIdentifier(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25)
				->AddTab(DetailTabFactory->GetIdentifier(), ETabState::OpenedTab)
			)
		)
	);
}

void UComboGraphEditorAppMode::RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) {
	TSharedPtr<FComboGraphEditorApp> EditorApp = App.Pin();
	EditorApp->PushTabFactories(Tabs);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

void UComboGraphEditorAppMode::PreDeactivateMode() {
	FApplicationMode::PreDeactivateMode();
}

void UComboGraphEditorAppMode::PostActivateMode() {
	FApplicationMode::PostActivateMode();
}