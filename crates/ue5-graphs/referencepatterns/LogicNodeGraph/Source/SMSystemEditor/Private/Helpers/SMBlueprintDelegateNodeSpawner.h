// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "BlueprintFieldNodeSpawner.h"
#include "K2Node_BaseMCDelegate.h"

#include "SMBlueprintDelegateNodeSpawner.generated.h"

/**
 * Based on UBlueprintDelegateNodeSpawner, but avoids any UI specific routines that might load the documentation module,
 * so this can be safely used for intermediate node creation in uncooked only builds without the editor.
 */
UCLASS(Transient)
class USMBlueprintDelegateNodeSpawner : public UBlueprintFieldNodeSpawner
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new USMBlueprintDelegateNodeSpawner for the specified property.
	 * Does not do any compatibility checking to ensure that the property is
	 * accessible from blueprints (do that before calling this).
	 *
	 * @param  NodeClass The node type that you want the spawner to spawn.
	 * @param  Property  The property you want assigned to spawned nodes.
	 * @param  Outer     Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static USMBlueprintDelegateNodeSpawner* Create(TSubclassOf<UK2Node_BaseMCDelegate> NodeClass, FMulticastDelegateProperty const* const Property, UObject* Outer = nullptr);

	/**
	 * Accessor to the delegate property that this spawner wraps (the delegate
	 * that this will assign spawned nodes with).
	 *
	 * @return The delegate property that this was initialized with.
	 */
	FMulticastDelegateProperty const* GetDelegateProperty() const;
};