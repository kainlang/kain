// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Misc/Build.h"

#if WITH_EDITOR

#include "IPluginWardenModule.h"

class FSMAuthenticator final
{
public:
	FSMAuthenticator(FSMAuthenticator const&) = delete;
	void operator=(FSMAuthenticator const&) = delete;
private:
	FSMAuthenticator() {}

public:
	static FSMAuthenticator& Get()
	{
		static FSMAuthenticator Authenticator;
		return Authenticator;
	}

	/**
	 * Perform an entitlement check for the plugin against the user's Epic Games account.
	 */
	FORCEINLINE void Authenticate(const FSimpleDelegate& InOnAuthenticatedDelegate)
	{
		if (IsAuthenticated())
		{
			InOnAuthenticatedDelegate.ExecuteIfBound();
			return;
		}

		const FString CatalogItem = TEXT("819543009be949c5b2d40236adcb8166");
		const FString PluginGuid = TEXT("9d8db9962594400988f8ddd3fb83cd88");

		IPluginWardenModule::Get().CheckEntitlementForPlugin(
			NSLOCTEXT("FSMAuth", "LogicDriverPluginName", "Logic Driver Pro"), CatalogItem, PluginGuid,
		NSLOCTEXT("FSMAuth", "UnauthorizedUse", "You are not authorized to use Logic Driver Pro. Marketplace plugin licenses are per-seat.\nWould you like to view the store page?"),
		IPluginWardenModule::EUnauthorizedErrorHandling::ShowMessageOpenStore, [&] ()
		{
			bIsAuthenticated = true;
			InOnAuthenticatedDelegate.ExecuteIfBound();
		});
	}

	FORCEINLINE bool IsAuthenticated() const
	{
#if (PLATFORM_WINDOWS || PLATFORM_MAC) && LOGICDRIVER_IS_MARKETPLACE_BUILD
		return bIsAuthenticated;
#else
		return true;
#endif
	}

private:
	bool bIsAuthenticated = false;
};

#endif