// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMEditorCustomization.h"

class FSMTransitionEdgeCustomization : public FSMNodeCustomization {
public:
	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// ~IDetailCustomization

	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	TArray<TSharedPtr<FString>> AvailableDelegates;
};
