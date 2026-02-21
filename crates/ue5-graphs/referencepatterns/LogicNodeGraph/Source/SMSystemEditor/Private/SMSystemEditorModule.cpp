// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMSystemEditorModule.h"

#include "Blueprints/SMBlueprintAssetTypeActions.h"
#include "Blueprints/SMBlueprintFactory.h"
#include "Commands/SMEditorCommands.h"
#include "Compilers/SMKismetCompiler.h"
#include "Configuration/SMEditorStyle.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Customization/SMEditorCustomization.h"
#include "Customization/SMLinkStateCustomization.h"
#include "Customization/SMNodeRulesCustomization.h"
#include "Customization/SMNodeStackCustomization.h"
#include "Customization/SMNodeWidgetInfoCustomization.h"
#include "Customization/SMStateMachineStateCustomization.h"
#include "Customization/SMTransitionEdgeCustomization.h"
#include "Customization/SMVariableCustomization.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Pins/SGraphPin_ActorSoftReferencePin.h"
#include "Graph/Pins/SGraphPin_ObjectPin.h"
#include "Graph/Pins/StateSelection/SGraphPin_GetStateByNamePin.h"
#include "Graph/SMGraphFactory.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMVersionUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "ISMSystemModule.h"
#include "SMRuntimeSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintEditorModule.h"
#include "Configuration/SMEditorSettings.h"
#include "EdGraphUtilities.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompilerModule.h"
#include "Misc/App.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectThreadContext.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "SMSystemEditorModule"

DEFINE_LOG_CATEGORY(LogLogicDriverEditor);

void FSMSystemEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FSMEditorStyle::Initialize();
	FSMEditorCommands::Register();
	RegisterSettings();
	
	// Register blueprint compiler -- primarily seems to be used when creating a new BP.
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	KismetCompilerModule.GetCompilers().Add(&SMBlueprintCompiler);
	KismetCompilerModule.GetCompilers().Add(&SMNodeBlueprintCompiler);

	// This is needed for actually pressing compile on the BP.
	FKismetCompilerContext::RegisterCompilerForBP(USMBlueprint::StaticClass(), &FSMSystemEditorModule::GetCompilerForStateMachineBP);
	FKismetCompilerContext::RegisterCompilerForBP(USMNodeBlueprint::StaticClass(), &FSMSystemEditorModule::GetCompilerForNodeBP);
	
	// Register graph related factories.
	SMGraphPanelNodeFactory = MakeShareable(new FSMGraphPanelNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(SMGraphPanelNodeFactory);

	RegisterPinFactories();
	
	RefreshAllNodesDelegateHandle = FBlueprintEditorUtils::OnRefreshAllNodesEvent.AddStatic(&FSMBlueprintEditorUtils::HandleRefreshAllNodes);
	RenameVariableDelegateHandle = FBlueprintEditorUtils::OnRenameVariableReferencesEvent.AddStatic(&FSMBlueprintEditorUtils::HandleRenameVariableEvent);
	
	// Register details customization.
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_StateNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_StateMachineStateNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMStateMachineStateCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_TransitionEdge::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMTransitionEdgeCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_RerouteNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMTransitionEdgeCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_ConduitNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_AnyStateNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_LinkStateNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMLinkStateCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USMGraphNode_StateMachineEntryNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeCustomization::MakeInstance));
	
	// Covers all node instances.
	PropertyModule.RegisterCustomClassLayout(USMNodeInstance::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeInstanceCustomization::MakeInstance));

	// State Stack.. forwards off requests to FSMNodeInstanceCustomization.
	FSMStructCustomization::RegisterNewStruct<FSMStateStackCustomization>(FStateStackContainer::StaticStruct()->GetFName());
	
	// Transition Stack.
	FSMStructCustomization::RegisterNewStruct<FSMTransitionStackCustomization>(FTransitionStackContainer::StaticStruct()->GetFName());

	// Rule customization.
	FSMStructCustomization::RegisterNewStruct<FSMNodeRulesCustomization>(FSMStateClassRule::StaticStruct()->GetFName());
	FSMStructCustomization::RegisterNewStruct<FSMNodeRulesCustomization>(FSMTransitionClassRule::StaticStruct()->GetFName());
	FSMStructCustomization::RegisterNewStruct<FSMNodeRulesCustomization>(FSMStateMachineClassRule::StaticStruct()->GetFName());
	FSMStructCustomization::RegisterNewStruct<FSMConnectionRulesCustomization>(FSMNodeConnectionRule::StaticStruct()->GetFName());

	// Widget Info Customization.
	FSMStructCustomization::RegisterNewStruct<FSMNodeWidgetInfoCustomization>(FSMNodeWidgetInfo::StaticStruct()->GetFName());
	FSMStructCustomization::RegisterNewStruct<FSMNodeWidgetInfoCustomization>(FSMTextDisplayWidgetInfo::StaticStruct()->GetFName());
	
