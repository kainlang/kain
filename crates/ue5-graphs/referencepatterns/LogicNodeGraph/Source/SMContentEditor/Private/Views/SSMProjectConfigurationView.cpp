// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMProjectConfigurationView.h"

#include "ViewModels/SMContentItemViewModel.h"

#include "IDetailsView.h"
#include "SlateOptMacros.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"

SSMProjectConfigurationView::~SSMProjectConfigurationView()
{
	PropertyView.Reset();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SSMProjectConfigurationView::Construct(const FArguments& InArgs)
{
	ViewModel = InArgs._ViewModel.Get();
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FNotifyHook* NotifyHook = this;
	FDetailsViewArgs DetailsViewArgs;
	
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = NotifyHook;
	DetailsViewArgs.bSearchInitialKeyFocus = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.ViewIdentifier = NAME_None;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.ColumnWidth = .3f; // Otherwise Key binding name is too small.

	PropertyView = EditModule.CreateDetailView(DetailsViewArgs);

	ChildSlot
	[
		PropertyView.ToSharedRef()
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SSMProjectConfigurationView::SetDetailsObject(UObject* InObject)
{
	PropertyView->SetObject(InObject);
}
