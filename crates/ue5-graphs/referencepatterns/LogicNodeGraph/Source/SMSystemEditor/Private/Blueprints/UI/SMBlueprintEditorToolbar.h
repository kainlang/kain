// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

class FSMBlueprintEditor;

class FSMBlueprintEditorToolbar : public TSharedFromThis<FSMBlueprintEditorToolbar> {
public:
	FSMBlueprintEditorToolbar(TSharedPtr<FSMBlueprintEditor> InEditor)
		: Editor(InEditor) {
	}

	void AddModesToolbar(TSharedPtr<FExtender> Extender);
	void AddPreviewToolbar(TSharedPtr<FExtender> Extender);

protected:
	void FillModesToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillPreviewToolbar(FToolBarBuilder& ToolbarBuilder);

private:
	TWeakPtr<FSMBlueprintEditor> Editor;
};
