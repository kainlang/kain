// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "AssetToolsModule.h"
#include "Editor.h"
#include "IAssetTools.h"
#include "Factories/Factory.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/IToolkit.h"

#include "Assets/SMContentAsset.h"

#include "Misc/SMAuthenticator.h"

#define LOGICDRIVER_CONTENT_MODULE_NAME "SMContentEditor"

class FExtensibilityManager;

/**
 * The public interface to this module
 */
class ISMContentEditorModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static ISMContentEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ISMContentEditorModule>(LOGICDRIVER_CONTENT_MODULE_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(LOGICDRIVER_CONTENT_MODULE_NAME);
	}

	/**
	 * Trigger content installation dialogue.
	 */
	FORCEINLINE void AddContentToProject() const
	{
		AddContentAuthenticated();
	}

	/**
	 * Checks the CVar status for if content can be created.
	 */
	virtual bool IsContentCreationEnabled() const = 0;

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const = 0;
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const = 0;

private:
	/** Create the appropriate content factory. */
	virtual UFactory* CreateFactory() const = 0;

	FORCEINLINE void AddContentAuthenticated() const
	{
		auto AddContentImpl = [](UFactory* Factory)
		{
			check(Factory);
			Factory->AddToRoot();
			FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(Factory);
			// ConfigureProperties opens installation dialogue. If installing or canceling this will return false.
			// If creating new content this will return true.
			if (Factory->ConfigureProperties())
			{
				IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
				FString Name;
				FString PackageName;
				AssetTools.CreateUniqueAssetName(TEXT("/Game/NewLogicDriverContentSample"), TEXT(""), /*out*/ PackageName, /*out*/ Name);
				const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

				UObject* NewAsset = AssetTools.CreateAsset(Name, PackagePath, USMContentAsset::StaticClass(), Factory);
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(
					NewAsset,
					EToolkitMode::Standalone);
			}
			Factory->RemoveFromRoot();
		};

		if (FSMAuthenticator::Get().IsAuthenticated())
		{
			AddContentImpl(CreateFactory());
		}
		else
		{
			FSMAuthenticator::Get().Authenticate(FSimpleDelegate::CreateLambda([&]()
			{
				if (GEditor && GEditor->IsTimerManagerValid())
				{
					// Add frame delay so the license validation window has a chance to close.
					GEditor->GetTimerManager()->SetTimerForNextTick([&]()
					{
						if (GEditor)
						{
							AddContentImpl(CreateFactory());
						}
					});
				}
			}));
		}
	}
};
