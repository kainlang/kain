// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Utilities/SMNodeInstanceUtils.h"

#include "Configuration/SMProjectEditorSettings.h"
#include "SMBlueprintEditorUtils.h"
#include "SMSystemEditorLog.h"
#include "Utilities/SMPropertyUtils.h"

#include "Blueprints/SMBlueprint.h"
#include "SMStateInstance.h"
#include "SMTransitionInstance.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "EdGraphSchema_K2.h"
#include "ISinglePropertyView.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_FunctionEntry.h"
#include "PropertyHandle.h"
#include "SMUnrealTypeDefs.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMNodeInstanceUtils"

/** Locate the property value from either the loaded class or the asset data tag. */
#define GET_PROPERTY_OR_TAG_VALUE(NodeSoftClass, PropertyName, OutPropertyValue)\
if (const UClass* LoadedClass = NodeSoftClass.Get())\
{\
	const FProperty* Property = LoadedClass->FindPropertyByName(PropertyName);\
	check(Property);\
	Property->GetValue_InContainer(LoadedClass->GetDefaultObject(), &OutPropertyValue);\
}\
else\
{\
	const FAssetData AssetData = GetAssetDataForNodeClass(NodeSoftClass);\
	if (AssetData.IsValid())\
	{\
		AssetData.GetTagValue(PropertyName, OutPropertyValue);\
	}\
}\

FAssetData FSMNodeInstanceUtils::GetAssetDataForNodeClass(const TSoftClassPtr<USMNodeInstance>& InNodeClass)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	const FString BlueprintPath = InNodeClass.ToSoftObjectPath().ToString().LeftChop(2); // Chop off _C
	const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(BlueprintPath);

	return AssetData;
}

FSMNodeDescription FSMNodeInstanceUtils::GetNodeDescriptionFromAssetData(const FAssetData& InAssetData)
{
	FSMNodeDescription NodeDescription;

	if (InAssetData.IsValid())
	{
		if (const FStructProperty* Property =
		CastField<FStructProperty>(USMNodeInstance::StaticClass()->FindPropertyByName(USMNodeInstance::GetNodeDescriptionPropertyName())))
		{
			const FString NodeDescriptionString =
				InAssetData.GetTagValueRef<FString>(Property->GetFName());
		
			Property->ImportText_Direct(*NodeDescriptionString, &NodeDescription, nullptr, PPF_None);
		}
	}
	
	return NodeDescription;
}

FString FSMNodeInstanceUtils::GetNodeDisplayName(const USMNodeInstance* InNodeInstance)
{
	check(InNodeInstance);
	return FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRestrictInvalidCharacters ?
	FSMBlueprintEditorUtils::GetSafeStateName(InNodeInstance->GetNodeDisplayName()) : InNodeInstance->GetNodeDisplayName();
}

FString FSMNodeInstanceUtils::GetNodeDisplayName(const TSoftClassPtr<USMNodeInstance>& InNodeClass)
{
	if (const UClass* Class = InNodeClass.Get())
	{
		return GetNodeDisplayName(CastChecked<USMNodeInstance>(Class->GetDefaultObject()));
	}

	// Attempt to use node description first.
	const FAssetData AssetData = GetAssetDataForNodeClass(InNodeClass);
	if (!AssetData.IsValid())
	{
		return FString();
	}
	
	const FSMNodeDescription NodeDescription = GetNodeDescriptionFromAssetData(AssetData);
	
	FString ClassDisplayName = NodeDescription.Name.IsNone() ? FString() : NodeDescription.Name.ToString();
	if (ClassDisplayName.IsEmpty())
	{
		// Attempt class display name or class name
		ClassDisplayName = AssetData.GetTagValueRef<FString>(FBlueprintTags::BlueprintDisplayName);
		if (ClassDisplayName.IsEmpty())
		{
			ClassDisplayName = AssetData.AssetName.ToString();
		}
		else
		{
			ClassDisplayName = FName::NameToDisplayString(ClassDisplayName, false);
		}

		ClassDisplayName.RemoveFromEnd(TEXT("_C"));
	}
	
	return FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRestrictInvalidCharacters ?
		FSMBlueprintEditorUtils::GetSafeStateName(ClassDisplayName) : ClassDisplayName;
}