#if LOGICDRIVER_HAS_PROPER_VARIABLE_CUSTOMIZATION
	if (FModuleManager::Get().IsModuleLoaded("Kismet"))
	{
		RegisterBlueprintVariableCustomization();
	}
	else
	{
		ModuleChangedHandle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FSMSystemEditorModule::HandleModuleChanged);
	}
#endif

	// Register asset categories.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Create a custom menu category.
	const EAssetTypeCategories::Type AssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(
		FName(TEXT("StateMachine")), LOCTEXT("StateMachineAssetCategory", "State Machines"));
	// Register state machines under our own category menu and under the Blueprint menu.
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FSMBlueprintAssetTypeActions(EAssetTypeCategories::Blueprint | AssetCategoryBit)));

	// Default configuration for node classes.
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FSMNodeInstanceAssetTypeActions(AssetCategoryBit)));
	
	// Hide base instance from showing up in misc menu.
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FSMInstanceAssetTypeActions(EAssetTypeCategories::None)));
	
	BeginPieHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FSMSystemEditorModule::BeginPIE);
	EndPieHandle = FEditorDelegates::EndPIE.AddRaw(this, &FSMSystemEditorModule::EndPie);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(this, &FSMSystemEditorModule::OnAssetAdded);

	const USMProjectEditorSettings* ProjectEditorSettings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
	if (ProjectEditorSettings->bUpdateAssetsOnStartup)
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FilesLoadedHandle = AssetRegistryModule.Get().OnFilesLoaded().AddStatic(&FSMVersionUtils::UpdateBlueprintsToNewVersion);
	}

	CheckForNewInstalledVersion();
}

void FSMSystemEditorModule::ShutdownModule()
{
	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	// Unregister all the asset types that we registered.
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
		}
	}

	FEdGraphUtilities::UnregisterVisualNodeFactory(SMGraphPanelNodeFactory);

	UnregisterPinFactories();
	
	FBlueprintEditorUtils::OnRefreshAllNodesEvent.Remove(RefreshAllNodesDelegateHandle);
	FBlueprintEditorUtils::OnRenameVariableReferencesEvent.Remove(RenameVariableDelegateHandle);
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_StateNode::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_StateMachineStateNode::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_TransitionEdge::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_RerouteNode::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_ConduitNode::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_AnyStateNode::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_LinkStateNode::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(USMGraphNode_StateMachineEntryNode::StaticClass()->GetFName());
	
	PropertyModule.UnregisterCustomClassLayout(USMNodeInstance::StaticClass()->GetFName());
	
	FSMStructCustomization::UnregisterAllStructs();

#if LOGICDRIVER_HAS_PROPER_VARIABLE_CUSTOMIZATION
	UnregisterBlueprintVariableCustomization();
#endif

	if (ModuleChangedHandle.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(ModuleChangedHandle);
	}

	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::GetModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Remove(&SMBlueprintCompiler);
	KismetCompilerModule.GetCompilers().Remove(&SMNodeBlueprintCompiler);

	FSMEditorCommands::Unregister();
	FSMEditorStyle::Shutdown();
	UnregisterSettings();

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FEditorDelegates::BeginPIE.Remove(BeginPieHandle);
	FEditorDelegates::EndPIE.Remove(EndPieHandle);

	if (AssetAddedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
	}
	
	if (FilesLoadedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().Remove(FilesLoadedHandle);
	}
}

TSharedPtr<FWorkspaceItem> FSMSystemEditorModule::GetToolsWorkspaceGroup() const
{
	if (!LogicDriverToolsWorkspaceGroup.IsValid())
	{
		const FSlateIcon LogicDriverIcon(FSMEditorStyle::GetStyleSetName(), "ClassIcon.SMInstance");
		LogicDriverToolsWorkspaceGroup = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
		LOCTEXT("LogicDriverToolsGroup", "Logic Driver"), LogicDriverIcon);
	}

	return LogicDriverToolsWorkspaceGroup;
}

void FSMSystemEditorModule::RegisterBlueprintVariableCustomization()
{
	if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bEnableVariableCustomization)
	{
		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
		BlueprintVariableCustomizationHandle = BlueprintEditorModule.RegisterVariableCustomization(FProperty::StaticClass(),
			FOnGetVariableCustomizationInstance::CreateStatic(&FSMVariableCustomization::MakeInstance));
	}
}

void FSMSystemEditorModule::UnregisterBlueprintVariableCustomization()
{
	if (FBlueprintEditorModule* BlueprintEditorModule = FModuleManager::GetModulePtr<FBlueprintEditorModule>("Kismet"))
	{
		BlueprintEditorModule->UnregisterVariableCustomization(FProperty::StaticClass(), BlueprintVariableCustomizationHandle);
	}
}

void FSMSystemEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

TSharedPtr<FKismetCompilerContext> FSMSystemEditorModule::GetCompilerForStateMachineBP(UBlueprint* BP,
	FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FSMKismetCompilerContext(CastChecked<USMBlueprint>(BP), InMessageLog, InCompileOptions));
}

TSharedPtr<FKismetCompilerContext> FSMSystemEditorModule::GetCompilerForNodeBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FSMNodeKismetCompilerContext(CastChecked<USMNodeBlueprint>(BP), InMessageLog, InCompileOptions));
}

void FSMSystemEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "Plugins", "LogicDriverEditor",
			LOCTEXT("SMEditorSettingsName", "Logic Driver Editor"),
			LOCTEXT("SMEditorSettingsDescription", "Configure the state machine editor."),
			GetMutableDefault<USMEditorSettings>());

		SettingsModule->RegisterSettings("Project", "Plugins", "LogicDriverRuntime",
			LOCTEXT("SMRuntimeSettingsName", "Logic Driver"),
			LOCTEXT("SMRuntimeSettingsDescription", "Configure runtime options for Logic Driver."),
			GetMutableDefault<USMRuntimeSettings>());
		
		SettingsModule->RegisterSettings("Project", "Plugins", "LogicDriverEditor",
			LOCTEXT("SMProjectEditorSettingsName", "Logic Driver Editor"),
			LOCTEXT("SMProjectEditorSettingsDescription", "Configure the state machine editor."),
			GetMutableDefault<USMProjectEditorSettings>());
	}
}

void FSMSystemEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "LogicDriverEditor");
		SettingsModule->UnregisterSettings("Project", "Plugins", "LogicDriverEditor");
		SettingsModule->UnregisterSettings("Project", "Plugins", "LogicDriverRuntime");
	}
}

