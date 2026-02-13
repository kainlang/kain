// Copyright 2026 K-Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "AlphaGenStyle.h"

/**
 * AlphaGen Editor Commands
 * 
 * Keyboard shortcut bindings for AlphaGen functionality.
 */
class FAlphaGenCommands : public TCommands<FAlphaGenCommands>
{
public:
	FAlphaGenCommands()
		: TCommands<FAlphaGenCommands>(
			TEXT("AlphaGen"),
			NSLOCTEXT("Contexts", "AlphaGen", "AlphaGen Plugin"),
			NAME_None,
			FAlphaGenStyle::GetStyleSetName()
		)
	{
	}

	// TCommands interface
	virtual void RegisterCommands() override;
	
public:
	/** Open the AlphaGen widget */
	TSharedPtr<FUICommandInfo> OpenAlphaGenWidget;
	
	/** Quick generate radial alpha */
	TSharedPtr<FUICommandInfo> QuickGenerateRadial;
	
	/** Quick generate perlin noise alpha */
	TSharedPtr<FUICommandInfo> QuickGeneratePerlin;
};
