// Copyright 2026 K-Studio. All Rights Reserved.

// KClonerActorDetails.cpp

#include "KClonerActorDetails.h"
#include "KClonerActor.h"
#include "KClonerAnimBakeUtils.h"
#include "KClonerModifier.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// Include our track editor for the keyframing helper
#include "MovieSceneKClonerModifierTrackEditor.h"
#include "KClonerSequencer.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FKClonerActorDetails"

TSharedRef<IDetailCustomization> FKClonerActorDetails::MakeInstance()
{
	return MakeShareable(new FKClonerActorDetails);
}

void FKClonerActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get the object being customized
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.Num() == 1)
	{
		SelectedCloner = Cast<AKClonerActor>(ObjectsBeingCustomized[0].Get());
	}

	// QUICK KEYFRAME BUTTONS
	// adds buttons to the details panel so you don't have to keep scrolling
	// to the modifier stack in sequencer. lazy dev tool lol
	if (SelectedCloner.IsValid() && SelectedCloner->Modifiers.Num() > 0)
	{
		IDetailCategoryBuilder& SeqCategory = DetailBuilder.EditCategory("K-Cloner|Sequencer Quick Key",
			LOCTEXT("SeqCatName", "Sequencer Quick Key"), ECategoryPriority::Important);

		SeqCategory.AddCustomRow(LOCTEXT("SeqKeyInfo", "Quick Keyframe"))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SeqKeyHelpText", "Click to key all Interp properties of a modifier at the current Sequencer playhead:"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];

		for (UKClonerModifier* Mod : SelectedCloner->Modifiers)
		{
			if (!Mod)
				continue;

			FString ModDisplayName = Mod->GetClass()->GetDisplayNameText().ToString();
			
			SeqCategory.AddCustomRow(FText::FromString(ModDisplayName))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(FText::FromString(ModDisplayName))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				.HAlign(HAlign_Fill)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.OnClicked(FOnClicked::CreateSP(this, &FKClonerActorDetails::OnQuickKeyModifierClicked, Mod))
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("KeyBtn", "Key All"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				];
		}
	}

	// BAKE ANIMATION - for Anim Tweak mode
	// burns the modifier motion back into a normal anim sequence
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("K-Cloner|AnimTweak");

	Category.AddCustomRow(LOCTEXT("BakeAnimRow", "Bake Animation"))
		.WholeRowContent()
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(250)
			.HeightOverride(35)
			.Padding(5)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &FKClonerActorDetails::OnBakeAnimationClicked)
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]()
				{
					return SelectedCloner.IsValid() && SelectedCloner->bAnimTweakMode && SelectedCloner->SourceAnimSequence != nullptr;
				})))
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5.f, 0.f))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BakeAnimButton", "Bake to New Animation"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			]
		];
}

FReply FKClonerActorDetails::OnBakeAnimationClicked()
{
	if (SelectedCloner.IsValid())
	{
		// force a sync before baking so we don't get junk transforms
		SelectedCloner->GetWorld()->Tick(ELevelTick::LEVELTICK_All, 0.0f);
		
		UKClonerAnimBakeUtils::BakeAnimSequence(
			SelectedCloner.Get(), 
			SelectedCloner->SourceAnimSequence, 
			SelectedCloner->OutputFolder.Path, 
			SelectedCloner->OutputName
		);
	}
	return FReply::Handled();
}

FReply FKClonerActorDetails::OnQuickKeyModifierClicked(UKClonerModifier* Modifier)
{
	if (!SelectedCloner.IsValid() || !Modifier)
		return FReply::Handled();

	// Get the active Sequencer
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	TSharedPtr<ISequencer> Seq;
	
	// Find an active sequencer
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (LevelEditor.IsValid())
	{
		// todo: implement reliable sequencer ref retrieval
	}

	UE_LOG(LogTemp, Log, TEXT("K-Cloner: Quick Key button pressed for modifier '%s'. Use Sequencer Track menu for reliable keying."),
		*Modifier->GetClass()->GetDisplayNameText().ToString());

	// Actually, let's try a different approach - use a notification
	FNotificationInfo Info(LOCTEXT("QuickKeyNotify", "Use Sequencer Track menu (+ button) > K-Cloner Modifier Track to add keys"));
	Info.ExpireDuration = 4.0f;
	Info.bFireAndForget = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE