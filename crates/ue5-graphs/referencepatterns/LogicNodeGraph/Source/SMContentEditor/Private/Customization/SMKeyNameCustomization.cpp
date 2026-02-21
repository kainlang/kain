// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMKeyNameCustomization.h"

#include "Assets/SMContentAsset.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "SKeySelector.h"

void FSMKeyNameCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
                                              IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FSMKeyNameCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> KeysHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSMInputActionWrapper, Keys));
	check(KeysHandle.IsValid());

	const TSharedPtr<IPropertyHandleArray> ArrayHandle = KeysHandle->AsArray();
	check(ArrayHandle.IsValid());

	uint32 NumElements = 0;
	ArrayHandle->GetNumElements(NumElements);

	for (uint32 Idx = 0; Idx < NumElements; ++Idx)
	{
		const TSharedRef<IPropertyHandle> ElementHandle = ArrayHandle->GetElement(Idx);

		FDetailWidgetRow& HeaderRow = ChildBuilder.AddProperty(ElementHandle).CustomWidget();

		const TSharedPtr<SKeySelector> KeySelector = SNew(SKeySelector)
			.CurrentKey(this, &FSMKeyNameCustomization::GetCurrentKey, ElementHandle)
			.Font(CustomizationUtils.GetRegularFont())
			.AllowClear(!ElementHandle->GetProperty()->HasAnyPropertyFlags(CPF_NoClear))
			.FilterBlueprintBindable(false)
			.IsEnabled(false);

		HeaderRow.NameContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(325.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				KeySelector.ToSharedRef()
			]
		];

		HeaderRow.OverrideResetToDefault(FResetToDefaultOverride::Hide());
	}
}

TOptional<FKey> FSMKeyNameCustomization::GetCurrentKey(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	TArray<void*> StructPtrs;
	PropertyHandle->AccessRawData(StructPtrs);

	if (StructPtrs.Num() > 0)
	{
		if (FKey* SelectedKey = static_cast<FKey*>(StructPtrs[0]))
		{
			for(int32 StructPtrIndex = 1; StructPtrIndex < StructPtrs.Num(); ++StructPtrIndex)
			{
				if (*static_cast<FKey*>(StructPtrs[StructPtrIndex]) != *SelectedKey)
				{
					return TOptional<FKey>();
				}
			}

			return *SelectedKey;
		}
	}

	return FKey();
}
