// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Utilities/SMPropertyUtils.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Utilities/SMBlueprintEditorUtils.h"

#include "SMTypes.h"
#include "SMUtils.h"

#include "ISinglePropertyView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"

TSharedPtr<ISinglePropertyView> LD::PropertyUtils::CreatePropertyViewForProperty(UObject* InObjectOwner,
	const FName& InPropertyName)
{
	check(InObjectOwner);

	const FSinglePropertyParams InitParams;
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedPtr<ISinglePropertyView> PropertyView = PropertyEditorModule.CreateSingleProperty(
		InObjectOwner,
		InPropertyName,
		InitParams);

	check(PropertyView.IsValid());
	return PropertyView;
}

/** Sets a single property value with no array handling. */
void SetSinglePropertyValueImpl(const FProperty* InProperty, const FString& InValue, uint8* Container,
                                UObject* InObject)
{
	if (!FBlueprintEditorUtils::PropertyValueFromString_Direct(InProperty, InValue, Container, InObject))
	{
		// Fallback to generic import, don't log as this is common when adding variables and not changing their defaults.
		InProperty->ImportText_Direct(*InValue, Container, InObject, PPF_SerializedAsImportText);
	}
}

void LD::PropertyUtils::SetPropertyValue(FProperty* InProperty, const FString& InValue, UObject* InObject,
                                         int32 InArrayIndex)
{
	if (InObject)
	{
		InObject->Modify();
	}

	// The final property that will have its value set.
	FProperty* PropertyToSet = InProperty;

	// The owner of the container which could be the object or the container if an array.
	uint8* ContainerOwner = reinterpret_cast<uint8*>(InObject);

	// The immediate container of the property.
	uint8* Container = nullptr;

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<uint8>(InObject));
		if (Helper.IsValidIndex(InArrayIndex))
		{
			PropertyToSet = ArrayProperty->Inner;
			Container = Helper.GetRawPtr(InArrayIndex);
			ContainerOwner = Container;
		}
	}
	else
	{
		Container = InProperty->ContainerPtrToValuePtr<uint8>(ContainerOwner, InArrayIndex);
	}

	// Check for extended graph property.
	if (FProperty* ExtendGraphResultProperty = GetExtendedGraphPropertyResult(PropertyToSet, ContainerOwner))
	{
		Container = ExtendGraphResultProperty->ContainerPtrToValuePtr<uint8>(Container);
		PropertyToSet = ExtendGraphResultProperty;
	}

	if (ensure(Container))
	{
		SetSinglePropertyValueImpl(PropertyToSet, InValue, Container, InObject);
	}
}

FString LD::PropertyUtils::GetPropertyValue(FProperty* InProperty, UObject* InObject, int32 InArrayIndex)
{
	// The final property that will have its value returned.
	FProperty* PropertyToGet = InProperty;

	// The owner of the container which could be the object or the container if an array.
	uint8* ContainerOwner = reinterpret_cast<uint8*>(InObject);

	// The immediate container of the property.
	uint8* Container = nullptr;

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<uint8>(InObject));
		if (!(ensureMsgf(Helper.IsValidIndex(InArrayIndex),
		                 TEXT("Invalid array index given to GetPropertyValue for array %s."), *InProperty->GetName())))
		{
			return FString();
		}

		PropertyToGet = ArrayProperty->Inner;
		Container = Helper.GetRawPtr(InArrayIndex);
		ContainerOwner = Container;
	}
	else
	{
		Container = InProperty->ContainerPtrToValuePtr<uint8>(ContainerOwner, InArrayIndex);
	}

	// Check for extended graph property.
	if (FProperty* ExtendGraphResultProperty = GetExtendedGraphPropertyResult(PropertyToGet, ContainerOwner))
	{
		Container = ExtendGraphResultProperty->ContainerPtrToValuePtr<uint8>(Container);
		PropertyToGet = ExtendGraphResultProperty;
	}

	FString Result;
	if (ensure(Container))
	{
		FBlueprintEditorUtils::PropertyValueToString_Direct(PropertyToGet, Container, Result, InObject);
	}

	return MoveTemp(Result);
}

