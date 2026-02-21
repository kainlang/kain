// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMBlueprintEditorTabSpawners.h"
#include "SMBlueprintEditor.h"
#include "SMBlueprintEditorModes.h"

#include "SMUnrealTypeDefs.h"

#define LOCTEXT_NAMESPACE "SMBlueprintEditorTabSpawners"

FSMViewSummonerBase::FSMViewSummonerBase(FName InIdentifier, TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn) : FWorkflowTabFactory(InIdentifier, InHostingApp)
	, BlueprintEditor(InHostingApp)
{
	bIsSingleton = false;
	TabWidget = TabWidgetIn;
}

TSharedRef<SWidget> FSMViewSummonerBase::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(TabWidget.IsValid());
	return TabWidget.Pin().ToSharedRef();
}

FSMPreviewDefaultsViewSummoner::FSMPreviewDefaultsViewSummoner(TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn) :
	FSMViewSummonerBase(FSMBlueprintEditorPreviewMode::TabID_DetailsView, InHostingApp, TabWidgetIn)
{
	TabLabel = LOCTEXT("ViewPreviewDefaultsLabel", "Preview Editor");
	TabIcon = FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), "LevelEditor.Tabs.Details");

	ViewMenuDescription = LOCTEXT("ViewPreviewDefaultsDescription", "Shows the preview editor settings");
	ViewMenuTooltip = LOCTEXT("ViewPreviewDefaultsTooltip", "Shows the preview editor settings");
}

FSMPreviewViewportViewSummoner::FSMPreviewViewportViewSummoner(TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn) :
	FSMViewSummonerBase(FSMBlueprintEditorPreviewMode::TabID_ViewportView, InHostingApp, TabWidgetIn)
{
	TabLabel = LOCTEXT("ViewPreviewViewportLabel", "Preview Viewport");
	TabIcon = FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), "LevelEditor.Tabs.Viewports");

	ViewMenuDescription = LOCTEXT("ViewPreviewViewportDescription", "Shows the preview level viewport");
	ViewMenuTooltip = LOCTEXT("ViewPreviewViewportTooltip", "Shows the preview level viewport");
}

FSMPreviewAdvancedDetailsViewSummoner::FSMPreviewAdvancedDetailsViewSummoner(
	TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn) : FSMViewSummonerBase(FSMBlueprintEditorPreviewMode::TabID_AdvSceneDetailsView, InHostingApp, TabWidgetIn)
{
	TabLabel = LOCTEXT("ViewPreviewAdvDetailsLabel", "Preview Scene Settings");
	TabIcon = FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), "LevelEditor.Tabs.Details");

	ViewMenuDescription = LOCTEXT("ViewPreviewAdvDetailsDescription", "Shows the advanced scene details");
	ViewMenuTooltip = LOCTEXT("ViewPreviewAdvDetailsTooltip", "Shows the advanced scene details");
}

#undef LOCTEXT_NAMESPACE
