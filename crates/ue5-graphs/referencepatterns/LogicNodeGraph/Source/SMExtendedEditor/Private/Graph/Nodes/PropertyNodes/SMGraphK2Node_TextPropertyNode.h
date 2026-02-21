// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "SMTextGraphProperty.h"

#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"

#include "SMGraphK2Node_TextPropertyNode.generated.h"

class ITextDecorator;
class URichTextBlock;

UCLASS(MinimalAPI)
class USMGraphK2Node_TextPropertyNode : public USMGraphK2Node_PropertyNode_Base
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	FSMTextGraphProperty_Runtime RuntimeTextProperty;
	
	UPROPERTY(EditAnywhere, Category = "Graph Property")
	FSMTextGraphProperty TextProperty;

	// UObject
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	// ~UObject
	
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void PostReconstructNode() override;
	virtual void AddPinSearchMetaDataInfo(const UEdGraphPin* Pin, TArray<FSearchTagDataPair>& OutTaggedMetaData) const override;
	// ~UEdGraphNode

	// USMGraphK2Node_Base
	virtual void PreConsolidatedEventGraphValidate(FCompilerResultsLog& MessageLog) override;
	// ~USMGraphK2Node_Base

	// USMGraphK2Node_PropertyNode
	virtual void ConfigureRuntimePropertyNode() override;
	virtual FSMGraphProperty_Base_Runtime* GetRuntimePropertyNode() override;
	virtual FSMGraphProperty_Base* GetPropertyNode() override { return &TextProperty; }
	virtual void SetPropertyNode(FSMGraphProperty_Base* NewNode) override { TextProperty = *static_cast<FSMTextGraphProperty*>(NewNode); }
	virtual TSharedPtr<SSMGraphProperty_Base> GetGraphNodeWidget() const override;
	virtual bool IsConsideredForDefaultProperty() const override { return TextProperty.WidgetInfo.bConsiderForDefaultWidget; }
	virtual void DefaultPropertyActionWhenPlaced(TSharedPtr<SWidget> Widget) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void SetPropertyDefaultsFromPin() override;
	virtual void SetPinValueFromPropertyDefaults(bool bUpdateTemplateDefaults, bool bUseArchetype, bool bForce) override;
	virtual bool IsValueSetToDefault() const override;
	
protected:
	virtual void Internal_GetContextMenuActionsForOwningNode(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FToolMenuSection& MenuSection, bool bIsDebugging) const override;
	// ~USMGraphK2Node_PropertyNode

public:
	/** Return the correct text graph property, accounting for arrays. If the array has 0 elements nullptr will be returned. */
	FSMTextGraphProperty* GetTextGraphProperty(USMNodeInstance* Template) const;

	/** Deep compares text and determines if the default value has changed. */
	void NotifyFormatTextUpdated(const FText& NewText);

	/** Create decorators for rich text styling if required. */
	void CreateDecorators(TArray<TSharedRef<ITextDecorator>>& OutDecorators);

private:
	/** Dummy object to be passed as an owner to decorators. */
	UPROPERTY(Transient)
	TObjectPtr<URichTextBlock> RichTextBlockDummyOwner;

	/** Rich style decorators. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URichTextBlockDecorator>> RichStyleInstanceDecorators;

	/** True only for versions prior to supporting text graph defaults. */
	uint8 bUpgradingToSupportDefaults: 1;
};
