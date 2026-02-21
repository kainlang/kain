// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMNodeWidgetInfoCustomization.h"

#include "SMNodeWidgetInfo.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakFieldPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

TSharedRef<IPropertyTypeCustomization> FSMNodeWidgetInfoCustomization::MakeInstance()
{
	return MakeShared<FSMNodeWidgetInfoCustomization>();
}

void FSMNodeWidgetInfoCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
                                                 FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FSMStructCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);

	if (StructCustomizationUtils.GetPropertyUtilities() != nullptr)
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized = StructCustomizationUtils.GetPropertyUtilities()->GetSelectedObjects();
		if (ObjectsBeingCustomized.Num() > 0)
		{
			UPropertyWrapper* PropertyWrapper = Cast<UPropertyWrapper>(ObjectsBeingCustomized[0].Get());
			const TWeakFieldPtr<FProperty> PropertyBeingCustomized = PropertyWrapper ? PropertyWrapper->GetProperty() : nullptr;
			if (PropertyBeingCustomized.IsValid())
			{
				OwnerField = PropertyBeingCustomized.Get();
			}
		}
	}
	
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			StructPropertyHandle->CreatePropertyValueWidget()
		];
}

void FSMNodeWidgetInfoCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Check if child configuration properties are allowed to be displayed for the owning property. 
	auto CanShowChildProperty = [this] (const TSharedPtr<IPropertyHandle>& InChildHandle)
	{
		if (InChildHandle->GetProperty() && InChildHandle->GetProperty()->GetFName() ==
			GET_MEMBER_NAME_CHECKED(FSMNodeWidgetInfo, DisplayThumbnail))
		{
			// DisplayThumbnail should reflect pin overrides under FSMObjectPinFactory::CreatePin
			const TArray<const FField*> Fields = GetOwnerFields();
			for (const FField* Field : Fields)
			{
				// Verify base classes
				const FFieldClass* FieldClass = Field->GetClass();
				if (FieldClass
					&& (FieldClass->IsChildOf(FObjectPropertyBase::StaticClass())
						|| FieldClass->IsChildOf(FInterfaceProperty::StaticClass())))
				{
					// Exclude actor based classes
					const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Field);
					const FClassProperty* ClassProperty = CastField<FClassProperty>(Field);
					const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Field);
					if (!(ObjectProperty
						&& ObjectProperty->PropertyClass
						&& ObjectProperty->PropertyClass->IsChildOf(AActor::StaticClass()))
						&& !(ClassProperty
							&& ClassProperty->MetaClass
							&& ClassProperty->MetaClass->IsChildOf(AActor::StaticClass()))
							&& !(SoftClassProperty
								&& SoftClassProperty->MetaClass
								&& SoftClassProperty->MetaClass->IsChildOf(AActor::StaticClass())))
					{
						return true;
					}
				}
			}

			return false;
		}

		return true;
	};
	
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);
	
	for (uint32 ChildNum = 0; ChildNum < NumChildren; ++ChildNum)
	{
		const TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildNum);
		if (!ChildHandle.IsValid() || !CanShowChildProperty(ChildHandle))
		{
			continue;
		}
		ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}

TArray<const FField*> FSMNodeWidgetInfoCustomization::GetOwnerFields() const
{
	TArray<const FField*> Fields;
			
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(OwnerField))
	{
		TArray<FField*> InnerFields;
		ArrayProperty->GetInnerFields(InnerFields);
		Fields.Append(MoveTemp(InnerFields));
	}
	else if (OwnerField != nullptr)
	{
		Fields.Add(OwnerField);
	}

	return Fields;
}

