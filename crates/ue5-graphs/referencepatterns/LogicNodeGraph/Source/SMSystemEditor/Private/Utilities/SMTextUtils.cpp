// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "Utilities/SMTextUtils.h"

#include "SMInstance.h"

#include "Internationalization/TextNamespaceUtil.h"

bool LD::TextUtils::DoesTextValueAndLocalizationMatch(const FText& InTextA, const FText& InTextB)
{
	if (InTextA.IsCultureInvariant() == InTextB.IsCultureInvariant())
	{
		auto CompareTextValue = [&]()
		{
			const bool bResult = InTextA.EqualTo(InTextB, ETextComparisonLevel::Quinary);
			return bResult;
		};

		if (InTextA.IsCultureInvariant())
		{
			// Native culture data may be present in one, but if both are invariant just compare the text.
			return CompareTextValue();
		}

		const FString FullNamespaceA = FTextInspector::GetNamespace(InTextA).Get(FString());
		const FString StrippedNamespaceA = TextNamespaceUtil::StripPackageNamespace(FullNamespaceA);
		
		const FString FullNamespaceB = FTextInspector::GetNamespace(InTextB).Get(FString());
		const FString StrippedNamespaceB = TextNamespaceUtil::StripPackageNamespace(FullNamespaceB);
					
		if (StrippedNamespaceA == StrippedNamespaceB)
		{
			const FString KeyA = FTextInspector::GetKey(InTextA).Get(FString());
			const FString KeyB = FTextInspector::GetKey(InTextB).Get(FString());
			
			if ((FullNamespaceA.IsEmpty() && FullNamespaceA == FullNamespaceB) || KeyA == KeyB)
			{
				// Empty namespace can indicate a native property without LOCTEXT that has a key set
				// automatically. Treat this as a namespace match.
				
				return CompareTextValue();
			}
		}
	}

	return false;
}

bool LD::TextUtils::DoesTextValueAndLocalizationMatch(const FString& InStringBufferA, const FString& InStringBufferB)
{
	const FText TextA = StringBufferToText(InStringBufferA);
	const FText TextB = StringBufferToText(InStringBufferB);
	return DoesTextValueAndLocalizationMatch(TextA, TextB);
}

FString LD::TextUtils::TextToStringBuffer(const FText& InText)
{
	FString StringValue;
	FTextStringHelper::WriteToBuffer(StringValue, InText, false);
	return MoveTemp(StringValue);
}

FText LD::TextUtils::StringBufferToText(const FString& InString)
{
	FText Text;
	const TCHAR* Success = FTextStringHelper::ReadFromBuffer(*InString, Text);
	check(Success);

	return MoveTemp(Text);
}

bool LD::TextUtils::SetTemporaryKeyForTextReferences(const UObject* InSourceObject, const FString& InTextSource,
                                               const FString& InDesiredNamespace, const FString& InDesiredKey, const FString& InTemporaryKey)
{
	check(InSourceObject);

	int32 RefCount = 0;
	TMap<UObject*, TArray<FText*>> ObjectsReferencing;
	// The desired key, objects found here need to have their key changed temporarily.
	{
		FSMTextReferenceCollector(InSourceObject->GetPackage(),
			FTextReferenceCollector::EComparisonMode::MatchId, InDesiredNamespace, InDesiredKey, InTextSource,
			RefCount, ObjectsReferencing);
	}

	// The temporary key, used only in cases where UE has already changed the key but we need to track the reference
	// count. IE this text should be considered to be part of the desired key references.
	{
		TMap<UObject*, TArray<FText*>> ObjectsReferencingTemp;
		int32 RefCountTemp = 0;
		FSMTextReferenceCollector(InSourceObject->GetPackage(),
			FTextReferenceCollector::EComparisonMode::MatchId, InDesiredNamespace, InTemporaryKey, InTextSource,
			RefCountTemp, ObjectsReferencingTemp);

		RefCount += RefCountTemp;
	}

	// A ref count of 1 implies the user only referenced this text once, but it may be referenced internally
	// multiple times preventing the original key from persisting.
	if (RefCount == 1)
	{
		for (TTuple<UObject*, TArray<FText*>>& KeyVal : ObjectsReferencing)
		{
			if (KeyVal.Key == InSourceObject)
			{
				// This reference which is allowed.
				continue;
			}
			for (FText* Text : KeyVal.Value)
			{
				// Change all other references to the new key so we can persist the old key.
				*Text = FText::ChangeKey(InDesiredNamespace, InTemporaryKey, *Text);
			}
		}

		return true;
	}

	return false;
}

