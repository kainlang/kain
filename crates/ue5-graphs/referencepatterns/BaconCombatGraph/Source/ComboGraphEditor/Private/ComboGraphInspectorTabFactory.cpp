// © 2025 mrbaconvn. All Rights Reserved.


#include "ComboGraphInspectorTabFactory.h"
#include "ComboGraphEditorApp.h"
#include "ComboGraphAsset.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "ComboGraphEditor"

#define TAB_NAME "ComboGraphInspectorTab"
#define PROPERTY_EDITOR_MODULE_NAME "PropertyEditor"

#define TAB_LABEL LOCTEXT("ComboGraphInspectorTab.Label", "Details")
#define TAB_DESCRIPTION LOCTEXT("ComboGraphInspectorTab.Description", "Display details")
#define TAB_TOOLTIP LOCTEXT("ComboGraphInspectorTab.ToolTip", "Edit details of the graph")

UComboGraphInspectorTabFactory::UComboGraphInspectorTabFactory(TSharedPtr<FComboGraphEditorApp> InApp) : FWorkflowTabFactory(TAB_NAME, InApp) {
	App = InApp;
	TabLabel = TAB_LABEL;
	ViewMenuDescription = TAB_DESCRIPTION;
	ViewMenuTooltip = TAB_TOOLTIP;
}

TSharedRef<SWidget> UComboGraphInspectorTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const {
	TSharedPtr<FComboGraphEditorApp> EditorApp = App.Pin();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(PROPERTY_EDITOR_MODULE_NAME);

	FDetailsViewArgs detailsViewArgs;
	{
		detailsViewArgs.bAllowSearch = false;
		detailsViewArgs.bHideSelectionTip = true;
		detailsViewArgs.bLockable = false;
		detailsViewArgs.bSearchInitialKeyFocus = true;
		detailsViewArgs.bUpdatesFromSelection = false;
		detailsViewArgs.NotifyHook = nullptr;
		detailsViewArgs.bShowOptions = true;
		detailsViewArgs.bShowModifiedPropertiesOption = false;
		detailsViewArgs.bShowScrollBar = false;
	}

	TSharedPtr<IDetailsView> detailsView = PropertyEditorModule.CreateDetailView(detailsViewArgs);
	detailsView->SetObject(EditorApp->GetWorkingAsset());

	TSharedPtr<IDetailsView> selectedNodeDetailsView = PropertyEditorModule.CreateDetailView(detailsViewArgs);
	selectedNodeDetailsView->SetObject(nullptr);
	EditorApp->SetSelectedNodeDetailView(selectedNodeDetailsView);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(0.5f)
		.HAlign(HAlign_Fill)
		[
			detailsView.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.FillHeight(2.f)
		.HAlign(HAlign_Fill)
		[
			selectedNodeDetailsView.ToSharedRef()
		];
}

FText UComboGraphInspectorTabFactory::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const {
	return TAB_TOOLTIP;
}

#undef TAB_NAME
#undef PROPERTY_EDITOR_MODULE_NAME
#undef TAB_LABEL
#undef TAB_DESCRIPTION
#undef TAB_TOOLTIP

#undef LOCTEXT_NAMESPACE