FProperty* LD::PropertyUtils::GetExtendedGraphPropertyResult(FProperty* InProperty, uint8* InContainer)
{
	if (const FStructProperty* StructProperty = GetGraphPropertyFromProperty(InProperty))
	{
		check(InContainer);

		// Access the graph property instance so can find the virtual result property name.
		const FSMGraphProperty_Base* GraphProperty = InProperty->ContainerPtrToValuePtr<FSMGraphProperty_Base>(InContainer);
		check(GraphProperty);

		const FName ResultPropertyName = GraphProperty->GetResultPropertyName();
		if (!ResultPropertyName.IsNone())
		{
			// A result property indicates this is a custom graph property that has a sub property managing the value.
			FProperty* ResultProperty = StructProperty->Struct->FindPropertyByName(ResultPropertyName);
			check(ResultProperty);

			return ResultProperty;
		}
	}

	return nullptr;
}

bool LD::PropertyUtils::IsObjectPropertyInstanced(const FObjectProperty* ObjectProperty)
{
	return ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ExportObject);
}

UObject* LD::PropertyUtils::FPropertyRetrieval::GetObjectValue() const
{
	if (ObjectProperty && ObjectContainer)
	{
		return ObjectProperty->GetObjectPropertyValue(ObjectContainer);
	}
	return nullptr;
}

void LD::PropertyUtils::FPropertyRetrieval::SetObjectValue(UObject* NewValue) const
{
	if (ObjectProperty && ObjectContainer)
	{
		return ObjectProperty->SetObjectPropertyValue(const_cast<void*>(ObjectContainer), NewValue);
	}
}

namespace LD
{
	namespace PropertyUtils
	{
		void GetAllObjectPropertiesImpl(const void* InObject, const UStruct* InPropertySource,
		                                TArray<FPropertyRetrieval>& OutProperties,
		                                TSet<const void*>* ObjectsChecked, const FPropertyRetrievalArgs& InArgs)
		{
			if (!InObject || ObjectsChecked->Contains(InObject))
			{
				return;
			}

			ObjectsChecked->Add(InObject);

			auto ProcessProperty = [&](FProperty* Property, const void* Object)
			{
				if (!Object)
				{
					return;
				}

				ObjectsChecked->Add(Object);

				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					const void* StructAddressInObject = StructProperty->ContainerPtrToValuePtr<const void>(Object);
					GetAllObjectPropertiesImpl(StructAddressInObject, StructProperty->Struct, OutProperties,
					                           ObjectsChecked, InArgs);
				}
				else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					// Check for any filters.
					if (InArgs.IncludePropertyFlags != CPF_None && !Property->HasAllPropertyFlags(InArgs.IncludePropertyFlags))
					{
						return;
					}
					if (InArgs.ExcludePropertyFlags != CPF_None && Property->HasAnyPropertyFlags(InArgs.ExcludePropertyFlags))
					{
						return;
					}

					const void* ObjectContainer = ObjectProperty->ContainerPtrToValuePtr<const void>(Object);
					const FPropertyRetrieval PropertyRetrieval{ObjectProperty, ObjectContainer};
					OutProperties.Add(PropertyRetrieval);

					if (IsObjectPropertyInstanced(ObjectProperty))
					{
						// Only check property instances stored within this object.
						if (const UObject* ObjectValue = PropertyRetrieval.GetObjectValue())
						{
							GetAllObjectPropertiesImpl(ObjectValue, ObjectValue->GetClass(), OutProperties,
							                           ObjectsChecked, InArgs);
						}
					}
				}
			};

			for (TFieldIterator<FProperty> PropertyIterator(InPropertySource); PropertyIterator; ++PropertyIterator)
			{
				FProperty* Property = *PropertyIterator;

				if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty,
					                               ArrayProperty->ContainerPtrToValuePtr<void>(InObject));
					for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
					{
						const uint8* ArrayData = ArrayHelper.GetRawPtr(Index);
						ProcessProperty(ArrayProperty->Inner, ArrayData);
					}
				}
				/*
				else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
				{
					// TODO: MapProperty
				}
				else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
				{
					// TODO: SetProperty
				}*/
				else
				{
					ProcessProperty(Property, InObject);
				}
			}
		}
	}
}

