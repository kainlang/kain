// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SSMSupportDialog.h"

#include "SMSystemInfo.h"

#include "SMUnrealTypeDefs.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSMSupportDialog"

void SSMSupportDialog::Construct(const FArguments& InArgs, const TSharedPtr<FSMSystemInfo>& InSupportInfo)
{
	SystemInfo = InSupportInfo;
	check(SystemInfo.IsValid());

	constexpr float HorizontalOffset = 4.f;
	constexpr float VerticalOffset = 4.f;
	
	SWindow::Construct(SWindow::FArguments()
	.Title(LOCTEXT("LogicDriverSystemInfo", "Logic Driver System Info"))
	.SizingRule(ESizingRule::Autosized)
	.SupportsMaximize(false)
	.SupportsMinimize(false)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0.f, 8.f, 0.f, 0.f)
		.HAlign(HAlign_Fill)
		[
			SNew(SGridPanel)
			+SGridPanel::Slot(0, 0)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Platform", "Platform"))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]
			+SGridPanel::Slot(1, 0)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(FText::FromString(SystemInfo->OSVersion))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]

			+SGridPanel::Slot(0, 1)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EngineVersion", "Engine Version"))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]
			+SGridPanel::Slot(1, 1)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(FText::FromString(SystemInfo->EngineVersion))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]

			+SGridPanel::Slot(0, 2)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PluginVersion", "Plugin Version"))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]
			+SGridPanel::Slot(1, 2)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(FText::FromString(SystemInfo->PluginVersion))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]

			+SGridPanel::Slot(0, 3)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BuildType", "Build Type"))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]
			+SGridPanel::Slot(1, 3)
			.Padding(FMargin(HorizontalOffset, VerticalOffset))
			[
				SNew(STextBlock)
				.Text(FText::FromString(SystemInfo->GetBuildType()))
				.Font(FSMUnrealAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(0.f, 8.f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("CopyButton_Text", "Copy"))
				.ToolTipText(LOCTEXT("CopyButton_Tooltip", "Copy system information to the clipboard."))
				.OnClicked(this, &SSMSupportDialog::OnCopyClicked)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("CancelButton_Text", "Cancel"))
				.ToolTipText(LOCTEXT("CancelButton_Tooltip", "Close the window."))
				.OnClicked_Lambda([this]()
				{
					RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		]
	]);
}

FReply SSMSupportDialog::OnCopyClicked()
{
	check(SystemInfo.IsValid());
	
	FPlatformApplicationMisc::ClipboardCopy(*SystemInfo->ToString());
	
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
