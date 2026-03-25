/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once

#include "CoreMinimal.h"
#include "DialogGraphNode.h"
#include "SDialogGraphNode.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"


class DIALOGASSETEDITOR_API SActorNameCombo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SActorNameCombo)
	: _ParentNode(nullptr),
	_dataModel(nullptr)
	{}

	SLATE_ARGUMENT(SDialogGraphNode *, ParentNode)
	SLATE_ARGUMENT(UDialogGraphNode *, dataModel)

	SLATE_END_ARGS()

	typedef TSharedPtr<FText> FComboItemType;

	void Construct(const FArguments& InArgs)
	{
		// Récupérer l'élément initialement sélectionné ou choisir le premier par défaut
		//CurrentItem = InArgs._InitiallySelectedItem.IsValid() ? InArgs._InitiallySelectedItem : (Options.Num() > 0 ? Options[0] : nullptr);

		// Store the parent node from the InArgs :
		ParentNode = InArgs._ParentNode;
		
		// Store the datamodel from the InArgs :
		dataModel = InArgs._dataModel;

		if(dataModel != nullptr)
		{
			nodeInfo = Cast<UDialogNodeInfo>(dataModel->GetNodeInfo());
		}

		ConstructOptionsFromDatamodel();
		
		ChildSlot
			[
				SNew(SComboBox<FComboItemType>)
				.OptionsSource(&Options)
				.OnComboBoxOpening(this, &SActorNameCombo::OnComboBoxOpening)
				.OnSelectionChanged(this, &SActorNameCombo::OnSelectionChanged)
				.OnGenerateWidget(this, &SActorNameCombo::MakeWidgetForOption)
				.InitiallySelectedItem(Options[nodeInfo->ActorIdx])
				[
					SNew(STextBlock)
					.Text(this, &SActorNameCombo::GetCurrentItemLabel)
				]
			];
	}

	TSharedRef<SWidget> MakeWidgetForOption(FComboItemType InOption)
	{
		return SNew(STextBlock).Text(*InOption);
	}

	void OnSelectionChanged(FComboItemType NewValue, ESelectInfo::Type)
	{
		int idx = 0;
		if(NewValue != nullptr)
		{
			for(FComboItemType value : Options)
			{
				if((*value).EqualTo(*NewValue))
					break;
			
				idx++;
			}
			CurrentItemIdx = idx;

			if(CurrentItemIdx < Options.Num())
			{
				nodeInfo->SetActorIdx(CurrentItemIdx, *Options[CurrentItemIdx]);
				ParentNode->UpdateGraphNode();
			}
		}
	}

	FText GetCurrentItemLabel() const
	{
		if(CurrentItemIdx < Options.Num())
			return *Options[CurrentItemIdx];

		return FText::FromString("Invalid ! Please reassign !");
	}

	void ConstructOptionsFromDatamodel()
	{
		UDialogAsset * asset = Cast<UDialogAsset>(dataModel->GetAsset());
		if(asset != nullptr)
		{
			Options.Empty(asset->DialogActors.Num() + 1);

			Options.Add(MakeShareable(new FText(asset->PlayerActor.ActorIdentifier)));
			
			for(FActorInfo actor : asset->DialogActors)
			{
				Options.Add(MakeShareable(new FText(actor.ActorIdentifier)));
			}
			
			CurrentItemIdx = nodeInfo->ActorIdx;
		}
	}
	
	void OnComboBoxOpening()
	{
		ConstructOptionsFromDatamodel();
	}
	
private:
	int CurrentItemIdx;
	TArray<FComboItemType> Options;
	SDialogGraphNode * ParentNode = nullptr;
	UDialogGraphNode * dataModel = nullptr;
	UDialogNodeInfo * nodeInfo = nullptr;
};