void FSMSystemEditorModule::RegisterPinFactories()
{
	SMGraphPinNodeFactory = MakeShareable(new FSMGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(SMGraphPinNodeFactory);

	SMPinNodeNameFactory = MakeShareable(new FSMGetStateByNamePinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(SMPinNodeNameFactory);
	
	const USMProjectEditorSettings* ProjectEditorSettings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
	if (ProjectEditorSettings->OverrideActorSoftReferencePins != ESMPinOverride::None)
	{
		SMPinSoftActorReferenceFactory = MakeShared<FSMActorSoftReferencePinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(SMPinSoftActorReferenceFactory);
	}
	if (ProjectEditorSettings->OverrideObjectPins != ESMPinOverride::None)
	{
		SMPinObjectFactory = MakeShared<FSMObjectPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(SMPinObjectFactory);
	}
}

void FSMSystemEditorModule::UnregisterPinFactories()
{
	FEdGraphUtilities::UnregisterVisualPinFactory(SMGraphPinNodeFactory);
	FEdGraphUtilities::UnregisterVisualPinFactory(SMPinNodeNameFactory);
	if (SMPinSoftActorReferenceFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(SMPinSoftActorReferenceFactory);
	}
	if (SMPinObjectFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(SMPinObjectFactory);
	}
}

void FSMSystemEditorModule::OnAssetAdded(const FAssetData& InAssetData)
{
	// This is a very slow task! Only check if the asset is already loaded!
	if (InAssetData.IsValid() && !InAssetData.IsRedirector() && InAssetData.IsAssetLoaded())
	{
		if (InAssetData.AssetClassPath == USMBlueprint::StaticClass()->GetClassPathName())
		{
			/*
			* Newly created blueprints need their SM graphs initially set up.
			* Creating blueprints from content menus, blueprint menus, or child menus
			* all trigger OnAssetAdded, but don't go through the same factory routines.
			*/
			
			USMBlueprint* Blueprint = CastChecked<USMBlueprint>(InAssetData.GetAsset());
			{
				USMBlueprintFactory::CreateGraphsForBlueprintIfMissing(Blueprint);
				// Prevents REINST class ensures in 4.27+ with child blueprints.
				if (!FUObjectThreadContext::Get().IsRoutingPostLoad && Blueprint->bIsNewlyCreated)
				{
					FKismetEditorUtilities::CompileBlueprint(Blueprint);
				}
			}
		}
		else if (InAssetData.AssetClassPath == USMNodeBlueprint::StaticClass()->GetClassPathName())
		{
			USMNodeBlueprint* NodeBlueprint = CastChecked<USMNodeBlueprint>(InAssetData.GetAsset());
			if (NodeBlueprint->bIsNewlyCreated)
			{
				USMNodeBlueprintFactory::SetupNewBlueprint(NodeBlueprint);
			}
		}
	}
}

void FSMSystemEditorModule::BeginPIE(bool bValue)
{
	bPlayingInEditor = true;
}

void FSMSystemEditorModule::EndPie(bool bValue)
{
	bPlayingInEditor = false;
}

void FSMSystemEditorModule::CheckForNewInstalledVersion()
{
	const FString PluginName = LD_PLUGIN_NAME;
	IPluginManager& PluginManager = IPluginManager::Get();

	const TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
	if (Plugin->IsEnabled())
	{
		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();

		USMProjectEditorSettings* ProjectEditorSettings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
		if (ProjectEditorSettings->InstalledVersion != Descriptor.VersionName)
		{
			const bool bIsUpdate = ProjectEditorSettings->InstalledVersion.Len() > 0;

			const FString OldVersion = ProjectEditorSettings->InstalledVersion;
			ProjectEditorSettings->InstalledVersion = Descriptor.VersionName;
			ProjectEditorSettings->SaveConfig();

			if (!bIsUpdate)
			{
				return;
			}

			FSMVersionUtils::UpdateProjectToNewVersion(OldVersion);

			if (ProjectEditorSettings->bDisplayUpdateNotification && FApp::CanEverRender())
			{
				DisplayUpdateNotification(Descriptor, bIsUpdate);
			}
		}
	}
}

void FSMSystemEditorModule::DisplayUpdateNotification(const FPluginDescriptor& Descriptor, bool bIsUpdate)
{
	TArray<FString> PreviousInstalledPlugins;
	GConfig->GetArray(TEXT("PluginBrowser"), TEXT("InstalledPlugins"), PreviousInstalledPlugins, GEditorPerProjectIni);

	if (PreviousInstalledPlugins.Contains(LD_PLUGIN_NAME))
	{
		// We only want to display the popup if the plugin was previously installed. Not always accurate so we check if there was a previous version.
		const FSMVersionUtils::FVersion Version(Descriptor.VersionName);
		const FString FriendlyVersionStr = Version.ToString(false);
		const FString DisplayString = !bIsUpdate ? FString::Printf(TEXT("Logic Driver Pro version %s installed"), *FriendlyVersionStr) :
												FString::Printf(TEXT("Logic Driver Pro updated to version %s"), *FriendlyVersionStr);
		FNotificationInfo Info(FText::FromString(DisplayString));
		Info.bFireAndForget = false;
		Info.bUseLargeFont = true;
		Info.bUseThrobber = false;
		Info.FadeOutDuration = 0.25f;
		Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("LogicDriverUpdateViewPatchNotes", "View Patch Notes..."), LOCTEXT("LogicDriverUpdateViewPatchTT", "Open the webbrowser to view patch notes"), FSimpleDelegate::CreateRaw(this, &FSMSystemEditorModule::OnViewNewPatchNotesClicked, FriendlyVersionStr)));
		Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("LogicDriverUpdatePopupDismiss", "Dismiss"), LOCTEXT("LogicDriverUpdatePopupDismissTT", "Dismiss this notification"), FSimpleDelegate::CreateRaw(this, &FSMSystemEditorModule::OnDismissUpdateNotificationClicked)));

		NewVersionNotification = FSlateNotificationManager::Get().AddNotification(Info);
		NewVersionNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void FSMSystemEditorModule::OnViewNewPatchNotesClicked(FString Version)
{
	// Strip '.' out of version.
	TArray<FString> VersionArray;
	Version.ParseIntoArray(VersionArray, TEXT("."));
	Version = FString::Join(VersionArray, TEXT(""));
	
	const FString Url = FString::Printf(TEXT("https://logicdriver.com/docs/pages/prochangelog/#version-%s"), *Version);
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
	NewVersionNotification.Pin()->ExpireAndFadeout();
}

void FSMSystemEditorModule::OnDismissUpdateNotificationClicked()
{
	NewVersionNotification.Pin()->ExpireAndFadeout();
}

void FSMSystemEditorModule::HandleModuleChanged(FName ModuleName, EModuleChangeReason ChangeReason)
{
#if LOGICDRIVER_HAS_PROPER_VARIABLE_CUSTOMIZATION
	if (ModuleName == TEXT("Kismet") && ChangeReason == EModuleChangeReason::ModuleLoaded)
	{
		if (!BlueprintVariableCustomizationHandle.IsValid())
		{
			RegisterBlueprintVariableCustomization();
		}
		FModuleManager::Get().OnModulesChanged().Remove(ModuleChangedHandle);
		ModuleChangedHandle.Reset();
	}
#endif
}

IMPLEMENT_MODULE(FSMSystemEditorModule, SMSystemEditor)

#undef LOCTEXT_NAMESPACE
