// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "KismetCompilerMisc.h"

/**
 * Track unique names per object.
 *
 * Based on FNetNameMapping from KismetCompilerMisc.h.
 *
 * Property name generation may require multiple names assigned per object and we need to allow that, while keeping each
 * name unique. The built in engine method doesn't work for us as it will make a name unique once, but then return that
 * name when the same object is used.
 */
struct FSMNameMapping
{
public:
	// Come up with a valid, unique (within the scope of NameToNet) name based on an existing Net object and (optional) context.
	// The resulting name is stable across multiple calls if given the same pointer.
	FString MakeValidName(const UEdGraphNode* Net, const FString& Context = TEXT(""))
	{
		return MakeValidNameImpl(Net, Context);
	}

	FString MakeValidName(const UEdGraphPin* Net, const FString& Context = TEXT(""))
	{
		return MakeValidNameImpl(Net, Context);
	}

	FString MakeValidName(const UObject* Net, const FString& Context = TEXT(""))
	{
		return MakeValidNameImpl(Net, Context);
	}

private:
	static FString MakeBaseName(const UEdGraphNode* Net);
	static FString MakeBaseName(const UEdGraphPin* Net);
	static FString MakeBaseName(const UObject* Net);

	template <typename NetType>
	FString MakeValidNameImpl(NetType Net, const FString& Context)
	{
		FString BaseName = MakeBaseName(Net);
		if (!Context.IsEmpty())
		{
			BaseName += FString::Printf(TEXT("_%s"), *Context);
		}

		FString NetName = GetUniqueName(MoveTemp(BaseName));

		NameToNet.Add(NetName, Net);
		return NetName;
	}

	FString GetUniqueName(FString NetName)
	{
		FNodeHandlingFunctor::SanitizeName(NetName);
		FString NewNetName(NetName);

		int32 Postfix = 0;
		const void** ExistingNet = NameToNet.Find(NewNetName);
		while (ExistingNet && *ExistingNet)
		{
			++Postfix;
			NewNetName = NetName + TEXT("_") + FString::FromInt(Postfix);
			ExistingNet = NameToNet.Find(NewNetName);
		}

		return NewNetName;
	}

	TMap<FString, const void*> NameToNet;
};
