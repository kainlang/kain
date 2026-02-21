// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMVersionUtils.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "ISMSystemModule.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "Blueprints/SMBlueprint.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/ScopedSlowTask.h"
#include "PluginDescriptor.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SMVersionUtils"

#define LD_SM_BLUEPRINT_VERSION 3
#define LD_NODE_BLUEPRINT_VERSION 2

static TWeakPtr<SNotificationItem> WrongVersionNotification;

int32 FSMVersionUtils::GetCurrentBlueprintVersion()
{
	return LD_SM_BLUEPRINT_VERSION;
}

int32 FSMVersionUtils::GetCurrentBlueprintNodeVersion()
{
	return LD_NODE_BLUEPRINT_VERSION;
}

int32 FSMVersionUtils::GetCurrentPluginVersion()
{
	const FString PluginName = LD_PLUGIN_NAME;
	IPluginManager& PluginManager = IPluginManager::Get();

	const TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
	check(Plugin.IsValid());

	const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
	return Descriptor.Version;
}

void FSMVersionUtils::UpdateBlueprintsToNewVersion()
{
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();

	TArray<FAssetData> OutNodeAssets, OutStateMachineAssets, AssetsToUpdate;
	AssetRegistry.GetAssetsByClass(USMNodeBlueprint::StaticClass()->GetClassPathName(), OutNodeAssets, true);
	AssetRegistry.GetAssetsByClass(USMBlueprint::StaticClass()->GetClassPathName(), OutStateMachineAssets, true);

	bool bIsAssetVersionNotSupported = false;
	
	// Check nodes first, they should be updated prior to state machines.
	for (const FAssetData& Asset : OutNodeAssets)
	{
		int32 AssetVersion = 0;

		const bool bAssetVersionFound = Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(USMNodeBlueprint, AssetVersion), AssetVersion);
		if (!bAssetVersionFound || !IsStateMachineNodeUpToDate(AssetVersion))
		{
			AssetsToUpdate.Add(Asset);
			continue;
		}

		int32 PluginVersion = 0;
		Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(USMNodeBlueprint, PluginVersion), PluginVersion);
		if (IsStateMachineNodeFromNewerPluginVersion(AssetVersion, PluginVersion))
		{
			LDEDITOR_LOG_ERROR(TEXT("Logic Driver asset '%s' is from a newer version of the plugin! There may be data loss if saved!"), *Asset.AssetName.ToString());
			bIsAssetVersionNotSupported = true;
		}
	}

	// State machines next.
	for (const FAssetData& Asset : OutStateMachineAssets)
	{
		int32 Version = 0;
		
		const bool bVersionFound = Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(USMBlueprint, AssetVersion), Version);
		if (!bVersionFound || !IsStateMachineUpToDate(Version))
		{
			AssetsToUpdate.Add(Asset);
			continue;
		}

		int32 PluginVersion = 0;
		Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(USMBlueprint, PluginVersion), PluginVersion);
		if (IsStateMachineFromNewerPluginVersion(Version, PluginVersion))
		{
			LDEDITOR_LOG_ERROR(TEXT("Logic Driver asset '%s' is from a newer version of the plugin! There may be data loss if saved!"), *Asset.AssetName.ToString());
			bIsAssetVersionNotSupported = true;
		}
	}

	if (bIsAssetVersionNotSupported)
	{
		LDEDITOR_LOG_ERROR(TEXT("Logic Driver assets are from a newer version of the plugin! Please update Logic Driver and verify your team is using the same version."));

		if (FApp::CanEverRender())
		{
			// There are assets from a newer version of the plugin.
			FNotificationInfo Info(LOCTEXT("LogicDriverAssetsFromNewerVersion", "Logic Driver assets are from a newer version of the plugin!\nPlease update Logic Driver and verify your team is using the same version."));
			Info.bFireAndForget = false;
			Info.bUseLargeFont = false;
			Info.bUseThrobber = false;
			Info.FadeOutDuration = 0.25f;
			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("LogicDriverWrongVersionDismiss", "Dismiss"), LOCTEXT("LogicDriverWrongVersionDismissTT", "Dismiss this notification"), FSimpleDelegate::CreateStatic(&FSMVersionUtils::DismissWrongVersionNotification)));
		
			WrongVersionNotification = FSlateNotificationManager::Get().AddNotification(Info);
			WrongVersionNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}

	if (AssetsToUpdate.Num() > 0)
	{
		FScopedSlowTask Feedback(AssetsToUpdate.Num(), NSLOCTEXT("LogicDriver", "LogicDriverAssetUpdate", "Updating Logic Driver assets to the current version..."));

		if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bDisplayAssetUpdateProgress)
		{
			Feedback.MakeDialog(true);
		}
		
		for (FAssetData& Asset : AssetsToUpdate)
		{
			if (USMBlueprint* SMBlueprint = Cast<USMBlueprint>(Asset.GetAsset()))
			{
				// Parent shouldn't be null, but licensee may have deleted the parent instead of deprecating it,
				// and Unreal could trip some ensures or checks during compile later.
				if (SMBlueprint->ParentClass.Get() != nullptr)
				{
					// Fixes existing broken graphs for t-141.
					USMBlueprintFactory::CreateGraphsForBlueprintIfMissing(SMBlueprint);
				
					TArray<USMGraphNode_Base*> GraphNodes;
					FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_Base>(SMBlueprint, GraphNodes);

					for (USMGraphNode_Base* Node : GraphNodes)
					{
						Node->ConvertToCurrentVersion(false);
					}

					SetToLatestVersion(SMBlueprint);
					SMBlueprint->MarkPackageDirty();
				}
			}
			else if (USMNodeBlueprint* NodeBlueprint = Cast<USMNodeBlueprint>(Asset.GetAsset()))
			{
				// Parent shouldn't be null, but licensee may have deleted the parent instead of deprecating it,
				// and Unreal could trip some ensures or checks during compile later.
				if (NodeBlueprint->ParentClass.Get() != nullptr)
				{
					SetToLatestVersion(NodeBlueprint);
					// For now we just need to recompile the node BP.
					FBlueprintEditorUtils::MarkBlueprintAsModified(NodeBlueprint);
				}
			}
			
			Feedback.CompletedWork += 1;
		}
	}
}

