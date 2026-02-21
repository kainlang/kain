// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "BlueprintEditorModes.h"

class FSMBlueprintEditor;

struct FSMViewSummonerBase : public FWorkflowTabFactory
{
	FSMViewSummonerBase(FName InIdentifier, TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
protected:
	TWeakPtr<FSMBlueprintEditor> BlueprintEditor;
	TWeakPtr<SWidget> TabWidget;
};

struct FSMPreviewDefaultsViewSummoner : public FSMViewSummonerBase
{
	FSMPreviewDefaultsViewSummoner(TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn);
};

struct FSMPreviewViewportViewSummoner : public FSMViewSummonerBase
{
	FSMPreviewViewportViewSummoner(TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn);
};

struct FSMPreviewAdvancedDetailsViewSummoner : public FSMViewSummonerBase
{
	FSMPreviewAdvancedDetailsViewSummoner(TSharedPtr<FSMBlueprintEditor> InHostingApp, TSharedPtr<SWidget> TabWidgetIn);
};