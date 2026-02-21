// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "ISMEditorGraphNodeInterface.generated.h"

class ISMEditorGraphNode_StateBaseInterface;
class ISMEditorGraphPropertyNodeInterface;
class USMNodeInstance;

// Do not expose as BlueprintType to avoid strong references to editor graph nodes or GC will not happen correctly in editor.
UINTERFACE(MinimalApi, DisplayName = "Editor Graph Node", meta = (CannotImplementInterfaceInBlueprint))
class USMEditorGraphNodeInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for accessing editor graph nodes from non-editor modules.
 */
class ISMEditorGraphNodeInterface
{
	GENERATED_BODY()

public:
	/**
	 * Retrieve an exposed graph property from the node.
	 *
	 * @param PropertyName The name of the public property on the node.
	 * @param NodeInstance The node instance template containing the property. Generally 'this' when called from a node class.
	 * Blueprint usage will default this to the self context.
	 * @param ArrayIndex The index of the element if the property is an array.
	 *
	 * @return A single editor graph property. If this is an array this will be a single element in the array.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = NodeProperties, meta = (DevelopmentOnly, DefaultToSelf = "NodeInstance", AdvancedDisplay = "NodeInstance, ArrayIndex"))
	virtual UPARAM(DisplayName="GraphProperty") TScriptInterface<ISMEditorGraphPropertyNodeInterface> GetEditorGraphProperty(FName PropertyName, const USMNodeInstance* NodeInstance, int32 ArrayIndex = 0) const = 0;

	/**
	 * Retrieve an exposed graph property as an array. This can allow all elements in an array to be returned.
	 *
	 * @param PropertyName The name of the public property on the node.
	 * @param NodeInstance The node instance template containing the property. Generally 'this' when called from a node class.
	 * Blueprint usage will default this to the self context.
	 * @param ArrayIndex The index of the element if the property is an array. If INDEX_NONE (-1) then all elements in the array are returned.
	 *
	 * @return An array of editor graph properties. This is typically a single property unless the exposed property is an array and ArrayIndex is -1.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = NodeProperties, meta = (DevelopmentOnly, DefaultToSelf = "NodeInstance", AdvancedDisplay = "NodeInstance, ArrayIndex"))
	virtual UPARAM(DisplayName="GraphProperties") TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> GetEditorGraphPropertyAsArray(FName PropertyName, const USMNodeInstance* NodeInstance, int32 ArrayIndex = -1) const = 0;

	/**
	 * Retrieve every graph property on the node for a node instance.
	 *
	 * @param NodeInstance The node instance template contained in this graph node to retrieve properties from.
	 * When null all node instances on the node are searched. Blueprint usage will default this to the self context.
	 * If you need to retrieve all stack instance variables in blueprints then either iterate each stack or promote this to a local empty variable.
	 *
	 * @return An array of all editor graph properties.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = NodeProperties, meta = (DevelopmentOnly, DefaultToSelf = "NodeInstance", AdvancedDisplay = "NodeInstance"))
	virtual UPARAM(DisplayName="GraphProperties") TArray<TScriptInterface<ISMEditorGraphPropertyNodeInterface>> GetAllEditorGraphProperties(const USMNodeInstance* NodeInstance) const = 0;

	/**
	 * Add a stack node to the graph node if applicable. Currently only supports state stacks.
	 * Instead of calling directly use the state instance methods to manipulate the state stack during construction.
	 *
	 * @param NodeClass The node class to be created.
	 * @param StackIndex The index to insert the node stack. Leave at -1 to place at the end.
	 *
	 * @return The stack instance created.
	 */
	virtual USMNodeInstance* AddStackNode(TSubclassOf<USMNodeInstance> NodeClass, int32 StackIndex = INDEX_NONE) = 0;

	/**
	 * Remove a stack node by index. Currently only supports state stacks.
	 * Instead of calling directly use the state instance methods to manipulate the state stack during construction.
	 *
	 * @param StackIndex The index to remove. Leave at -1 to remove from the end.
	 */
	virtual void RemoveStackNode(int32 StackIndex = INDEX_NONE) = 0;

	/**
	 * Remove all nodes from the stack. Currently only supports state stacks.
	 */
	virtual void ClearStackNodes() = 0;

	/**
	 * Retrieve the node name assigned in the editor. This is generally the graph name contained in the node and what
	 * is assigned to the run-time variation of the node.
	 * 
	 * This isn't guaranteed to be the same result that GetNodeName on the node instance returns since this method
	 * can be called before the run-time node is created. Additionally some names may later be adjusted by the compiler.
	 * 
	 * @return The node name used in the editor.
	 */
	UFUNCTION(BlueprintCallable, Category = General, meta = (DevelopmentOnly))
	virtual FString GetNodeName() const = 0;
	
	/**
	 * Rename an editor graph node. This generally calls Rename() on the bound graph contained in the node.
	 *
	 * The rename policy is up to the node this is being called on. States enforce unique names per state machine
	 * scope and will automatically adjust duplicate names, whereas transitions allow duplicate names.
	 * 
	 * @param NewName The new node name to assign.
	 * @param OutErrorMessage The error message explaining why a node wasn't renamed, if any.
	 * 
	 * @return True if the rename succeeded, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = General, meta = (DevelopmentOnly))
	virtual bool SetNodeName(const FString& NewName, FText& OutErrorMessage) = 0;

	/**
	 * Reset a node name back to its default.
	 */
	UFUNCTION(BlueprintCallable, Category = General, meta = (DevelopmentOnly))
	virtual void ResetNodeName() = 0;
	
	/**
	 * Attempt to cast this to the EditorGraphNode_StateBaseInterface which contains functions available to all
	 * state base type graph nodes.
	 *
	 * @return Return the State Base Interface. Retrieve the underlying UObject to test validity.
	 */
	UFUNCTION(BlueprintCallable, Category = Casting, meta = (DevelopmentOnly))
	virtual UPARAM(DisplayName="Editor Node (State Base)") TScriptInterface<ISMEditorGraphNode_StateBaseInterface> AsStateBaseInterface() = 0;
};
