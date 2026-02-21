// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMSupportUtils.h"

#include "SMSystemInfo.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Misc/SMAuthenticator.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Misc/EngineVersion.h"

TSharedPtr<FSMSystemInfo> LD::Support::GenerateSystemInfo()
{
	TSharedPtr<FSMSystemInfo> SupportInfo = MakeShared<FSMSystemInfo>();

	SupportInfo->OSVersion = FPlatformMisc::GetOSVersion();
	SupportInfo->EngineVersion = FEngineVersion::Current().ToString();
	SupportInfo->PluginVersion = FSMBlueprintEditorUtils::GetProjectEditorSettings()->InstalledVersion;

#if LOGICDRIVER_IS_MARKETPLACE_BUILD
	SupportInfo->bIsMarketplace = true;
#else
	SupportInfo->bIsMarketplace = false;
#endif

	SupportInfo->bIsAuthenticated = FSMAuthenticator::Get().IsAuthenticated();
	
	return SupportInfo;
}
