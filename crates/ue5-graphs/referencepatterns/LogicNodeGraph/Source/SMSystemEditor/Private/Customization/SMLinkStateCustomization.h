// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMEditorCustomization.h"

class USMGraphNode_StateNodeBase;

class FSMLinkStateCustomization : public FSMNodeCustomization {
public:
	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// ~IDetailCustomization

	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	TArray<TSharedPtr<FString>> AvailableStateNames;
};
