// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMContentAssetFactory.h"

#include "Assets/SMContentAsset.h"
#include "Views/SSMContentSelectView.h"
#include "Utilities/SMContentEditorUtils.h"

#include "ISMSystemModule.h"

#include "K2Node_CallFunction.h"
#include "BlueprintEditorSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SMContentAssetFactory"

USMContentAssetFactory::USMContentAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = USMContentAsset::StaticClass();
}

bool USMContentAssetFactory::ConfigureProperties()
{
	TArray<TSharedPtr<FSMContentItemViewModel>> Options;

	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();

	TArray<FAssetData> ContentAssets;
	AssetRegistry.GetAssetsByClass(USMContentAsset::StaticClass()->GetClassPathName(), ContentAssets, true);

	Options.Reserve(ContentAssets.Num());
	for (const FAssetData& Asset : ContentAssets)
	{
		if (Asset.IsRedirector())
		{
			continue;
		}

		USMContentAsset* ContentAsset = CastChecked<USMContentAsset>(Asset.GetAsset());
		TSharedPtr<FSMContentItemViewModel> Option = MakeShareable(new FSMContentItemViewModel(ContentAsset));
		Options.Add(MoveTemp(Option));
	}

	const IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	const TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

	const TSharedRef<SSMContentSelectView> NewSystemDialog = SNew(SSMContentSelectView, MoveTemp(Options));
	FSlateApplication::Get().AddModalWindow(NewSystemDialog, ParentWindow);

	// User has selected a content item and install location.
	if (NewSystemDialog->GetUserConfirmedSelection())
	{
		const TSharedPtr<FSMContentItemViewModel> Selection = NewSystemDialog->GetSelectedContentViewModel();
		check(Selection.IsValid());

		const TWeakObjectPtr<USMContentAsset> SelectedAsset = Selection->GetContentAsset();
		check(SelectedAsset.IsValid());

		const FString TargetPath = Selection->InstallPath / Selection->DestinationFolder;

		LD::ContentEditorPackageUtils::InstallContent(SelectedAsset.Get(), TargetPath,
			Selection->GetProjectConfigurationViewModel(), Selection->bOpenMapOnInstall);

		return false;
	}

	return NewSystemDialog->GetUserCreatedNew();
}

UObject* USMContentAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	USMContentAsset* ContentAsset = NewObject<USMContentAsset>(InParent, Class, Name, Flags);
	return ContentAsset;
}

UObject* USMContentAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

bool USMContentAssetFactory::DoesSupportClass(UClass* Class)
{
	return Class == USMContentAsset::StaticClass();
}

#undef LOCTEXT_NAMESPACE
