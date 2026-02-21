// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMBlueprintDelegateNodeSpawner.h"

USMBlueprintDelegateNodeSpawner* USMBlueprintDelegateNodeSpawner::Create(TSubclassOf<UK2Node_BaseMCDelegate> NodeClass,
	FMulticastDelegateProperty const* const Property, UObject* Outer)
{
	check(Property != nullptr);
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	//--------------------------------------
	// Constructing the Spawner
	//--------------------------------------

	USMBlueprintDelegateNodeSpawner* NodeSpawner = NewObject<USMBlueprintDelegateNodeSpawner>(Outer);
	NodeSpawner->SetField(const_cast<FMulticastDelegateProperty*>(Property));
	NodeSpawner->NodeClass = NodeClass;

	//--------------------------------------
	// Default UI Signature
	//--------------------------------------

	// NO DEFAULT UI SIGNATURE! If this should be user facing, use the default engine class instead.
	
	//--------------------------------------
	// Post-Spawn Setup
	//--------------------------------------

	auto SetDelegateLambda = [](UEdGraphNode* NewNode, FFieldVariant InField)
	{
		FMulticastDelegateProperty const* MCDProperty = CastField<FMulticastDelegateProperty>(InField.ToField());

		UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(NewNode);
		if ((DelegateNode != nullptr) && (MCDProperty != nullptr))
		{
			UClass* OwnerClass = MCDProperty->GetOwnerClass();

			DelegateNode->SetFromProperty(MCDProperty, false, OwnerClass);
		}
	};
	NodeSpawner->SetNodeFieldDelegate = FSetNodeFieldDelegate::CreateStatic(SetDelegateLambda);

	return NodeSpawner;
}

FMulticastDelegateProperty const* USMBlueprintDelegateNodeSpawner::GetDelegateProperty() const
{
	return CastField<FMulticastDelegateProperty>(GetField().ToField());
}
