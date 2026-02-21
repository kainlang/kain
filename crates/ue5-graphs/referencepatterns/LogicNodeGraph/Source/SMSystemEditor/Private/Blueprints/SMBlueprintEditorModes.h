// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "BlueprintEditorModes.h"

#define LOCTEXT_NAMESPACE "SMBlueprintEditorModes"

class FSMBlueprintEditor;

// This is the list of IDs for SM Editor modes
struct FSMBlueprintEditorModes
{
	// App Name
	SMSYSTEMEDITOR_API static const FName SMEditorName;

	// Mode constants
	SMSYSTEMEDITOR_API static const FName SMGraphMode;
	SMSYSTEMEDITOR_API static const FName SMPreviewMode;
	
	static FText GetLocalizedMode(const FName InMode)
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add(SMGraphMode, NSLOCTEXT("SMEditorModes", "SMGraphMode", "Graph"));
			LocModes.Add(SMPreviewMode, NSLOCTEXT("SMEditorModes", "SMPreviewMode", "Preview"));
		}

		check(InMode != NAME_None);
		const FText* OutDesc = LocModes.Find(InMode);
		check(OutDesc);
		return *OutDesc;
	}
private:
	FSMBlueprintEditorModes() {}
};

class FSMBlueprintEditorModeBase : public FBlueprintEditorApplicationMode
{
public:
	FSMBlueprintEditorModeBase(TSharedPtr<FSMBlueprintEditor> EditorIn, FName EditorModeIn);

protected:
	TWeakPtr<FSMBlueprintEditor> Editor;

	FWorkflowAllowedTabSet EditorTabFactories;
};

class FSMBlueprintEditorGraphMode : public FSMBlueprintEditorModeBase
{
public:
	FSMBlueprintEditorGraphMode(TSharedPtr<class FSMBlueprintEditor> EditorIn);

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	// ~FApplicationMode interface

};

class FSMBlueprintEditorPreviewMode : public FSMBlueprintEditorModeBase
{
public:
	static const FName TabID_DetailsView;
	static const FName TabID_ViewportView;
	static const FName TabID_AdvSceneDetailsView;

public:
	FSMBlueprintEditorPreviewMode(TSharedPtr<class FSMBlueprintEditor> EditorIn);
	virtual ~FSMBlueprintEditorPreviewMode() override;
	
	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	// ~FApplicationMode interface

private:
	TSharedPtr<SWidget> ViewportView;
	TSharedPtr<SWidget> DefaultsView;
	TSharedPtr<SWidget> AdvancedDetailsView;
};

#undef LOCTEXT_NAMESPACE