FText FSMNodeInstanceUtils::GetNodeCategory(const USMNodeInstance* InNodeInstance)
{
	check(InNodeInstance);
	const FSMNodeDescription& Description = InNodeInstance->GetNodeDescription();
	if (!Description.Category.IsEmpty())
	{
		return Description.Category;
	}

	return InNodeInstance->GetClass()->GetMetaDataText(TEXT("Category"));
}

FText FSMNodeInstanceUtils::GetNodeCategory(const TSoftClassPtr<USMNodeInstance>& InNodeClass)
{
	if (const UClass* Class = InNodeClass.Get())
	{
		return GetNodeCategory(CastChecked<USMNodeInstance>(Class->GetDefaultObject()));
	}

	// Attempt to use node description first.
	const FAssetData AssetData = GetAssetDataForNodeClass(InNodeClass);
	if (!AssetData.IsValid())
	{
		return FText::GetEmpty();
	}
	
	const FSMNodeDescription NodeDescription = GetNodeDescriptionFromAssetData(AssetData);
	
	FText Category = NodeDescription.Category;
	if (Category.IsEmpty())
	{
		Category = AssetData.GetTagValueRef<FText>(FBlueprintTags::BlueprintCategory);
	}
	
	return Category;
}

FText FSMNodeInstanceUtils::GetNodeDescriptionText(const USMNodeInstance* InNodeInstance)
{
	check(InNodeInstance);
	return InNodeInstance->GetNodeDescriptionText();
}

FText FSMNodeInstanceUtils::GetNodeDescriptionText(const TSoftClassPtr<USMNodeInstance>& InNodeClass)
{
	if (const UClass* Class = InNodeClass.Get())
	{
		return GetNodeDescriptionText(CastChecked<USMNodeInstance>(Class->GetDefaultObject()));
	}

	// Attempt to use node description first.
	const FAssetData AssetData = GetAssetDataForNodeClass(InNodeClass);
	if (!AssetData.IsValid())
	{
		return FText::GetEmpty();
	}
	
	const FSMNodeDescription NodeDescription = GetNodeDescriptionFromAssetData(AssetData);
	
	FText Description = NodeDescription.Description;
	if (Description.IsEmpty())
	{
		Description = AssetData.GetTagValueRef<FText>(FBlueprintTags::BlueprintDescription);
		if (Description.IsEmpty())
		{
			Description = FText::FromString(AssetData.AssetName.ToString());
		}
	}
	
	return Description;
}

bool FSMNodeInstanceUtils::ShouldHideFromContextMenuIfRulesFail(const TSoftClassPtr<USMStateInstance_Base>& InNodeClass)
{
	const FName PropertyName = USMStateInstance_Base::GetHideFromContextMenuIfRulesFailPropertyName();
	bool bResult = false;

	GET_PROPERTY_OR_TAG_VALUE(InNodeClass, PropertyName, bResult);
	
	return bResult;
}

bool FSMNodeInstanceUtils::ShouldHideFromDropDownIfRulesFail(const TSoftClassPtr<USMNodeInstance>& InNodeClass)
{
	const FName PropertyName = USMStateInstance_Base::GetHideFromDropDownIfRulesFailPropertyName();
	bool bResult = false;

	GET_PROPERTY_OR_TAG_VALUE(InNodeClass, PropertyName, bResult);
	
	return bResult;
}

bool FSMNodeInstanceUtils::IsStateRegisteredWithContextMenu(const TSoftClassPtr<USMStateInstance_Base>& InNodeClass)
{
	bool bRegisteredWithContextMenu = false;
	const FName PropertyName = USMStateInstance_Base::GetRegisteredWithContextMenuPropertyName();
	
	if (const UClass* Class = InNodeClass.Get())
	{
		if (!Class->HasAnyClassFlags(CLASS_NotPlaceable))
		{
			const FProperty* Property = InNodeClass->FindPropertyByName(PropertyName);
			check(Property);
			Property->GetValue_InContainer(InNodeClass->GetDefaultObject(), &bRegisteredWithContextMenu);
		}
	}
	else
	{
		const FAssetData AssetData = GetAssetDataForNodeClass(InNodeClass);
		if (!(AssetData.GetTagValueRef<uint32>(FBlueprintTags::ClassFlags) & CLASS_NotPlaceable))
		{
			AssetData.GetTagValue(PropertyName, bRegisteredWithContextMenu);
		}
	}
	
	return bRegisteredWithContextMenu;
}

