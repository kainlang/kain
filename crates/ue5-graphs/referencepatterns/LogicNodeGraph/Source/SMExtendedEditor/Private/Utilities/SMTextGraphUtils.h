// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMTextGraphProperty.h"

class UK2Node_CallFunction;
class USMGraphNode_Base;
class USMTextPropertyGraph;

class FSMTextGraphUtils
{
public:
	/** Searches all nodes that contain text graph properties. Provide a filter to limit to graph properties that contain a property or function name. */
	SMEXTENDEDEDITOR_API static void GetAllNodesWithTextPropertiesNested(UBlueprint* Blueprint, TArray<USMGraphNode_Base*>& NodesOut, const FName& ParsedNameFilter = NAME_None);

	/** Retrieve all text property nodes in a blueprint. */ 
	SMEXTENDEDEDITOR_API static void GetAllTextPropertiesNested(UBlueprint* Blueprint, TArray<class USMGraphK2Node_TextPropertyNode*>& TextPropertyNodesOut, const FName& ParsedNameFilter = NAME_None);

	/** Resets text property graphs and reconstructs text property nodes. */
	static void RefreshTextProperties(UBlueprint* InBlueprint, const FName& ContainingParsedName = NAME_None);

	// Blueprint events
	static void HandleRenameVariableReferencesEvent(UBlueprint* InBlueprint, UClass* InVariableClass, const FName& InOldVarName, const FName& InNewVarName);
	static void HandleRenameGraphEvent(UBlueprint* InBlueprint, UEdGraph* InVariableClass, const FName& InOldVarName, const FName& InNewVarName);
	static void HandleOnPropertyChangedEvent(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent);
	static void HandlePostConditionallyCompileBlueprintEvent(UBlueprint* Blueprint, bool bUpdateDependencies = true, bool bRecreateGraphProperties = false);

	static UK2Node_CallFunction* CreateTextConversionNode(USMTextPropertyGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, const FSMTextSerializer& TextSerializer, bool bWireConnection = true);
	static UEdGraphPin* GetConversionInputPin(UEdGraphNode* Node, bool bCheckSelfPin = false);
	static UEdGraphPin* GetConversionOutputPin(UEdGraphNode* Node);
	static UEdGraphPin* GetStaticFunctionPin(UEdGraphNode* Node);
	static FName FindTextConversionFunctionName(FName FromType, UObject* Object);

	static FName GetCustomConversionFunctionName(const FSMTextSerializer& TextSerializer);

	/** Used to represent a native property since they have no guids. */
	static const FGuid& GetNativePropertyGuid();
};