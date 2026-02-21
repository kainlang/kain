// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace LD
{
	namespace JsonFields
	{
		const FString FIELD_JSON_VERSION = TEXT("LogicDriverJsonVersion");
		const FString FIELD_NAME = TEXT("Name");
		const FString FIELD_PARENT_CLASS = TEXT("ParentClass");
		const FString FIELD_ROOT_GUID = TEXT("RootGuid");
		const FString FIELD_CDO = TEXT("Defaults");
		const FString FIELD_STATES = TEXT("States");
		const FString FIELD_TRANSITIONS = TEXT("Transitions");
		/** Entry nodes on SM graphs (contains parallel info) */
		const FString FIELD_ENTRY_NODES = TEXT("EntryNodes");

		/** Comment boxes */
		const FString FIELD_COMMENTS = TEXT("Comments");
		
		const FString FIELD_GRAPH_NODE_CLASS = TEXT("GraphNodeClass");
		/** A state is connected to an entry node. */
		const FString FIELD_CONNECTED_TO_ENTRY = TEXT("IsConnectedToEntry");

		const FString FIELD_OWNER_GUID = TEXT("OwnerGuid");
		const FString FIELD_NODE_GUID = TEXT("NodeGuid");

		/** Useful for cases where a specific k2 graph needs to be located within an owner guid, such as a PropertyGraph */
		const FString FIELD_GRAPH_NAME = TEXT("GraphName");

		const FString FIELD_FROM_GUID = TEXT("FromGuid");
		const FString FIELD_TO_GUID = TEXT("ToGuid");

		// Eval field that might be set to true.
		const FString FIELD_EVAL_DEFAULT = TEXT("EvalDefault");
	}

	namespace JsonUtils
	{
		SMASSETTOOLS_API UClass* GetClassFromStringField(TSharedPtr<FJsonObject> InJsonObject, const FString& InFieldName);
		SMASSETTOOLS_API UObject* GetObjectFromStringField(TSharedPtr<FJsonObject> InJsonObject, const FString& InFieldName);

		const int32 CurrentVersion = 1;
	}
}