class FSMTextReferencesArchive : public FArchiveUObject
{
public:
	FSMTextReferencesArchive(const UPackage* const InPackage,
		const FTextReferenceCollector::EComparisonMode InComparisonMode, const FString& InTextNamespace,
		const FString& InTextKey, const FString& InTextSource, int32& OutUniqueRefCount)
		: ComparisonMode(InComparisonMode)
		, NamespaceToMatch(&InTextNamespace)
		, KeyToMatch(&InTextKey)
		, SourceToMatch(&InTextSource)
		, Count(&OutUniqueRefCount)
	{
		this->SetIsSaving(true);
		this->SetIsPersistent(true); // Skips transient properties
		ArShouldSkipBulkData = true; // Skips bulk data as we can't handle saving that!

		// Build up the list of objects that are within our package - we won't follow object references to things outside of our package
		{
			TArray<UObject*> AllObjectsInPackageArray;
			GetObjectsWithOuter(InPackage, AllObjectsInPackageArray, true, RF_Transient, EInternalObjectFlags::Garbage);

			AllObjectsInPackage.Reserve(AllObjectsInPackageArray.Num());
			for (UObject* Object : AllObjectsInPackageArray)
			{
				AllObjectsInPackage.Add(Object);
			}
		}

		TArray<UObject*> RootObjectsInPackage;
		GetObjectsWithOuter(InPackage, RootObjectsInPackage, false, RF_Transient, EInternalObjectFlags::Garbage);

		// Iterate over each root object in the package
		for (UObject* Obj : RootObjectsInPackage)
		{
			ProcessObject(Obj);
		}
	}

	void ProcessObject(UObject* Obj)
	{
		if (!Obj || !AllObjectsInPackage.Contains(Obj) || ProcessedObjects.Contains(Obj))
		{
			return;
		}

		ProcessedObjects.Add(Obj);
		CurrentObject = Obj;

		Obj->Serialize(*this);
	}

	using FArchiveUObject::operator<<; // For visibility of the overloads we don't override

	virtual FArchive& operator<<(UObject*& Obj) override
	{
		ProcessObject(Obj);
		return *this;
	}

	virtual FArchive& operator<<(FText& Value) override
	{
		const FString TextNamespace = FTextInspector::GetNamespace(Value).Get(FString());
		const FString TextKey = FTextInspector::GetKey(Value).Get(FString());
		if (TextNamespace.Equals(*NamespaceToMatch, ESearchCase::CaseSensitive) && TextKey.Equals(*KeyToMatch, ESearchCase::CaseSensitive))
		{
			check(ComparisonMode == FTextReferenceCollector::EComparisonMode::MatchId);

			TArray<FText*>& TextArray = ObjectToText.FindOrAdd(CurrentObject);
			TextArray.Add(&Value);

			// Just skip the CDO, it will contain additional duplicates and the archetypes found in the blueprint will
			// give an accurate representation.
			const USMInstance* CDO = CurrentObject->GetTypedOuter<USMInstance>();
			if (!CDO || !CDO->HasAnyFlags(RF_ClassDefaultObject))
			{
				int32& ClassCount = ProcessedClasses.FindOrAdd(CurrentObject->GetClass());
				ClassCount++;

				if (ClassCount > *Count)
				{
					*Count = ClassCount;
				}
			}
		}

		return *this;
	}

public:
	TMap<UObject*, TArray<FText*>> ObjectToText;

private:
	FTextReferenceCollector::EComparisonMode ComparisonMode;
	const FString* NamespaceToMatch;
	const FString* KeyToMatch;
	const FString* SourceToMatch;
	int32* Count;

	UObject* CurrentObject = nullptr;

	TSet<const UObject*> AllObjectsInPackage;
	TSet<const UObject*> ProcessedObjects;
	TMap<const UClass*, int32> ProcessedClasses;
};

LD::TextUtils::FSMTextReferenceCollector::FSMTextReferenceCollector(const UPackage* const InPackage,
	const FTextReferenceCollector::EComparisonMode InComparisonMode, const FString& InTextNamespace,
	const FString& InTextKey, const FString& InTextSource, int32& OutUniqueRefCount, TMap<UObject*, TArray<FText*>>& OutObjectsReferencing)
{
	FSMTextReferencesArchive Archive(InPackage, InComparisonMode, InTextNamespace, InTextKey, InTextSource, OutUniqueRefCount);
	OutObjectsReferencing = MoveTemp(Archive.ObjectToText);
}