bool FSMNodeInstanceUtils::CanTransitionBeAutoPlaced(const TSoftClassPtr<USMTransitionInstance>& InNodeClass)
{
	const FName PropertyName = USMTransitionInstance::GetAutoPlaceTransitionIfRulesPassPropertyName();
	bool bResult = false;

	GET_PROPERTY_OR_TAG_VALUE(InNodeClass, PropertyName,  bResult);
	
	return bResult;
}

bool FSMNodeInstanceUtils::IsWidgetChildOf(TSharedPtr<SWidget> Parent, TSharedPtr<SWidget> PossibleChild)
{
	FChildren* Children = Parent->GetChildren();
	for (int32 i = 0; i < Children->Num(); ++i)
	{
		TSharedRef<SWidget> Child = Children->GetChildAt(i);
		if (Child == PossibleChild)
		{
			return true;
		}
		return IsWidgetChildOf(Child, PossibleChild);
	}

	return false;
}

FText FSMNodeInstanceUtils::CreateNodeClassTextSummary(const USMNodeInstance* NodeInstance)
{
	check(NodeInstance);

	const FString Name = GetNodeDisplayName(NodeInstance);
	const FText Description = GetNodeDescriptionText(NodeInstance);

	const FText TextFormat = FText::FromString(Description.IsEmpty() ? "{0}" : "{0} - {1}");
	return FText::Format(TextFormat, FText::FromString(Name), Description);
}

TSharedPtr<SWidget> FSMNodeInstanceUtils::CreateNodeClassWidgetDisplay(const USMNodeInstance* NodeInstance)
{
	check(NodeInstance);
	
	const FSMNodeDescription& Description = NodeInstance->GetNodeDescription();

	FString ClassName = NodeInstance->GetClass()->GetName();
	ClassName.RemoveFromEnd(TEXT("_C"));
	const FString Name = Description.Name.IsNone() ? ClassName : Description.Name.ToString();
	
	const FText TextFormat = FText::FromString(Description.Description.IsEmpty() ? "{0}" : "{0} - {1}");
	const FText NodeClassSummaryText = CreateNodeClassTextSummary(NodeInstance);
	
	return SNew(SOverlay)
	+ SOverlay::Slot()
	[
		SNew(SBorder)
		.BorderImage(FSMUnrealAppStyle::Get().GetBrush("Graph.Node.TitleBackground"))
		.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 0.4f))
	]
	+ SOverlay::Slot()
	.VAlign(VAlign_Center)
	.Padding(FMargin(6,4))
	[
		SNew(STextBlock)
			.Text(NodeClassSummaryText)
			.TextStyle(FSMUnrealAppStyle::Get(), TEXT("NormalText"))
			.ColorAndOpacity(FLinearColor::White)
	];
}

