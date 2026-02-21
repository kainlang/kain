// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "EditorStyleSet.h"
#include "SMUnrealTypeDefs.h"
#include "Framework/Commands/Commands.h"

class FSMAssetToolsCommands : public TCommands<FSMAssetToolsCommands>
{
public:
	/** Constructor */
	FSMAssetToolsCommands()
		: TCommands<FSMAssetToolsCommands>(TEXT("SMAssetTools"), NSLOCTEXT("Contexts", "SMAssetTools", "Logic Driver Asset Tools"),
			NAME_None, FSMUnrealAppStyle::Get().GetStyleSetName())
	{
	}

	// TCommand
	virtual void RegisterCommands() override;
	FORCENOINLINE static const FSMAssetToolsCommands& Get();
	// ~TCommand

	/** Export an asset. */
	TSharedPtr<FUICommandInfo> ExportAsset;

	/** Export an asset. */
	TSharedPtr<FUICommandInfo> ImportAsset;
};
