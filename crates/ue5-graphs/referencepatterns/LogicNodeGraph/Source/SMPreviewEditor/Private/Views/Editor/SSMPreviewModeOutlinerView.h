// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneOutlinerFwd.h"
#include "SGraphPanel.h"

class USMPreviewObject;
class FSMBlueprintEditor;

/**
 * Custom outliner allowing a context to be selected and filtering the world and actor list.
 */
class SSMPreviewModeOutlinerView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSMPreviewModeOutlinerView) {}
	SLATE_END_ARGS()

	~SSMPreviewModeOutlinerView();
	void Construct(const FArguments& InArgs, TSharedPtr<FSMBlueprintEditor> InStateMachineEditor, UWorld* InWorld);
	void CreateWorldOutliner(UWorld* World);

protected:
	void UpdateWidget();
	void OnOutlinerSelectionChanged(FSceneOutlinerTreeItemPtr TreeItem, ESelectInfo::Type Type);
	
	void OnSimulationEnded(USMPreviewObject* PreviewObject);

	// Called then the user presses delete on the scene outliner.
	void OnDelete(const TArray<TWeakPtr<ISceneOutlinerTreeItem>>& InSelectedItem);
	
protected:
	TSharedPtr<SSceneOutliner> SceneOutliner;
	TWeakPtr<FSMBlueprintEditor> BlueprintEditor;
	
	FDelegateHandle SceneOutlinerSelectionChanged;
	FDelegateHandle OnSimEndHandle;
};