bool FSMNodeInstanceUtils::DoesNodeClassPossiblyHaveConstructionScripts(TSubclassOf<USMNodeInstance> NodeClass, ESMExecutionEnvironment ExecutionType)
{
	if (!NodeClass)
	{
		return false;
	}

	const bool bIsBaseClass = FSMNodeClassRule::IsBaseClass(NodeClass);
	if (bIsBaseClass)
	{
		// Base classes have no construction script logic.
		return false;
	}

	auto DoesGraphHaveUserLogic = [ExecutionType](UEdGraph* InGraph, bool& bOutHasParentCall) -> bool
	{
		UK2Node_FunctionEntry* EntryNode = FSMBlueprintEditorUtils::GetFirstNodeOfClassNested<UK2Node_FunctionEntry>(InGraph);
		if (!ensure(EntryNode))
		{
			return false;
		}

		UEdGraphPin* ThenPin = EntryNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
		if (ThenPin->LinkedTo.Num() == 0)
		{
			// No connections, no logic.
			return false;
		}

		UK2Node_CallFunction* ExecutionEnvironmentFunction = nullptr;
		
		UK2Node_CallParentFunction* ParentCall = Cast<UK2Node_CallParentFunction>(ThenPin->LinkedTo[0]->GetOwningNode());
		if (ParentCall == nullptr)
		{
			// Check if instead of the parent we are connected right to the with execution node.
			ExecutionEnvironmentFunction = Cast<UK2Node_CallFunction>(ThenPin->LinkedTo[0]->GetOwningNode());
			if (ExecutionEnvironmentFunction == nullptr)
			{
				// This isn't a default layout, assume user logic.
				return true;
			}
		}
		else
		{
			bOutHasParentCall = true;
		}
		
		if (ParentCall && ParentCall->GetThenPin()->LinkedTo.Num() == 0)
		{
			// No connections, no logic.
			return false;
		}

		if (ExecutionEnvironmentFunction == nullptr && ParentCall != nullptr)
		{
			ExecutionEnvironmentFunction = Cast<UK2Node_CallFunction>(ParentCall->GetThenPin()->LinkedTo[0]->GetOwningNode());
		}
		
		if (!ExecutionEnvironmentFunction || ExecutionEnvironmentFunction->GetFunctionName() != GET_FUNCTION_NAME_CHECKED(USMNodeInstance, WithExecutionEnvironment))
		{
			// Unexpected type or different function, assume user logic.
			return true;
		}

		if (ExecutionType == ESMExecutionEnvironment::EditorExecution)
		{
			if (UEdGraphPin* EditorExecutionPin = ExecutionEnvironmentFunction->FindPin(TEXT("EditorExecution"), EGPD_Output))
			{
				if (EditorExecutionPin->LinkedTo.Num() > 0)
				{
					// Editor output pin is connected somewhere, there is user logic.
					return true;
				}
			}
		}
		else if (ExecutionType == ESMExecutionEnvironment::GameExecution)
		{
			if (UEdGraphPin* GameExecutionPin = ExecutionEnvironmentFunction->FindPin(TEXT("GameExecution"), EGPD_Output))
			{
				if (GameExecutionPin->LinkedTo.Num() > 0)
				{
					// Game output pin is connected somewhere, there is user logic.
					return true;
				}
			}
		}
		
		return false;
	};
	
	if (USMNodeBlueprint* NodeBlueprint = Cast<USMNodeBlueprint>(UBlueprint::GetBlueprintFromClass(NodeClass)))
	{
		if (TObjectPtr<UEdGraph>* ConstructionScriptGraph = NodeBlueprint->FunctionGraphs.FindByPredicate([] (UEdGraph* InGraph)
		{
			return InGraph->GetFName() == USMNodeInstance::GetConstructionScriptFunctionName();
		}))
		{
			bool bHasParentCall = false;
			const bool bHasGraphLogic = DoesGraphHaveUserLogic(*ConstructionScriptGraph, bHasParentCall);
			if (bHasGraphLogic)
			{
				return true;
			}

			// No graph logic, check parents.
			if (bHasParentCall)
			{
				return DoesNodeClassPossiblyHaveConstructionScripts(NodeClass->GetSuperClass(), ExecutionType);
			}

			return false;
		}
	}

	// No blueprint or graph found... probably a native class.
	if (USMNodeInstance* NodeDefaults = Cast<USMNodeInstance>(NodeClass->GetDefaultObject()))
	{
		return !NodeDefaults->ShouldSkipNativeEditorConstructionScripts();
	}

	return true;
}

