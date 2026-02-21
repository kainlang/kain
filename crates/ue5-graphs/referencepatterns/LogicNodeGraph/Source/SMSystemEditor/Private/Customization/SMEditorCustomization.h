// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "IPropertyUtilities.h"
#include "Modules/ModuleManager.h"

class USMGraphNode_Base;
class USMGraphK2Node_PropertyNode_Base;

template<typename T>
static T* GetObjectBeingCustomized(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (TWeakObjectPtr<UObject> Object : Objects)
	{
		if (T* CastedObject = Cast<T>(Object.Get()))
		{
			return CastedObject;
		}
	}

	return nullptr;
}

static EVisibility VisibilityConverter(bool bValue)
{
	return bValue ? EVisibility::Visible : EVisibility::Collapsed;
}

class FSMBaseCustomization : public IDetailCustomization
{
public:
	// IDetailCustomization
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override {}
	// ~IDetailCustomization

	/** Recursively hide all handles. */
	static void HideNestedCategoryHandles(const TSharedPtr<IPropertyHandle>& InHandle);
protected:
	void ForceUpdate();

	/** Hides the any state editor tags since HideCategory won't work for them. */
	static void HideAnyStateTags(IDetailLayoutBuilder& DetailBuilder);
	
	TWeakPtr<IDetailLayoutBuilder> DetailBuilderPtr;
};

class FSMNodeCustomization : public FSMBaseCustomization
{
public:
	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// ~IDetailCustomization

	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	TWeakObjectPtr<USMGraphNode_Base> SelectedGraphNode;
};

class FSMNodeInstanceCustomization : public FSMBaseCustomization {
public:
	enum class ENodeStackType : uint8
	{
		None,
		StateStack,
		TransitionStack
	};
	
	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// ~IDetailCustomization

	/**
	 * Handle the details panel for exposed graph properties for all node instances.
	 *
	 * @param GraphNode					The selected graph node.
	 * @param TemplateProperties		All template properties to check.
	 * @param NodeInstance				The node instance template containing the properties.
	 * @param DetailBuilder				The detail builder to use.
	 * 
	 */
	static void ProcessNodeInstance(TWeakObjectPtr<USMGraphNode_Base> GraphNode, const TArray<TSharedRef<IPropertyHandle>>& TemplateProperties,
		class USMNodeInstance* NodeInstance, IDetailLayoutBuilder& DetailBuilder);

	/**
	 * Display a single exposed property widget in the details panel. Can be called from either state base or state stack.
	 *
	 * @param GraphNode					The selected graph node.
	 * @param PropertyHandle			The specific template property.
	 * @param NodeInstance				The node instance template containing the properties.
	 * @param DetailBuilder				The detail builder to use if ChildrenBuilder is not supplied.
	 * @param ChildrenBuilder			A child builder if being called from struct customization, such as for the state stack.
	 */
	static void DisplayExposedPropertyWidget(TWeakObjectPtr<USMGraphNode_Base> GraphNode, const TSharedRef<IPropertyHandle>& PropertyHandle, USMNodeInstance* NodeInstance,
		IDetailLayoutBuilder* DetailBuilder = nullptr, IDetailChildrenBuilder* ChildrenBuilder = nullptr);

	/**
	 * Find the correct node instance to use by seeing if the given property belongs to a node stack array.
	 *
	 * @param GraphNode The Graph node containing the template.
	 * @param InPropertyHandle A handle for the property being modified, such as a node stack template property.
	 * @return The node instance belonging to a node stack, or the primary node instance if no stack index was found.
	 */
	static USMNodeInstance* GetCorrectNodeInstanceFromPropertyHandle(TWeakObjectPtr<USMGraphNode_Base> GraphNode,
		TSharedPtr<IPropertyHandle> InPropertyHandle);

	static bool IsResetToDefaultVisible(TSharedPtr<IPropertyHandle> PropertyHandle, TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base> GraphPropertyNode);
	static void OnResetToDefaultClicked(TSharedPtr<IPropertyHandle> PropertyHandle, TWeakObjectPtr<USMGraphK2Node_PropertyNode_Base> GraphPropertyNode);

	/** Recursively check if a handle has no properties and hide the handle. */
	static bool HideEmptyCategoryHandles(const TSharedPtr<IPropertyHandle>& InHandle, ENodeStackType NodeStackType);
	
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	static void GenerateGraphArrayWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder,
		TWeakObjectPtr<USMGraphNode_Base> SelectedNode, USMNodeInstance* NodeInstance, FText FilterString);

	static void SortCategories(const TMap<FName, IDetailCategoryBuilder*>& AllCategoryMap);

protected:
	TWeakObjectPtr<USMGraphNode_Base> SelectedGraphNode;
};

class FSMStructCustomization : public IPropertyTypeCustomization
{
public:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		PropertyHandle = InPropertyHandle;
		check(PropertyHandle.IsValid());
	}
	
	USMGraphNode_Base* GetGraphNodeBeingCustomized(IPropertyTypeCustomizationUtils& StructCustomizationUtils, bool bCheckParent = false) const;

	/** Register the given struct with the Property Editor. */
	template<typename T>
	FORCEINLINE static void RegisterNewStruct(const FName& Name)
	{
		bool bAlreadySet = false;
		RegisteredStructs.Add(Name, &bAlreadySet);
		if (bAlreadySet)
		{
			return;
		}
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(Name, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&T::MakeInstance));
	}

	/** Unregister all previously registered structs from the Property Editor. */
	static void UnregisterAllStructs()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		for (const FName& Name : RegisteredStructs)
		{
			PropertyModule.UnregisterCustomPropertyTypeLayout(Name);
		}
	}
	
	template<typename T>
	T* GetObjectBeingCustomized(IPropertyTypeCustomizationUtils& StructCustomizationUtils,
	                            bool bCheckParent = false) const
	{
		const TSharedPtr<IPropertyUtilities> PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();
		if (PropertyUtilities.IsValid())
		{
			const TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized = PropertyUtilities->GetSelectedObjects();

			if (ObjectsBeingCustomized.Num() != 1)
			{
				return nullptr;
			}

			if (T* GraphNode = Cast<T>(ObjectsBeingCustomized[0]))
			{
				return GraphNode;
			}

			if (bCheckParent)
			{
				return Cast<T>(ObjectsBeingCustomized[0]->GetOuter());
			}
		}

		return nullptr;
	}

protected:
	TSharedPtr<IPropertyHandle> PropertyHandle;

private:
	SMSYSTEMEDITOR_API static TSet<FName> RegisteredStructs;
};

class FSMGraphPropertyCustomization : public FSMStructCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// ~IPropertyTypeCustomization
};
