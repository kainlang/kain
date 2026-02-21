// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "Serialization/TextReferenceCollector.h"

namespace LD
{
	namespace TextUtils
	{
		/** Checks if localization settings and namespace and value are equivalent. Can be used for CDO->Instance propagation.  */
		SMSYSTEMEDITOR_API bool DoesTextValueAndLocalizationMatch(const FText& InTextA, const FText& InTextB);

		/** Checks if localization settings and namespace and value are equivalent from string exported values. Can be used for CDO->Instance propagation.  */
		SMSYSTEMEDITOR_API bool DoesTextValueAndLocalizationMatch(const FString& InStringBufferA, const FString& InStringBufferB);

		/** Convert FText to an exported string including localization data. */
		SMSYSTEMEDITOR_API FString TextToStringBuffer(const FText& InText);

		/** Convert exported string with localization to text. */
		SMSYSTEMEDITOR_API FText StringBufferToText(const FString& InString);

		/**
		 * Finds all impacted references of the given text, desired namespace, and desired key.
		 * If all references belong to a single property owner, they will then be set to the temporary key.
		 * Afterward the caller should set the text source key to the desired key if this returns true.
		 *
		 * @return True if there was one unique property owner and duplicate references such as from the CDO were renamed.
		 * If false then there were no references or multiple property owners. In this case UE should just handle the key change.
		 */
		SMSYSTEMEDITOR_API bool SetTemporaryKeyForTextReferences(const UObject* InSourceObject, const FString& InTextSource, const FString& InDesiredNamespace,
		                                                   const FString& InDesiredKey, const FString& InTemporaryKey);

		/**
		 * Finds and counts all persistent text references from within a package, based on FTextReferenceCollector.
		 * OutUniqueRefCount attempts to determine separate properties that use the same text. A single property
		 * may be referenced multiple times, such as in the BP pin, the node archetype, and the CDO.
		 */
		class SMSYSTEMEDITOR_API FSMTextReferenceCollector
		{
		public:
			FSMTextReferenceCollector(const UPackage* const InPackage,
				const FTextReferenceCollector::EComparisonMode InComparisonMode, const FString& InTextNamespace,
				const FString& InTextKey, const FString& InTextSource, int32& OutUniqueRefCount, TMap<UObject*, TArray<FText*>>& OutObjectsReferencing);
		};
	}
}