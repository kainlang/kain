// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "ViewModels/SMContentItemViewModel.h"

#include "CoreMinimal.h"
#include "SGraphPanel.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Widgets/SWindow.h"
#include "Widgets/Views/STileView.h"

class SSMFilePath;
class SSMProjectConfigurationView;

class SSMContentSelectView : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSMContentSelectView)
	{}
	SLATE_END_ARGS()

	SSMContentSelectView() = default;
	virtual ~SSMContentSelectView() override;
	
	void Construct(const FArguments& InArgs, const TArray<TSharedPtr<FSMContentItemViewModel>>& InContentItems);
	bool GetUserConfirmedSelection() const;
	bool GetUserCreatedNew() const;

	TSharedPtr<FSMContentItemViewModel> GetSelectedContentViewModel() const;
	USMContentProjectConfigurationViewModel* GetSelectedProjectConfigurationViewModel() const;

protected:
	void ConfirmSelection();
	bool IsDestinationPathValid() const;

	void FilterContent();

	EVisibility GetSelectedContentImageVisibility() const;
	const FSlateBrush* GetSelectedContentImage() const;
	FText GetSelectedContentName() const;
	FText GetSelectedContentDetailedDescription() const;
	FText GetSelectedContentInstallationPath() const;

private:
	/** Creates a row in the content list. */
	TSharedRef<ITableRow> MakeContentViewWidget(TSharedPtr<FSMContentItemViewModel> ContentItem, const TSharedRef<STableViewBase>& OwnerTable);

	void OnFilterTextChanged(const FText& SearchText);

	/** Called when the selection changes in the content list. */
	void OnContentViewSelectionChanged(TSharedPtr<FSMContentItemViewModel> ContentItem, ESelectInfo::Type SelectInfo);

	ECheckBoxState GetOpenMapOnInstallCheckState() const;
	bool DoesContentHaveSampleMap() const;
	void OnOpenMapCheckStateChanged(ECheckBoxState NewState);

	bool IsInstallButtonEnabled() const;
	void OnInstallButtonClicked();
	void OnCancelButtonClicked();

	void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);

private:
	TSharedPtr<STileView<TSharedPtr<FSMContentItemViewModel>>> ContentTileView;
	TArray<TSharedPtr<FSMContentItemViewModel>> ContentItems;
	TArray<TSharedPtr<FSMContentItemViewModel>> FilteredContentItems;
	TSharedPtr<FSMContentItemViewModel> SelectedContent;
	TSharedPtr<SSMProjectConfigurationView> ProjectConfigurationBindingView;
	TSharedPtr<SSMFilePath> FilePath;
	mutable FText DestinationPathErrorMessage;

	bool bUserConfirmedSelection = false;
	bool bCreateNewSelected = false;
	bool bShowPluginContent = false;
};
