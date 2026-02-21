// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMContentItemViewModel.h"

#include "InputMappingContext.h"
#include "Engine/Texture2D.h"

USMContentProjectConfigurationViewModel::USMContentProjectConfigurationViewModel()
{
}

void USMContentProjectConfigurationViewModel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	static FName ActionMappingPropertyName = GET_MEMBER_NAME_CHECKED(USMContentProjectConfigurationViewModel, ActionMapping);

	if (PropertyChangedEvent.GetMemberPropertyName() == ActionMappingPropertyName || PropertyChangedEvent.GetPropertyName() == ActionMappingPropertyName)
	{
		bInputModified = true;
	}
}

FSMContentItemViewModel::FSMContentItemViewModel(USMContentAsset* InContentAsset): Name(InContentAsset->Name)
                                                                                   , Author(InContentAsset->Author)
                                                                                   , Description(InContentAsset->Description)
                                                                                   , DetailedDescription(InContentAsset->DetailedDescription)
                                                                                   , ContentAsset(InContentAsset)
{
	check(ContentAsset.IsValid());

	if (ContentAsset->TitleImage)
	{
		// If image was just loaded it needs to finish.
		ContentAsset->TitleImage->FinishCachePlatformData();
		ContentAsset->TitleImage->UpdateResource();
	}

	if (ContentAsset->DescriptionImage)
	{
		// If image was just loaded it needs to finish.
		ContentAsset->DescriptionImage->FinishCachePlatformData();
		ContentAsset->DescriptionImage->UpdateResource();
	}

	TitleBrush.SetResourceObject(ContentAsset->TitleImage);
	TitleBrush.DrawAs = ESlateBrushDrawType::Image;
	TitleBrush.SetImageSize(FVector2D(104.f, 304.f));

	DescriptionBrush.SetResourceObject(ContentAsset->DescriptionImage);
	DescriptionBrush.DrawAs = ESlateBrushDrawType::Image;
	DescriptionBrush.SetImageSize(FVector2D(576.f, 276.f));

	ContentType = UEnum::GetDisplayValueAsText(ContentAsset->ContentType);
	
	ProjectConfigurationViewModel = TStrongObjectPtr<USMContentProjectConfigurationViewModel>(NewObject<USMContentProjectConfigurationViewModel>());
	ProjectConfigurationViewModel->FileCount = ContentAsset->GetPackagedFiles().Num();

	ProjectConfigurationViewModel->ActionMapping = InContentAsset->GetInputActionsMapping();
}

const FSlateBrush* FSMContentItemViewModel::GetTitleBrush() const
{
	return &TitleBrush;
}

const FSlateBrush* FSMContentItemViewModel::GetDescriptionBrush() const
{
	return &DescriptionBrush;
}

FLinearColor FSMContentItemViewModel::GetTypeColor() const
{
	static FLinearColor DefaultColor = FLinearColor(1, 1, 1, .9f);
	
	if (ContentAsset.IsValid())
	{
		switch(ContentAsset->ContentType)
		{
		case ESMContentType::Example:
			return FLinearColor(0.930f, 0.449f, 0.0558f);
		case ESMContentType::Template:
			return FLinearColor(0.0558, 0.639, 0.930);
		default:
			return DefaultColor;
		}
	}

	return DefaultColor;
}

bool FSMContentItemViewModel::IsInstalledToPluginContent() const
{
	return ContentAsset.IsValid() && ContentAsset->bInstallToPluginContent;
}
