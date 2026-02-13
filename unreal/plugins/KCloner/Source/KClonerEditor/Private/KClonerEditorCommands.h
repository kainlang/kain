// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FKClonerEditorCommands : public TCommands<FKClonerEditorCommands>
{
public:
	FKClonerEditorCommands()
		: TCommands<FKClonerEditorCommands>(
			TEXT("KClonerEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "KClonerEditor", "KCloner Editor"), // Localized context name
			NAME_None, // Parent context name
			FAppStyle::GetAppStyleSetName() // Icon Style Set
		)
	{}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> BakeAnim;
};
