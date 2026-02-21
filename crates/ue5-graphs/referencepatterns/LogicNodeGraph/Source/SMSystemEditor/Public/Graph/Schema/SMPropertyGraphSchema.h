// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Graph/Schema/SMGraphK2Schema.h"

#include "SMPropertyGraphSchema.generated.h"

UCLASS()
class SMSYSTEMEDITOR_API USMPropertyGraphSchema : public USMGraphK2Schema
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphSchema_K2
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual bool CanDuplicateGraph(UEdGraph* InSourceGraph) const override;
	virtual void HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const override;
	/** This isn't currently called by UE4. */
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	// ~UEdGraphSchema_K2
};
