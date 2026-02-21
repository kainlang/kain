// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMVariableCustomization.h"

#include "Blueprints/SMBlueprintEditor.h"
#include "Compilers/SMCompilerDelegates.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Utilities/SMPropertyUtils.h"

#include "SMNodeInstance.h"
#include "SMTransitionInstance.h"
#include "SMUtils.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "SMVariableCustomization"

TSharedPtr<IDetailCustomization> FSMVariableCustomization::MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor)
{
	const TArray<UObject*>* Objects = (InBlueprintEditor.IsValid() ? InBlueprintEditor->GetObjectsCurrentlyBeingEdited() : nullptr);
	if (Objects && Objects->Num() == 1)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>((*Objects)[0]))
		{
			if (Blueprint->ParentClass->IsChildOf(USMNodeInstance::StaticClass()) &&
				!Blueprint->ParentClass->IsChildOf(USMTransitionInstance::StaticClass()))
			{
				return MakeShareable(new FSMVariableCustomization(InBlueprintEditor, Blueprint));
			}
		}
	}

	return nullptr;
}

FSMVariableCustomization::~FSMVariableCustomization()
{
	FSMCompilerDelegates::OnNodePostCompiled.RemoveAll(this);
}

void FSMVariableCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	if (!BlueprintPtr.IsValid() || !BlueprintPtr->GeneratedClass)
	{
		return;
	}
	
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 0)
	{
		UPropertyWrapper* PropertyWrapper = Cast<UPropertyWrapper>(ObjectsBeingCustomized[0].Get());
		const TWeakFieldPtr<FProperty> PropertyBeingCustomized = PropertyWrapper ? PropertyWrapper->GetProperty() : nullptr;
		if (PropertyBeingCustomized.IsValid() && PropertyBeingCustomized->Owner.IsValid())
		{
			UClass* OwnerClass = PropertyBeingCustomized->Owner.Get<UClass>();
			const bool bIsGraphProperty = LD::PropertyUtils::GetGraphPropertyFromProperty(PropertyBeingCustomized.Get()) != nullptr;
			
			// Filter to exclude local variables or properties that shouldn't be exposed to the graph.
			if (OwnerClass && OwnerClass->IsChildOf(USMNodeInstance::StaticClass()) &&
				(bIsGraphProperty || LD::PropertyUtils::IsPropertyExposedToGraphNode(PropertyBeingCustomized.Get())))
			{
				{
					FSMCompilerDelegates::OnNodePostCompiled.RemoveAll(this);

					const TSharedPtr<IPropertyUtilities> Utilities = DetailLayout.GetPropertyUtilities();
					FSMCompilerDelegates::OnNodePostCompiled.AddSP(this, &FSMVariableCustomization::OnNodeCompiled, Utilities);
				}
				
				USMNodeInstance* NodeInstance = CastChecked<USMNodeInstance>(BlueprintPtr->GeneratedClass->ClassDefaultObject);

				const TSharedPtr<IPropertyHandle> GraphPropertyHandle = FSMNodeInstanceUtils::FindOrAddExposedPropertyOverrideByName(NodeInstance, PropertyBeingCustomized->GetFName(),
					ExposedPropertyOverridePropertyView);
				check(GraphPropertyHandle.IsValid());
			
				IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Variable");

				const TSharedPtr<IPropertyHandle> ReadOnlyHandle = GraphPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSMGraphProperty, bReadOnly));
				const TSharedPtr<IPropertyHandle> HiddenHandle = GraphPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSMGraphProperty, bHidden));
				const TSharedPtr<IPropertyHandle> WidgetHandle = GraphPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSMGraphProperty, WidgetInfo));
				
				IDetailGroup& DetailGroup = Category.AddGroup("StateMachineVariable", LOCTEXT("StateMachineVariableDisplayName", "State Machine Variable"),
					false, true);

				// Add individual properties rather under a group rather than the owning struct.
				// In UE4 the array handle of the struct will show up otherwise.

				// Perform special ResetToDefaults handling only on the class owning the property.
				// It always shows up otherwise, however in the event the user is overriding a parent property
				// ResetToDefaults works and displays correctly.

				UClass* NodeInstanceUpToDateClass = FBlueprintEditorUtils::GetMostUpToDateClass(NodeInstance->GetClass());
				UClass* OwnerClassUpToDateClass = FBlueprintEditorUtils::GetMostUpToDateClass(OwnerClass);
				
				const bool bPropertyOwnedByThisBlueprint = NodeInstanceUpToDateClass == OwnerClassUpToDateClass;

				const FResetToDefaultOverride ResetToDefaultOverride = FResetToDefaultOverride::Create(
					FIsResetToDefaultVisible::CreateSP(this, &FSMVariableCustomization::IsResetToDefaultVisible),
					FResetToDefaultHandler::CreateSP(this, &FSMVariableCustomization::OnResetToDefaultClicked));

				FSMGraphProperty_Base::FVariableDetailsCustomizationConfiguration Config;
				if (bIsGraphProperty)
				{
					// For actual graph properties use the most up to date class for correct variable details information.
					// TODO: NodeInstanceUpToDateClass needed to avoid an ensure, but arrays don't work right if not using the default GeneratedClass.
					TArray<FSMGraphProperty_Base*> GraphProperties;
					USMUtils::BlueprintPropertyToNativeProperty(PropertyBeingCustomized.Get(), NodeInstanceUpToDateClass->ClassDefaultObject, GraphProperties);
					if (GraphProperties.Num())
					{
						// Apply specific customization for this type of graph property.
						GraphProperties[0]->GetVariableDetailsCustomization(Config);
					}
				}

				if (Config.bShowReadOnly)
				{
					IDetailPropertyRow& PropertyRow = DetailGroup.AddPropertyRow(ReadOnlyHandle.ToSharedRef());
					if (bPropertyOwnedByThisBlueprint)
					{
						PropertyRow.OverrideResetToDefault(ResetToDefaultOverride);
					}
				}

				if (Config.bShowHidden)
				{
					IDetailPropertyRow& PropertyRow = DetailGroup.AddPropertyRow(HiddenHandle.ToSharedRef());
					if (bPropertyOwnedByThisBlueprint)
					{
						PropertyRow.OverrideResetToDefault(ResetToDefaultOverride);
					}
				}

				if (Config.bShowWidgetInfo)
				{
					IDetailPropertyRow& PropertyRow = DetailGroup.AddPropertyRow(WidgetHandle.ToSharedRef());
					if (bPropertyOwnedByThisBlueprint)
					{
						PropertyRow.OverrideResetToDefault(FResetToDefaultOverride::Hide());
					}
				}

				const FSimpleDelegate OnStructContentsPreChangedDelegate = FSimpleDelegate::CreateSP(this,
					&FSMVariableCustomization::OnStructContentsPreChanged, NodeInstance);
			
				GraphPropertyHandle->SetOnChildPropertyValuePreChange(OnStructContentsPreChangedDelegate);
				GraphPropertyHandle->SetOnPropertyValuePreChange(OnStructContentsPreChangedDelegate);
			}
		}
	}
}

void FSMVariableCustomization::OnStructContentsPreChanged(USMNodeInstance* InNodeInstance)
{
	if (InNodeInstance)
	{
		InNodeInstance->Modify();
	}
}

bool FSMVariableCustomization::IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> InProperty) const
{
	bool bValue = false;
	if (InProperty.IsValid())
	{
		InProperty->GetValue(bValue);
	}

	// False is the default value for the properties we're checking.
	return bValue;
}

void FSMVariableCustomization::OnResetToDefaultClicked(TSharedPtr<IPropertyHandle> InProperty)
{
	if (InProperty.IsValid())
	{
		InProperty->SetValue(false);
	}
}

void FSMVariableCustomization::OnNodeCompiled(const FKismetCompilerContext& InCompilerContext, TSharedPtr<IPropertyUtilities> InPropertyUtilities)
{
	// Loading children node classes can invalidate the current selection.
	if (InPropertyUtilities.IsValid())
	{
		InPropertyUtilities->EnqueueDeferredAction(FSimpleDelegate::CreateSP(InPropertyUtilities.ToSharedRef(), &IPropertyUtilities::ForceRefresh));
	}
}

#undef LOCTEXT_NAMESPACE