void LD::PropertyUtils::GetAllObjectProperties(const void* InObject, const UStruct* InPropertySource,
                                               TArray<FPropertyRetrieval>& OutProperties,
                                               const FPropertyRetrievalArgs& InArgs)
{
	TSet<const void*> ObjectsCheckedSource;
	GetAllObjectPropertiesImpl(InObject, InPropertySource, OutProperties, &ObjectsCheckedSource, InArgs);
}

void LD::PropertyUtils::ForEachInstancedSubObject(const UObject* InObject, const TFunction<void(UObject*)>& Function)
{
	// Verify there are sub objects first before finding properties. This saves a call to GetAllObjectProperties which
	// is much slower.
	bool bHasSubObjects = false;
	ForEachObjectWithOuter(InObject, [&](const UObject* Child)
	{
		if (IsValid(Child))
		{
			bHasSubObjects = true;
		}
	});

	if (bHasSubObjects)
	{
		TArray<FPropertyRetrieval> ObjectProperties;
		FPropertyRetrievalArgs Args;
		Args.IncludePropertyFlags = CPF_InstancedReference;
		Args.ExcludePropertyFlags = CPF_Transient;
		GetAllObjectProperties(InObject, InObject->GetClass(),
		                       ObjectProperties, MoveTemp(Args));
		for (FPropertyRetrieval& PropertyRetrieval : ObjectProperties)
		{
			if (!PropertyRetrieval.ObjectProperty ||
				!ensure(!PropertyRetrieval.ObjectProperty->HasAnyPropertyFlags(CPF_Transient)))
			{
				continue;
			}

			if (UObject* ObjectValue = PropertyRetrieval.GetObjectValue())
			{
				Function(ObjectValue);
			}
		}
	}
}

