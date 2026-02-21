// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSMGraphProperty_Base_Runtime;

/**
 * Thread safe cache to store all required graph properties for quick retrieval during initialization.
 */
class FSMCachedPropertyData
{
public:
	/** Locate all cached properties for a given class. */
	const TSet<FProperty*>* FindCachedProperties(const UClass* InClass);

	/** Add a class's properties to the cache. */
	void AddCachedProperties(const UClass* InClass, const TSet<FProperty*>& InProperties);

	/** Set an entire map of guids to a run time property instance. */
	void SetMappedGraphPropertyInstances(const TMap<FGuid, FSMGraphProperty_Base_Runtime*>& InMappedGraphPropertyInstances);

	/** Retrieve the graph property instance map. */
	const TMap<FGuid, FSMGraphProperty_Base_Runtime*>& GetMappedGraphPropertyInstances() const;

private:
	/** Class to graph FProperties. */
	TMap<const UClass*, TSet<FProperty*>> CachedProperties;

	/** Property guid to graph runtime instances.  */
	TMap<FGuid, FSMGraphProperty_Base_Runtime*> MappedGraphPropertyInstances;

	FCriticalSection CriticalSection;
};