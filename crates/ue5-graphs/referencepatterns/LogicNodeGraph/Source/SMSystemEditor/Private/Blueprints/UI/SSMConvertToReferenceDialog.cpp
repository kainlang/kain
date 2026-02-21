// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMConvertToReferenceDialog.h"

#include "AssetToolsModule.h"
#include "Blueprints/SMBlueprint.h"
#include "Blueprints/SMBlueprintFactory.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "SMInstance.h"
#include "SSMAssetPickerList.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMUnrealTypeDefs.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSMConvertToReferenceDialog"

void SSMConvertToReferenceDialog::Construct(const FArguments& InArgs, USMBlueprintFactory* InFactory)
{
	OriginalParentClass = InArgs._OwningInstanceClass ? InArgs._OwningInstanceClass->GetSuperClass() : nullptr;
	SelectedParentClass = OriginalParentClass;
	AssetName = InArgs._AssetName;
	AssetPath = InArgs._AssetPath;
	Factory = InFactory;
	CreateContent();
}

void SSMConvertToReferenceDialog::CreateContent()
{
	FSMBlueprintEditorUtils::GetProjectEditorSettings()->DefaultStateMachineBlueprintParentClass.LoadSynchronous();

	TSharedPtr<SSMAssetPickerList> ParentClassPicker;
	SAssignNew(ParentClassPicker, SSMAssetPickerList)
	.AssetPickerMode(SSMAssetPickerList::EAssetPickerMode::ClassPicker)
	.OnClassSelected(this, &SSMConvertToReferenceDialog::OnSelectedClassChanged)
	.InitiallySelectedClass(SelectedParentClass);

	const TSharedPtr<SVerticalBox> Content =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.f, 4.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					FString ClassName = SelectedParentClass ? SelectedParentClass->GetName() : TEXT("None");
					ClassName.RemoveFromEnd("_C");
					return FText::Format(LOCTEXT("ParentClassHeader_Text", "Parent Class: {0}"), FText::FromString(ClassName));
				})
				.ToolTipText(LOCTEXT("ParentClassHeader_Tooltip", "The parent class to use when creating the state machine reference."))
				.TextStyle(FSMUnrealAppStyle::Get(), "NormalText")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 4.f, 8.f, 8.f)
			[
				// Checkbox buttons
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f)
				[
					SNew(SCheckBox)
					.HAlign(HAlign_Center)
					.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckedState)
					{
						if (InCheckedState == ECheckBoxState::Checked)
						{
							OnSelectedClassChanged(OriginalParentClass);
							CreateContent();
						}
					})
					.IsChecked_Lambda([this]()
					{
						return SelectedParentClass == OriginalParentClass ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.IsEnabled_Lambda([this]
					{
						return SelectedParentClass != OriginalParentClass;
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UseCurrentParentButton_Text", "Use Current Parent"))
						.ToolTipText(LOCTEXT("UseCurrentParentButton_Tooltip", "Use the owning state machine's parent class."))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f)
				[
					SNew(SCheckBox)
					.HAlign(HAlign_Center)
					.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckedState)
					{
						if (InCheckedState == ECheckBoxState::Checked)
						{
							OnSelectedClassChanged(FSMBlueprintEditorUtils::GetProjectEditorSettings()->DefaultStateMachineBlueprintParentClass.Get());
							CreateContent();
						}
					})
					.IsChecked_Lambda([this]()
					{
						return SelectedParentClass == FSMBlueprintEditorUtils::GetProjectEditorSettings()->DefaultStateMachineBlueprintParentClass.Get()
						? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.IsEnabled_Lambda([this]
					{
						return SelectedParentClass != FSMBlueprintEditorUtils::GetProjectEditorSettings()->DefaultStateMachineBlueprintParentClass.Get();
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UseProjectDefaultButton_Text", "Use Project Default"))
						.ToolTipText(LOCTEXT("UseProjectDefaultButton_Tooltip", "Use the project default parent class."))
					]
				]
			]
			+ SVerticalBox::Slot()
			[
				ParentClassPicker.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Orientation(EOrientation::Orient_Horizontal)
			.Thickness(2.0f)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(0.f, 8.f)
		[
			SNew(SHorizontalBox)
			// Save/Cancel buttons
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("SaveAs_Text", "Save As"))
				.ToolTipText(LOCTEXT("SaveAs_Tooltip", "Save the new state machine asset."))
				.OnClicked(this, &SSMConvertToReferenceDialog::OnSaveAsClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("CancelButton_Text", "Cancel"))
				.ToolTipText(LOCTEXT("CancelButton_Tooltip", "Cancel converting to a reference."))
				.OnClicked_Lambda([this]()
				{
					RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		];
	
	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("ConvertToStateMachineReferenceTitle", "Convert to State Machine Reference"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(550, 475))
		.MinWidth(450.f)
		.MinHeight(450.f)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			Content.ToSharedRef()
		]);
}

void SSMConvertToReferenceDialog::OnSelectedClassChanged(UClass* InNewClass)
{
	SelectedParentClass = InNewClass ? InNewClass : USMInstance::StaticClass();
}

FReply SSMConvertToReferenceDialog::OnSaveAsClicked()
{
	if (SelectedParentClass)
	{
		Factory->SetParentClass(SelectedParentClass);
	}
	
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	NewBlueprint = Cast<USMBlueprint>(AssetTools.CreateAssetWithDialog(AssetName, AssetPath, USMBlueprint::StaticClass(), Factory));

	if (NewBlueprint)
	{
		RequestDestroyWindow();
	}
	
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