const FGuid& LD::PropertyUtils::SetGraphPropertyFromProperty(FSMGraphProperty_Base& GraphProperty, FProperty* Property,
                                                             const USMNodeInstance* NodeInstance, int32 Index,
                                                             bool bSetGuid, bool bUseTemplateInGuid,
                                                             bool bUseTempNativeGuid)
{
	check(NodeInstance)
	check(Property);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	GraphProperty.bIsInArray = Property->IsA<FArrayProperty>() || Property->GetOwnerProperty()->IsA<FArrayProperty>();

	GraphProperty.VariableName = Property->GetFName();
	GraphProperty.MemberReference.SetFromField<FProperty>(Property, false);
	K2Schema->ConvertPropertyToPinType(Property, GraphProperty.VariableType);

	// TemplateGuid is used to calculate final guid.
	GraphProperty.SetTemplateGuid(NodeInstance->GetTemplateGuid());

	if (!bSetGuid)
	{
		return GraphProperty.GetGuid();
	}

	if (GraphProperty.MemberReference.GetMemberGuid().IsValid())
	{
		// Blueprint variable
		return GraphProperty.SetGuid(GraphProperty.MemberReference.GetMemberGuid(), Index, bUseTemplateInGuid);
	}

	// Search string Taken from FMemberReference::GetReferenceSearchString of engine CL 17816129.
	auto GetTempNativeSearchString = [&](UClass* InFieldOwner)
	{
		const FGuid MemberGuid = GraphProperty.MemberReference.GetMemberGuid();
		const FName MemberName = GraphProperty.MemberReference.GetMemberName();
		if (!GraphProperty.MemberReference.IsLocalScope())
		{
			if (InFieldOwner)
			{
				if (MemberGuid.IsValid())
				{
					return FString::Printf(
						TEXT(
							"Nodes(VariableReference(MemberName=+\"%s\" && MemberGuid(A=%i && B=%i && C=%i && D=%i)) || Name=\"(%s)\") || Pins(Binding=\"%s\") || Binding=\"%s\""),
						*MemberName.ToString(), MemberGuid.A, MemberGuid.B, MemberGuid.C, MemberGuid.D,
						*MemberName.ToString(), *MemberName.ToString(), *MemberName.ToString());
				}
				else
				{
					FString ExportMemberParentName = InFieldOwner->GetClass()->GetName();
					ExportMemberParentName.AppendChar('\'');
					ExportMemberParentName += InFieldOwner->GetAuthoritativeClass()->GetPathName();
					ExportMemberParentName.AppendChar('\'');

					return FString::Printf(
						TEXT(
							"Nodes(VariableReference(MemberName=+\"%s\" && (MemberParent=\"%s\" || bSelfContext=true) ) || Name=\"(%s)\") || Pins(Binding=\"%s\") || Binding=\"%s\""),
						*MemberName.ToString(), *ExportMemberParentName, *MemberName.ToString(), *MemberName.ToString(),
						*MemberName.ToString());
				}
			}
			else if (MemberGuid.IsValid())
			{
				return FString::Printf(
					TEXT(
						"Nodes(VariableReference(MemberName=+\"%s\" && MemberGuid(A=%i && B=%i && C=%i && D=%i)) || Name=\"(%s)\") || Pins(Binding=\"%s\") || Binding=\"%s\""),
					*MemberName.ToString(), MemberGuid.A, MemberGuid.B, MemberGuid.C, MemberGuid.D,
					*MemberName.ToString(), *MemberName.ToString(), *MemberName.ToString());
			}
			else
			{
				return FString::Printf(
					TEXT(
						"Nodes(VariableReference(MemberName=+\"%s\") || Name=\"(%s)\") || Pins(Binding=\"%s\") || Binding=\"%s\""),
					*MemberName.ToString(), *MemberName.ToString(), *MemberName.ToString(), *MemberName.ToString());
			}
		}
		else
		{
			return FString::Printf(
				TEXT("Nodes(VariableReference((MemberName=+\"%s\" && MemberScope=+\"%s\"))) || Binding=\"%s\""),
				*MemberName.ToString(), *GraphProperty.MemberReference.GetMemberScopeName(), *MemberName.ToString());
		}
	};

	// Previous search string, from 4.27 & below. This is what is used currently.
	auto GetNativeSearchString = [&](UClass* InFieldOwner)
	{
		const FGuid MemberGuid = GraphProperty.MemberReference.GetMemberGuid();
		const FName MemberName = GraphProperty.MemberReference.GetMemberName();
		if (!GraphProperty.MemberReference.IsLocalScope())
		{
			if (InFieldOwner)
			{
				if (MemberGuid.IsValid())
				{
					return FString::Printf(
						TEXT(
							"Nodes(VariableReference(MemberName=+\"%s\" && MemberGuid(A=%i && B=%i && C=%i && D=%i) ))"),
						*MemberName.ToString(), MemberGuid.A, MemberGuid.B, MemberGuid.C, MemberGuid.D);
				}
				else
				{
					FString ExportMemberParentName = InFieldOwner->GetClass()->GetName();
					ExportMemberParentName.AppendChar('\'');
					ExportMemberParentName += InFieldOwner->GetAuthoritativeClass()->GetPathName();
					ExportMemberParentName.AppendChar('\'');

					return FString::Printf(
						TEXT(
							"Nodes(VariableReference(MemberName=+\"%s\" && (MemberParent=\"%s\" || bSelfContext=true) ))"),
						*MemberName.ToString(), *ExportMemberParentName);
				}
			}
			else if (MemberGuid.IsValid())
			{
				return FString::Printf(
					TEXT("Nodes(VariableReference(MemberName=+\"%s\" && MemberGuid(A=%i && B=%i && C=%i && D=%i)))"),
					*MemberName.ToString(), MemberGuid.A, MemberGuid.B, MemberGuid.C, MemberGuid.D);
			}
			else
			{
				return FString::Printf(TEXT("Nodes(VariableReference(MemberName=+\"%s\"))"), *MemberName.ToString());
			}
		}
		else
		{
			return FString::Printf(
				TEXT("Nodes(VariableReference(MemberName=+\"%s\" && MemberScope=+\"%s\"))"), *MemberName.ToString(),
				*GraphProperty.MemberReference.GetMemberScopeName());
		}
	};

	//  Native variable.
	const FString SearchString = bUseTempNativeGuid
		                             ? GetTempNativeSearchString(Property->GetOwnerClass())
		                             : GetNativeSearchString(Property->GetOwnerClass());
	return GraphProperty.SetGuid(USMUtils::PathToGuid(SearchString), Index, bUseTemplateInGuid);
}

