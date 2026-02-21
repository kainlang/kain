// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMCachedPropertyData.h"

#include "Misc/ScopeLock.h"

const TSet<FProperty*>* FSMCachedPropertyData::FindCachedProperties(const UClass* InClass)
{
	FScopeLock ScopedLock(&CriticalSection);
	return CachedProperties.Find(InClass);
}

void FSMCachedPropertyData::AddCachedProperties(const UClass* InClass, const TSet<FProperty*>& InProperties)
{
	FScopeLock ScopedLock(&CriticalSection);
	CachedProperties.Add(InClass, InProperties);
}

void FSMCachedPropertyData::SetMappedGraphPropertyInstances(
	const TMap<FGuid, FSMGraphProperty_Base_Runtime*>& InMappedGraphPropertyInstances)
{
	FScopeLock ScopedLock(&CriticalSection);
	MappedGraphPropertyInstances = InMappedGraphPropertyInstances;
}

const TMap<FGuid, FSMGraphProperty_Base_Runtime*>& FSMCachedPropertyData::GetMappedGraphPropertyInstances() const
{
	return MappedGraphPropertyInstances;
}
