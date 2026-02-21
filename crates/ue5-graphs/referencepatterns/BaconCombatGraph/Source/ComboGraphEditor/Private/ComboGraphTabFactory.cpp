// © 2025 mrbaconvn. All Rights Reserved.


#include "ComboGraphTabFactory.h"
#include "ComboGraphEditorApp.h"
#include "ComboGraphAsset.h"
#include "Graph/ComboGraph.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "GraphEditor.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "ComboGraphEditor"

#define TAB_NAME "ComboGraphTab"

#define TAB_LABEL LOCTEXT("ComboGraphTab.Label", "Graph")
#define TAB_DESCRIPTION LOCTEXT("ComboGraphTab.Description", "Display combo graph")
#define TAB_TOOLTIP LOCTEXT("ComboGraphTab.ToolTip", "Edit combo graph using nodes and connections")

UComboGraphTabFactory::UComboGraphTabFactory(TSharedPtr<FComboGraphEditorApp> InApp) : FWorkflowTabFactory(TAB_NAME, InApp) {
	App = InApp;
	TabLabel = TAB_LABEL;
	ViewMenuDescription = TAB_DESCRIPTION;
	ViewMenuTooltip = TAB_TOOLTIP;
}

TSharedRef<SWidget> UComboGraphTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const {
	TSharedPtr<FComboGraphEditorApp> EditorApp = App.Pin();

	return EditorApp->GetGraphEditorWidget().ToSharedRef();
}

FText UComboGraphTabFactory::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const {
	return TAB_TOOLTIP;
}

#undef TAB_NAME
#undef TAB_LABEL
#undef TAB_DESCRIPTION
#undef TAB_TOOLTIP

#undef LOCTEXT_NAMESPACE