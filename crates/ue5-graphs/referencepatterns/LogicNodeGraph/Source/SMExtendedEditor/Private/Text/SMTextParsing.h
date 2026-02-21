// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"

class SMTextParser
{
public:
	struct FParserResults
	{
		/** Variables replaced with rich text formatting. */
		FText RichText;
		/** Plain text updated if the variable name has changed. */
		FText PlainText;
		/** Variables found mapped with their names to their guids. */
		TMap<FName, FGuid> Variables;
		/** Functions found. */
		TSet<UFunction*> Functions;
		/** True if a variable was renamed. */
		bool bVarRenamed = false;
		/** If an error occurred during processing. */
		bool bErrorProcessing = false;
	};

	/** Take plain text and convert it to a rich text format for supported variables as well as update the plain text variable references. */
	static FParserResults ConvertToRichText(const FText& InText, UBlueprint* InBlueprint,
		TMap<FName, FGuid>* ExistingVariables = nullptr, TMap<FName, FGuid>* ExistingFunctions = nullptr);

	static bool FindVariable(const FName& VarName, UBlueprint* InBlueprint, TMap<FName, FGuid>* ExistingVariables, FBPVariableDescription& OutVariable, bool& bIsNameInvalid);
	static UFunction* FindFunction(const FName& Name, UBlueprint* InBlueprint, TMap<FName, FGuid>* ExistingFunctions);
	
	/** Standard UE4 var name check. */
	static bool IsVariableNameValid(const FString& InName);
};