void FSMVersionUtils::UpdateProjectToNewVersion(const FString& PreviousVersionName)
{
	const FVersion PreviousVersion(PreviousVersionName);

	const FVersion ProBaseVersion("2.0.0");
	const FVersion ConstructionScriptVersion(LD_PLUGIN_VERSION_CONSTRUCTION_SCRIPTS);
	if (PreviousVersion < ConstructionScriptVersion && PreviousVersion >= ProBaseVersion)
	{
		// For safety default to legacy construction scripts when updating.
		USMProjectEditorSettings* Settings = GetMutableDefault<USMProjectEditorSettings>();
		Settings->EditorNodeConstructionScriptSetting = ESMEditorConstructionScriptProjectSetting::SM_Legacy;
		Settings->SaveConfig();
	}
}

bool FSMVersionUtils::IsStateMachineUpToDate(int32 CompareVersion)
{
	return CompareVersion >= GetCurrentBlueprintVersion();
}

bool FSMVersionUtils::IsStateMachineNodeUpToDate(int32 CompareVersion)
{
	return CompareVersion >= GetCurrentBlueprintNodeVersion();
}

bool FSMVersionUtils::IsAssetUpToDate(UBlueprint* Blueprint)
{
	if (USMBlueprint* SMBlueprint = Cast<USMBlueprint>(Blueprint))
	{
		const int32 Version = SMBlueprint->AssetVersion;
		return IsStateMachineUpToDate(Version);
	}
	
	if (USMNodeBlueprint* NodeBlueprint = Cast<USMNodeBlueprint>(Blueprint))
	{
		const int32 Version = NodeBlueprint->AssetVersion;
		return IsStateMachineNodeUpToDate(Version);
	}
	
	return false;
}

bool FSMVersionUtils::IsStateMachineFromNewerPluginVersion(int32 AssetVersion, int32 PluginVersion)
{
	return AssetVersion > GetCurrentBlueprintVersion() || PluginVersion > GetCurrentPluginVersion();
}

bool FSMVersionUtils::IsStateMachineNodeFromNewerPluginVersion(int32 AssetVersion, int32 PluginVersion)
{
	return AssetVersion > GetCurrentBlueprintNodeVersion() || PluginVersion > GetCurrentPluginVersion();
}

void FSMVersionUtils::SetToLatestVersion(UBlueprint* Blueprint)
{
	if (USMBlueprint* SMBlueprint = Cast<USMBlueprint>(Blueprint))
	{
		SMBlueprint->AssetVersion = GetCurrentBlueprintVersion();
		SMBlueprint->PluginVersion = GetCurrentPluginVersion();
	}
	else if (USMNodeBlueprint* NodeBlueprint = Cast<USMNodeBlueprint>(Blueprint))
	{
		NodeBlueprint->AssetVersion = GetCurrentBlueprintNodeVersion();
		NodeBlueprint->PluginVersion = GetCurrentPluginVersion();
	}
}

void FSMVersionUtils::DismissWrongVersionNotification()
{
	WrongVersionNotification.Pin()->ExpireAndFadeout();
}

#undef LOCTEXT_NAMESPACE
