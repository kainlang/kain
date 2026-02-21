// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMNodeInstance.h"

#include "Templates/SharedPointer.h"

class IPropertyHandle;
class ISinglePropertyView;
class USMGraphNode_Base;
class USMStateInstance_Base;
class USMTransitionInstance;

// Helpers for managing node instances and related objects.
class SMSYSTEMEDITOR_API FSMNodeInstanceUtils
{
public:
	/** @return The asset data for a node class. */
	static FAssetData GetAssetDataForNodeClass(const TSoftClassPtr<USMNodeInstance>& InNodeClass);
	static FSMNodeDescription GetNodeDescriptionFromAssetData(const FAssetData& InAssetData);

	/** @return The display name for this node, accounting for editor name sanitization and class data. */
	static FString GetNodeDisplayName(const USMNodeInstance* InNodeInstance);
	static FString GetNodeDisplayName(const TSoftClassPtr<USMNodeInstance>& InNodeClass);

	/** @return The category of the node for use in context menus. */
	static FText GetNodeCategory(const USMNodeInstance* InNodeInstance);
	static FText GetNodeCategory(const TSoftClassPtr<USMNodeInstance>& InNodeClass);
	
	/** @return The description, accounting for overrides or class data. */
	static FText GetNodeDescriptionText(const USMNodeInstance* InNodeInstance);
	static FText GetNodeDescriptionText(const TSoftClassPtr<USMNodeInstance>& InNodeClass);

	/** @return True if the node class is configured to hide from the context menu if rules fail. */
	static bool ShouldHideFromContextMenuIfRulesFail(const TSoftClassPtr<USMStateInstance_Base>& InNodeClass);

	/** @return True if the node class is configured to hide from class drop downs if rules fail. */
	static bool ShouldHideFromDropDownIfRulesFail(const TSoftClassPtr<USMNodeInstance>& InNodeClass);

	/** @return True if the state class is both placeable and registered with the context menu. */
	static bool IsStateRegisteredWithContextMenu(const TSoftClassPtr<USMStateInstance_Base>& InNodeClass);

	/** @return True if the transition is allowed to be auto placed. */
	static bool CanTransitionBeAutoPlaced(const TSoftClassPtr<USMTransitionInstance>& InNodeClass);

	/** Recursively checks a child to see if it belongs to a parent. */
	static bool IsWidgetChildOf(TSharedPtr<SWidget> Parent, TSharedPtr<SWidget> PossibleChild);

	/** Create formatted text to summarize the node class. */
	static FText CreateNodeClassTextSummary(const USMNodeInstance* NodeInstance);

	/** Create a widget to display node class information. */
	static TSharedPtr<SWidget> CreateNodeClassWidgetDisplay(const USMNodeInstance* NodeInstance);

	/**
	 * Checks if the node might have user defined construction scripts.
	 *
	 * @param NodeClass Node class construction script graph to check.
	 * @param ExecutionType Editor or game construction scripts.
	 */
	static bool DoesNodeClassPossiblyHaveConstructionScripts(TSubclassOf<USMNodeInstance> NodeClass, ESMExecutionEnvironment ExecutionType);

	/**
	 * Return an existing override. O(n).
	 * @param InNodeInstance The node instance to modify. Providing a CDO will propagate values to instances.
	 * @param VariableName The property name to override.
	 * @param OutPropView A single property view which owns the handle. If this goes out of scope the return value will become stale.
	 * @return The property handle if one exists.
	 */
	static TSharedPtr<IPropertyHandle> FindExposedPropertyOverrideByName(USMNodeInstance* InNodeInstance, const FName& VariableName,
		TSharedPtr<ISinglePropertyView>& OutPropView);
	
	/**
	 * Return an existing override or add a new one. O(n).
	 * @param InNodeInstance The node instance to modify. Providing a CDO will propagate values to instances.
	 * @param VariableName The property name to override.
	 * @param OutPropView A single property view which owns the handle. If this goes out of scope the return value will become stale.
	 * @return The existing property handle or a new one.
	 */
	static TSharedPtr<IPropertyHandle> FindOrAddExposedPropertyOverrideByName(USMNodeInstance* InNodeInstance, const FName& VariableName,
		TSharedPtr<ISinglePropertyView>& OutPropView);

	/**
	 * Update an exposed property's VariableName. O(n).
	 * @return True if successful.
	 */
	static bool UpdateExposedPropertyOverrideName(USMNodeInstance* InNodeInstance, const FName& OldVarName, const FName& NewVarName);

	/**
	 * Remove an exposed property override by name. O(n).
	 * @return Number of elements removed.
	 */
	static uint32 RemoveExposedPropertyOverrideByName(USMNodeInstance* InNodeInstance, const FName& VariableName);

	/**
	 * Find all node instance classes of type T derived classes, both native and blueprint.
	 *
	 * @param TargetClass Descendents of this class will be found.
	 * @param OutClasses All found subclasses of the target class.
	 */
	template<typename T>
	static void GetAllNodeSubClasses(const UClass* TargetClass, TArray<TSoftClassPtr<T>>& OutClasses)
	{
		static_assert(TIsDerivedFrom<T, USMNodeInstance>::IsDerived, "T must derive from USMNodeInstance.");
		GetAllNodeSubClasses_Internal(TargetClass, reinterpret_cast<TArray<TSoftClassPtr<USMNodeInstance>>&>(OutClasses));
	}

private:
	static void GetAllNodeSubClasses_Internal(const UClass* TargetClass, TArray<TSoftClassPtr<USMNodeInstance>>& OutClasses);
};

