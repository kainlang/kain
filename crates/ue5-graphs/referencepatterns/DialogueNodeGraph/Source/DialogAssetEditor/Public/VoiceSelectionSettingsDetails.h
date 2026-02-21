/*    Copyright (c) 2024, Alexandre GREMONT@Overwhelmed Guys Studio
 *    All rights reserved
 *
 *    This file is part of the Dialogue Master plugin for Unreal Engine
 *    and is licensed under the Unreal Engine EULA:
 *    https://www.unrealengine.com/en-US/eula/publishing
 */

#pragma once
#include "IDetailCustomization.h"

class FVoiceSelectionSettingsDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	FVoiceSelectionSettingsDetails();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
private:
	static USoundWave* CreateSoundWaveFromResponse(const TArray<uint8>& AudioData);
	//TArray<TSharedPtr<FString>> VoiceOptions;
	
	/*
	void OnVoiceSelected(TSharedPtr<FString> NewValue, ESelectInfo::Type);
	TSharedPtr<FString> GetSelectedVoice() const;
	TArray<TSharedPtr<FString>> VoiceOptions;

	TWeakObjectPtr<UObject> EditedObject;
	*/
};
