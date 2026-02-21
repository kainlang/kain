// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Nodes/RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"

#include "ISMEditorGraphPropertyNodeInterface.h"

#include "SMGraphK2Node_PropertyNode_Base.generated.h"

class FCompilerResultsLog;
class FSMKismetCompilerContext;
class FStructProperty;
class USMGraphNode_Base;
class USMNodeBlueprint;
class USMNodeInstance;
class USMPropertyGraph;
struct FToolMenuSection;
struct FSMGraphProperty_Base;
struct FSearchTagDataPair;
struct FSMGraphProperty_Base_Runtime;

/**
 * Pure root reference nodes that are placed within a property graph returning a value.
 * USMGraphNode_Base -> K2 BoundGraph -> Property Graph -> Property Node (this)
 * Slate Node for USMGraphNode_Base -> this->GetGraphNodeWidget
 * Details Panel for USMGraphNode_Base -> this->GetGraphDetailWidget
 */
UCLASS(Abstract)
class SMSYSTEMEDITOR_API USMGraphK2Node_PropertyNode_Base : public USMGraphK2Node_RuntimeNodeReference, public ISMEditorGraphPropertyNodeInterface
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~USMGraphK2Node_PropertyNode_Base() override;
	
	// UEdGraphNode
	virtual bool IsNodePure() const override { return true; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PostPlacedNewNode() override;
	virtual void ReconstructNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual bool HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const override;
	virtual bool CanCollapseNode() const override { return false; }
	virtual void AddPinSearchMetaDataInfo(const UEdGraphPin* Pin, TArray<FSearchTagDataPair>& OutTaggedMetaData) const override;
	virtual FString GetPinMetaData(FName InPinName, FName InKey) override;
	// ~UEdGraphNode

protected:
	/** Add shared pin search meta between properties. Children should call this if they overload AddPinSearchMetaDataInfo. */
	void AddSharedPinSearchMetaDataInfo(TArray<FSearchTagDataPair>& OutTaggedMetaData) const;

public:
	// USMGraphK2Node_RuntimeNodeReference
	virtual void PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog) override;
	// ~USMGraphK2Node_RuntimeNodeReference

	// ISMEditorGraphPropertyNodeInterface
	virtual void SetHighlight(bool bEnable, FLinearColor Color, bool bClearOnCompile) override;
	virtual void SetNotification(bool bEnable, ESMLogType Severity, const FString& Message, bool bClearOnCompile) override;
	virtual void SetNotificationAndHighlight(bool bEnable, ESMLogType Severity, const FString& Message, bool bClearOnCompile) override;
	virtual void ResetProperty() override;
	virtual void RefreshPropertyPinFromValue() override;
	virtual void RefreshPropertyValueFromPin() override;
	// ~ISMEditorGraphPropertyNodeInterface

	/** Called during pre-compile before construction scripts have run. */
	virtual void PreCompileBeforeConstructionScripts(FSMKismetCompilerContext& CompilerContext);

	/** Called during pre compile by the owning state machine graph node. */
	virtual void PreCompile(FSMKismetCompilerContext& CompilerContext) {}

	/** Return the FProperty this node represents. */
	FProperty* GetProperty() const;

	/** Retrieve the property graph where this property node is located. */
	USMPropertyGraph* GetPropertyGraph() const;

	/** Allow runtime properties to set their values from their editor counterparts. */
	virtual void ConfigureRuntimePropertyNode() {}
	
	/** Get the runtime graph property. */
	virtual FSMGraphProperty_Base_Runtime* GetRuntimePropertyNode() { return nullptr; }
	FSMGraphProperty_Base_Runtime* GetRuntimePropertyNodeChecked() { FSMGraphProperty_Base_Runtime* Node = GetRuntimePropertyNode(); check(Node); return Node; }
	
	/** Get the editor property node. */
	virtual FSMGraphProperty_Base* GetPropertyNode() { return nullptr; }
	
	/** Get the editor graph property. */
	FSMGraphProperty_Base* GetPropertyNodeChecked() { FSMGraphProperty_Base* Node = GetPropertyNode(); check(Node); return Node; }
	FSMGraphProperty_Base* GetPropertyNodeConst() const { return const_cast<USMGraphK2Node_PropertyNode_Base*>(this)->GetPropertyNode(); }
	FSMGraphProperty_Base* GetPropertyNodeConstChecked() const { FSMGraphProperty_Base* Node = GetPropertyNodeConst(); check(Node); return Node; }
	
	/** Sets the new node. Useful for refreshing the node with updated values from a template. */
	virtual void SetPropertyNode(FSMGraphProperty_Base* NewNode) {}
	
	/** Sets the property on the node template to match this pin. */
	virtual void SetPropertyDefaultsFromPin();
	
	/**
	 * Sets the pin default value from the property value.
	 *
	 * This method was originally written to reset the pin to the node instance CDO value but has since grown to handle
	 * the current node instance value that could be changed programatically.
	 *
	 * @param bUpdateTemplateDefaults Potentially calls SetPropertyDefaultsFromPin instead, should not be used.
	 * @param bUseArchetype Use the node instance CDO to read values instead of the current instance.
	 * @param bForce Forces a change through even if the current value matches defaults.
	 */
	virtual void SetPinValueFromPropertyDefaults(bool bUpdateTemplateDefaults, bool bUseArchetype, bool bForce = false);
	
	/** Get the runtime graph property type. */
	UScriptStruct* GetRuntimePropertyNodeType() const;
	
	/** Get the runtime graph property as a FStructProperty. */
	FStructProperty* GetRuntimePropertyNodeProperty() const;
	
	/** Return either a runtime property node only or an editor property node. */
	FStructProperty* GetPropertyNodeProperty(bool bRuntimeOnly) const;
	
	/** The template which owns this property. */
	USMNodeInstance* GetOwningTemplate() const;
	
	/** The blueprint of the template owning this property. */
	USMNodeBlueprint* GetTemplateBlueprint() const;
	
	/** The property graph containing this property node. */
	UEdGraph* GetOwningGraph() const;
	
	/** The state machine graph node which owns this property. */
	USMGraphNode_Base* GetOwningGraphNode() const;
	USMGraphNode_Base* GetOwningGraphNodeChecked() const;
	
	/** Open the property graph for this node. Should be the containing graph. */
	void JumpToPropertyGraph();
	
	/* Opens the template blueprint for this node. */
	void JumpToTemplateBlueprint();
	
	/** Get the widget to represent this property node within a state machine graph node. */
	virtual TSharedPtr<class SSMGraphProperty_Base> GetGraphNodeWidget() const;
	
	/** Get the widget to represent the graph node in the details panel.*/
	virtual TSharedPtr<SWidget> GetGraphDetailWidget() const;
	
	/** Widget used to go to the graph. */
	virtual TSharedPtr<SWidget> GetViewGraphDetailWidget() const;
	
	/** Widget used to toggled edit mode of the graph. */
	virtual TSharedPtr<SWidget> GetToggleEditGraphDetailWidget() const;
	
	/** This forwards up context menu actions when the owning state machine graph node is right clicked on over this property. */
	void GetContextMenuActionsForOwningNode(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class UToolMenu* ToolMenu, bool bIsDebugging) const;

	/** If the sgraphnode doesn't have a default name field then this property may be used as the default property to edit. */
	virtual bool IsConsideredForDefaultProperty() const { return false; }
	
	/** If the sgraphnode chooses this property as the default it will pass the already constructed widget in (should be the same type as GetGraphDetailWidget())
	 * so it can be used for a default action -- such as automatically editing text. */
	virtual void DefaultPropertyActionWhenPlaced(TSharedPtr<SWidget> Widget) {}

	/** Locates the result pin if one exists. */
	virtual UEdGraphPin* GetResultPin() const;
	UEdGraphPin* GetResultPinChecked() const { UEdGraphPin* Pin = GetResultPin(); check(Pin); return Pin; }

	/** Checks if the result pin has a connection going to it, making this a variable connection rather than a defaults setter. */
	virtual bool DoesResultPinHaveConnections() const;

	/** Checks if this instance matches the CDO default value. */
	virtual bool IsValueSetToDefault() const;

	/** Return bDefaultValueChanged which is true once any change has occurred. */
	bool HasDefaultValueExplicitlyBeenChanged() const { return bDefaultValueChanged; }

	/** Checks if the value has changed from the default or if a variable is wired to it. */
	bool IsValueModifiedOrWired() const;

	/** The last exported property text value. */
	const FString& GetLastAutoGeneratedDefaultValue() const { return LastAutoGeneratedDefaultValue; }

	struct FHighlightArgs
	{
		/** Enable highlighting for this property. */
		bool bEnable = false;

		/** Allow the compile process to clear the current highlight state. */
		bool bClearOnCompile = true;

		/** The color of the highlight. */
		FLinearColor Color = FLinearColor(1.f, 0.84f, 0.f, 1.2);
	};

	struct FNotifyArgs
	{
		bool bEnable = false;

		/** Allow the compile process to clear the current notify state. */
		bool bClearOnCompile = true;

		ESMLogType LogType;
		FString Message;
	};

	/** Set if the node should be highlighted. */
	void SetHighlightedArgs(const FHighlightArgs& InHighlightArgs);

	/** Return the current args used for highlighting. */
	const FHighlightArgs& GetHighlightArgs() const { return HighlightArgs; }

	/** Set if the node should display a notification. */
	void SetNotificationArgs(const FNotifyArgs& InNotifyArgs);

	/** Return the current args used for notification info. */
	const FNotifyArgs& GetNotifyArgs() const { return NotifyArgs; }

	/** For correcting the package and namespace on newly placed text properties. Based on EdGraphSchema_K2.cpp. */
	static void ConformLocalizationPackage(
		const FEdGraphPinType& PinType, 
		FString& InOutTextString, 
		const FText& DefaultTextValue,
		const UObject* Package
	);

	UPROPERTY()
	TObjectPtr<USMGraphNode_Base> OwningGraphNode;

	/** Set from slate widget representing this property. Used to help determine if the context menu creation should
	 * forward creation to this node as well.
	 * TODO: There has to be a better way of tracing the cursor to a widget. */
	mutable bool bMouseOverNodeProperty;

	DECLARE_MULTICAST_DELEGATE(FForceVisualRefresh);
	void ForceVisualRefresh() const { ForceVisualRefreshEvent.Broadcast(); }
	FForceVisualRefresh ForceVisualRefreshEvent;

protected:
	virtual void Internal_GetContextMenuActionsForOwningNode(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FToolMenuSection& MenuSection, bool bIsDebugging) const;

protected:
	/** Used to determine if the property should be highlighted in slate. */
	FHighlightArgs HighlightArgs;

	/** Used to determine if a property should have a notification icon and message. */
	FNotifyArgs NotifyArgs;

	/** The last default autogenerated value. The pin value resets to this if the default value hasn't changed. */
	UPROPERTY()
	FString LastAutoGeneratedDefaultValue;

	/** True once the user has changed the default value. It cannot become false again unless through Undo or Property Reset. */
	UPROPERTY()
	uint8 bDefaultValueChanged: 1;

	/** True only while setting default pin value from the property. */
	uint8 bGeneratedDefaultValueBeingSet: 1;

	/** True only while setting default property values from the pin. */
	uint8 bSettingPropertyDefaultsFromPin: 1;

	/** True only during a property reset. */
	uint8 bResettingProperty: 1;
};
