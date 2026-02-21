// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"

struct SMEXTENDEDEDITOR_API FSMTextGraphPropertyCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// When reading and setting defaults was added
		DefaultsSupported,
		
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FSMTextGraphPropertyCustomVersion() {}
};
