#include "SMJsonUtils.h"

#include "Dom/JsonObject.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

UClass* LD::JsonUtils::GetClassFromStringField(TSharedPtr<FJsonObject> InJsonObject, const FString& InFieldName)
{
	FString GraphNodeClassString;
	if (ensure(InJsonObject->TryGetStringField(InFieldName, GraphNodeClassString)))
	{
		const TSoftClassPtr<UObject> GraphNodeSoftClassPtr(GraphNodeClassString);
		const TSubclassOf<UObject> GraphNodeClass = GraphNodeSoftClassPtr.LoadSynchronous();
		if (ensureMsgf(GraphNodeClass, TEXT("Could not load class %s."), *GraphNodeClassString))
		{
			return GraphNodeClass;
		}
	}

	return nullptr;
}

UObject* LD::JsonUtils::GetObjectFromStringField(TSharedPtr<FJsonObject> InJsonObject, const FString& InFieldName)
{
	FString BlueprintString;
	if (ensure(InJsonObject->TryGetStringField(InFieldName, BlueprintString)))
	{
		if (!BlueprintString.IsEmpty() && !FName(BlueprintString).IsNone())
		{
			const TSoftObjectPtr<UObject> SoftPtr(BlueprintString);
			UObject* Object = SoftPtr.LoadSynchronous();
			if (ensureMsgf(Object, TEXT("Could not load object %s."), *BlueprintString))
			{
				return Object;
			}
		}
	}

	return nullptr;
}
