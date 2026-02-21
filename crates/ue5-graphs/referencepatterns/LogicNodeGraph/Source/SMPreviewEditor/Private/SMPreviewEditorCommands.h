// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "EditorStyleSet.h"
#include "SMUnrealTypeDefs.h"
#include "Framework/Commands/Commands.h"

class FSMPreviewEditorCommands : public TCommands<FSMPreviewEditorCommands>
{
public:
	/** Constructor */
	FSMPreviewEditorCommands()
		: TCommands<FSMPreviewEditorCommands>(TEXT("SMPreviewEditor"), NSLOCTEXT("Contexts", "SMPreviewEditor", "Logic Driver Preview Editor"),
			NAME_None, FSMUnrealAppStyle::Get().GetStyleSetName())
	{
	}

	// TCommand
	virtual void RegisterCommands() override;
	FORCENOINLINE static const FSMPreviewEditorCommands& Get();
	// ~TCommand

	/** Resets the camera to the default position. */
	TSharedPtr<FUICommandInfo> ResetCamera;
	
	/** Enable/disable editor grid. */
	TSharedPtr<FUICommandInfo> ShowGrid;
};