TSharedPtr<IPropertyHandle> FSMNodeInstanceUtils::FindExposedPropertyOverrideByName(USMNodeInstance* InNodeInstance,
	const FName& VariableName, TSharedPtr<ISinglePropertyView>& OutPropView)
{
	check(InNodeInstance);
	
	OutPropView = LD::PropertyUtils::CreatePropertyViewForProperty(InNodeInstance, GET_MEMBER_NAME_CHECKED(USMNodeInstance, ExposedPropertyOverrides));

	const TSharedPtr<IPropertyHandle> PropertyHandle = OutPropView->GetPropertyHandle();
	check(PropertyHandle->IsValidHandle());

	const TSharedPtr<IPropertyHandleArray> ArrayPropertyHandle = PropertyHandle->AsArray();
	check(ArrayPropertyHandle.IsValid());
	
	uint32 NumElements = 0;
	{
		const FPropertyAccess::Result Result = ArrayPropertyHandle->GetNumElements(NumElements);
		check(Result == FPropertyAccess::Success);
	}

	for (uint32 Idx = 0; Idx < NumElements; ++Idx)
	{
		const TSharedRef<IPropertyHandle> Element = ArrayPropertyHandle->GetElement(Idx);
		check(Element->IsValidHandle());

		void* Data;
		const FPropertyAccess::Result Result = Element->GetValueData(Data);
		check(Result == FPropertyAccess::Success);

		const FSMGraphProperty* GraphProperty = static_cast<FSMGraphProperty*>(Data);
		if (GraphProperty->VariableName == VariableName)
		{
			return Element;
		}
	}

	return nullptr;
}

TSharedPtr<IPropertyHandle> FSMNodeInstanceUtils::FindOrAddExposedPropertyOverrideByName(USMNodeInstance* InNodeInstance,
                                                                                         const FName& VariableName, TSharedPtr<ISinglePropertyView>& OutPropView)
{
	check(InNodeInstance);

	// First look for an existing element.
	TSharedPtr<IPropertyHandle> ExistingPropertyHandle = FindExposedPropertyOverrideByName(InNodeInstance, VariableName, OutPropView);
	if (ExistingPropertyHandle.IsValid())
	{
		return ExistingPropertyHandle;
	}
	
	const bool bPackageWasDirty = InNodeInstance->GetPackage()->IsDirty();

	const TSharedPtr<IPropertyHandle> PropertyHandle = OutPropView->GetPropertyHandle();
	check(PropertyHandle->IsValidHandle());

	const TSharedPtr<IPropertyHandleArray> ArrayPropertyHandle = PropertyHandle->AsArray();
	check(ArrayPropertyHandle.IsValid());
	
	// Not found, add a new one.
	{
		const FPropertyAccess::Result Result = ArrayPropertyHandle->AddItem();
		check(Result == FPropertyAccess::Success);
	}

	uint32 NumElements = 0;
	{
		const FPropertyAccess::Result Result = ArrayPropertyHandle->GetNumElements(NumElements);
		check(Result == FPropertyAccess::Success);
	}

	TSharedPtr<IPropertyHandle> ElementHandle = ArrayPropertyHandle->GetElement(NumElements - 1);
	check(ElementHandle->IsValidHandle());
	
	// Set the correct name.
	{
		const TSharedPtr<IPropertyHandle> NameHandle = ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSMGraphProperty, VariableName));
		check(NameHandle->IsValidHandle());
	
		NameHandle->SetValue(VariableName);
	}

	if (!bPackageWasDirty)
	{
		// It's okay not to save this on initial add. It only needs to be saved if a value has changed. Otherwise just
		// clicking on a variable could dirty the asset.
		InNodeInstance->GetPackage()->ClearDirtyFlag();
	}

	return ElementHandle;
}

bool FSMNodeInstanceUtils::UpdateExposedPropertyOverrideName(USMNodeInstance* InNodeInstance, const FName& OldVarName,
	const FName& NewVarName)
{
	TSharedPtr<ISinglePropertyView> PropView;
	const TSharedPtr<IPropertyHandle> ExistingPropertyHandle = FindExposedPropertyOverrideByName(InNodeInstance, OldVarName, PropView);
	if (ExistingPropertyHandle.IsValid())
	{
		const TSharedPtr<IPropertyHandle> NameHandle = ExistingPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSMGraphProperty, VariableName));
		check(NameHandle.IsValid());
		
		NameHandle->SetValue(NewVarName);
		return true;
	}

	return false;
}