bool LD::PropertyUtils::IsPropertyExposedToGraphNode(const FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

	if (const UScriptStruct* OwnerStruct = Cast<UScriptStruct>(Property->GetOwnerStruct()))
	{
		// Properties that belong to a struct are never displayed on the node.
		return false;
	}

	return !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance) && Property->HasAllPropertyFlags(CPF_BlueprintVisible)
		&& !Property->HasMetaData(TEXT("HideOnNode"));
}

bool LD::PropertyUtils::IsPropertyHandleExposedContainer(const TSharedPtr<IPropertyHandle>& InHandle)
{
	// TODO Containers: If supporting maps or sets this needs to be updated.
	check(InHandle.IsValid());
	return InHandle->AsArray().IsValid() && IsPropertyExposedToGraphNode(InHandle->GetProperty());
}

FStructProperty* LD::PropertyUtils::GetGraphPropertyFromProperty(FProperty* Property)
{
	if (Property->HasMetaData(TEXT("HideOnNode")))
	{
		// Assume this node never wants to be displayed.
		return nullptr;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct->IsChildOf(FSMGraphProperty_Base_Runtime::StaticStruct()))
		{
			return StructProperty;
		}
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
		{
			if (StructProperty->Struct->IsChildOf(FSMGraphProperty_Base_Runtime::StaticStruct()))
			{
				return StructProperty;
			}
		}
	}

	return nullptr;
}

bool LD::PropertyUtils::IsPropertyGraphProperty(const FProperty* Property)
{
	return GetGraphPropertyFromProperty(const_cast<FProperty*>(Property)) != nullptr;
}

namespace LD::PropertyUtils
{
	// Helper to support both meta=(TagName) and meta=(TagName=true) syntaxes
	TOptional<bool> GetTagOrBoolMetadata(const FProperty* Property, const TCHAR* TagName)
	{
		TOptional<bool> bResult;

		if (Property->HasMetaData(TagName))
		{
			bResult = true;

			const FString ValueString = Property->GetMetaData(TagName);
			if (!ValueString.IsEmpty())
			{
				if (ValueString == TEXT("true"))
				{
					bResult = true;
				}
				else if (ValueString == TEXT("false"))
				{
					bResult = false;
				}
			}
		}

		return bResult;
	}
}

bool LD::PropertyUtils::ShouldGraphPropertyDisplayThumbnail(const USMGraphK2Node_PropertyNode_Base* InGraphPropertyNode)
{
	TOptional<bool> DisplayThumbnail;

	if (InGraphPropertyNode)
	{
		// Check blueprint flags first
		if (const FSMGraphProperty_Base* GraphProperty = InGraphPropertyNode->GetPropertyNodeConst())
		{
			if (const FSMNodeWidgetInfo* WidgetInfo = GraphProperty->GetWidgetInfo())
			{
				if (WidgetInfo->DisplayThumbnail == ESMOptionalBool::IsFalse)
				{
					DisplayThumbnail = false;
				}
				else if (WidgetInfo->DisplayThumbnail == ESMOptionalBool::IsTrue)
				{
					DisplayThumbnail = true;
				}
			}
		}

		// Check native metadata
		if (!DisplayThumbnail.IsSet())
		{
			if (const FProperty* Property = InGraphPropertyNode->GetProperty())
			{
				DisplayThumbnail = GetTagOrBoolMetadata(Property, TEXT("DisplayThumbnail"));
			}
		}
	}

	return DisplayThumbnail.Get(FSMBlueprintEditorUtils::GetProjectEditorSettings()->bDisplayThumbnailsByDefault);
}