uint32 FSMNodeInstanceUtils::RemoveExposedPropertyOverrideByName(USMNodeInstance* InNodeInstance, const FName& VariableName)
{
	const TSharedPtr<ISinglePropertyView> PropertyView =
		LD::PropertyUtils::CreatePropertyViewForProperty(InNodeInstance, GET_MEMBER_NAME_CHECKED(USMNodeInstance, ExposedPropertyOverrides));

	const TSharedPtr<IPropertyHandle> PropertyHandle = PropertyView->GetPropertyHandle();
	check(PropertyHandle->IsValidHandle());

	const TSharedPtr<IPropertyHandleArray> ArrayPropertyHandle = PropertyHandle->AsArray();
	check(ArrayPropertyHandle.IsValid());
	
	uint32 NumElements = 0;
	{
		const FPropertyAccess::Result Result = ArrayPropertyHandle->GetNumElements(NumElements);
		check(Result == FPropertyAccess::Success);
	}

	uint32 ElementsRemoved = 0;
	for (uint32 Idx = 0; Idx < NumElements;)
	{
		const TSharedRef<IPropertyHandle> Element = ArrayPropertyHandle->GetElement(Idx);
		check(Element->IsValidHandle());

		void* Data;
		{
			const FPropertyAccess::Result Result = Element->GetValueData(Data);
			check(Result == FPropertyAccess::Success);
		}
		
		const FSMGraphProperty* GraphProperty = static_cast<FSMGraphProperty*>(Data);
		if (GraphProperty->VariableName == VariableName)
		{
			const FPropertyAccess::Result Result = ArrayPropertyHandle->DeleteItem(Idx);
			check(Result == FPropertyAccess::Success);
			
			++ElementsRemoved;
			--NumElements;
		}
		else
		{
			++Idx;
		}
	}

	return ElementsRemoved;
}

void FSMNodeInstanceUtils::GetAllNodeSubClasses_Internal(const UClass* TargetClass,
	TArray<TSoftClassPtr<USMNodeInstance>>& OutClasses)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSMNodeInstanceUtils_GetAllNodeSubClasses"), STAT_GetAllNodeSubClasses, STATGROUP_LogicDriverEditor);

	check(TargetClass);
	
	constexpr EClassFlags InvalidClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract;

	// Gather native classes.
	{
		auto IsValidNativeClass = [&](const UClass* Class) -> bool
		{
			return Class->IsNative()
				&& Class->IsChildOf(TargetClass)
				&& !Class->HasAnyClassFlags(InvalidClassFlags)
				&& !Class->GetName().StartsWith(TEXT("SKEL_")) && !Class->GetName().StartsWith(TEXT("REINST_"));
		};
		
		if (IsValidNativeClass(TargetClass))
		{
			OutClasses.Add(TargetClass);
		}

		TArray<UClass*> DerivedClasses;
		GetDerivedClasses(TargetClass, DerivedClasses);
		for (const UClass* DerivedClass : DerivedClasses)
		{
			if (IsValidNativeClass(DerivedClass))
			{
				OutClasses.Add(DerivedClass);
			}
		}
	}

	// Gather blueprint classes.
	{
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();

		TSet<FTopLevelAssetPath> DerivedClassNames;
		AssetRegistry.GetDerivedClassNames({ TargetClass->GetClassPathName() }, {}, DerivedClassNames);

		TArray<FAssetData> OutAssets;
		AssetRegistry.GetAssetsByClass(USMNodeBlueprint::StaticClass()->GetClassPathName(), OutAssets, true);

		for (const FAssetData& Asset : OutAssets)
		{
			FAssetDataTagMapSharedView::FFindTagResult Result = Asset.TagsAndValues.FindTag(TEXT("GeneratedClass"));
			if (Result.IsSet())
			{
				const FString& GeneratedClassPathPtr = Result.GetValue();
				const FTopLevelAssetPath ClassObjectPath(FPackageName::ExportTextPathToObjectPath(*GeneratedClassPathPtr));

				if (DerivedClassNames.Contains(ClassObjectPath))
				{
					TSoftClassPtr<USMNodeInstance> SoftClassPath(ClassObjectPath.ToString());

					// Only need to check against flags. REINST classes aren't possible here and child check already performed.
					const uint32 ClassFlags = Asset.GetTagValueRef<uint32>(FBlueprintTags::ClassFlags);
					if (!EnumHasAnyFlags(static_cast<EClassFlags>(ClassFlags), InvalidClassFlags))
					{
						OutClasses.Add(SoftClassPath);
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
