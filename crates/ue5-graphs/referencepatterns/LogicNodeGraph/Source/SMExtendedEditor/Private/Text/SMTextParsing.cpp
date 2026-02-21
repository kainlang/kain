// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTextParsing.h"
#include "SMRunTypes.h"
#include "Utilities/SMTextGraphUtils.h"

#include "Utilities/SMBlueprintEditorUtils.h"

#include "Framework/Text/IRichTextMarkupWriter.h"
#include "Framework/Text/RichTextMarkupProcessing.h"
#include "Kismet2/Kismet2NameValidators.h"

SMTextParser::FParserResults SMTextParser::ConvertToRichText(const FText& InText, UBlueprint* InBlueprint,
                                                             TMap<FName, FGuid>* ExistingVariables, TMap<FName, FGuid>* ExistingFunctions)
{
	static TSharedRef<FDefaultRichTextMarkupWriter> MarkupWriter = FDefaultRichTextMarkupWriter::GetStaticInstance();

	TSharedRef<FDefaultRichTextMarkupParser> RichTextMarkupProcessing = FDefaultRichTextMarkupParser::GetStaticInstance();

	TArray<IRichTextMarkupWriter::FRichTextLine> Line;
	Line.Add(
		IRichTextMarkupWriter::FRichTextLine
		{
			{
				IRichTextMarkupWriter::FRichTextRun(FRunInfo(), TEXT(""))
			}
		}
	);

	// Let unreal process any actual rich style first.
	TArray<FTextLineParseResults> RichLineResultsArray;
	FString RichStyleString;

	// Determine the newline characters to use later. Just find the first occurence.
	const TCHAR* NewLineCharacter = nullptr;
	{
		const TCHAR* const InputStart = *InText.ToString();
		for (const TCHAR* CurrentChar = InputStart; CurrentChar && *CurrentChar; ++CurrentChar)
		{
			if (*CurrentChar == '\r' && *(CurrentChar + 1) == '\n')
			{
				// Windows new line.
				NewLineCharacter = TEXT("\r\n");
				break;
			}

			if (*CurrentChar == '\n')
			{
				NewLineCharacter = TEXT("\n");
				break;
			}

			if (*CurrentChar == '\r')
			{
				NewLineCharacter = TEXT("\r");
				break;
			}
		}

		if (!NewLineCharacter)
		{
			// Generic newline.
			NewLineCharacter = TEXT("\n");
		}
	}

	RichTextMarkupProcessing->Process(RichLineResultsArray, InText.ToString(), RichStyleString);

	FParserResults Result;

	auto IsOpeningBracket = [&](const TCHAR& InChar, const int32 InIdx) -> bool
	{
		return InChar == '{' && !(InIdx > 0 && RichStyleString[InIdx - 1] == '`');
	};

	auto HandleError = [&]()
	{
		Result.bErrorProcessing = true;
		Result.PlainText = InText;
		Result.RichText = InText;
	};

	int32 OpeningBracket = INDEX_NONE;
	int32 OpeningBracketOnLine = INDEX_NONE;
	TArray<FString> FormattedRichLines; // Rich text with variable and function runs applied.
	TArray<FString> FormattedPlainLines; // Plain text that has {Var} formatting applied.
	TArray<FString> CurrentNameAsStrArray; // Only if a name is broken into multiple lines for some reason.

	for (int32 LineIdx = 0; LineIdx < RichLineResultsArray.Num(); ++LineIdx)
	{
		FTextLineParseResults& CurrentLine = RichLineResultsArray[LineIdx];

		FString CurrentFormattedRichString;
		FString CurrentFormattedPlainString;
		FString CurrentNameAsStr;
		for (int32 CharIdx = CurrentLine.Range.BeginIndex; CharIdx < CurrentLine.Range.EndIndex; ++CharIdx)
		{
			const TCHAR& CurrentChar = RichStyleString[CharIdx];

			// Start of variable. The key ` should prevent this from being parsed according to UE4 spec.
			if (IsOpeningBracket(CurrentChar, CharIdx))
			{
				if (OpeningBracket != INDEX_NONE)
				{
					// We've hit an opening bracket without a close. Cancel everything.
					HandleError();
					return Result;
				}

				CurrentNameAsStr.Empty();
				CurrentNameAsStrArray.Empty();
				OpeningBracket = CharIdx;
				OpeningBracketOnLine = LineIdx;
			}
			// Looking for variable
			else if (OpeningBracket >= 0)
			{
				// End of variable
				if (CurrentChar == '}')
				{
					FRunInfo RunInfo;

					const bool bMultiLineError = CurrentNameAsStrArray.Num() > 0;

					FString VariableNameAsString = bMultiLineError ? FString::Join(CurrentNameAsStrArray, NewLineCharacter) + CurrentNameAsStr : CurrentNameAsStr;
					const FName VariableName = FName(*VariableNameAsString);

					UFunction* Function = FindFunction(VariableName, InBlueprint, ExistingFunctions);
					if (!bMultiLineError && Function)
					{
						RunInfo = FRunTypeUtils::CreateFunctionRunInfo(Function);
						VariableNameAsString = Function->GetName();
						Result.Functions.Add(Function);
					}
					else
					{
						FBPVariableDescription Variable;
						bool bIsVarNameInvalid = false;
						bool bVarExists = !bMultiLineError && FindVariable(VariableName, InBlueprint, ExistingVariables, Variable, bIsVarNameInvalid);
						if (bVarExists)
						{
							if (VariableName != Variable.VarName)
							{
								Result.bVarRenamed = true;
							}

							VariableNameAsString = Variable.VarName.ToString();
							Result.Variables.Add(Variable.VarName, Variable.VarGuid);
						}
						else
						{
							// Invalid, just add what was there originally.
							if (bIsVarNameInvalid)
							{
								VariableNameAsString = "";
								CurrentFormattedPlainString.Append(*RichStyleString, CharIdx - OpeningBracket);
								CurrentFormattedRichString.Append(*RichStyleString, CharIdx - OpeningBracket);
							}

							Result.Variables.Add(VariableName, FGuid());

							// Check for a multi line variable and restore all original text.
							if (bMultiLineError)
							{
								check(OpeningBracketOnLine >= 0 && OpeningBracketOnLine < FormattedPlainLines.Num());
								check(OpeningBracketOnLine >= 0 && OpeningBracketOnLine < FormattedRichLines.Num());
								FormattedPlainLines[OpeningBracketOnLine].Append(TEXT("{"));
								FormattedRichLines[OpeningBracketOnLine].Append(TEXT("{"));

								int32 NameIdx = 0;
								for (int32 Idx = OpeningBracketOnLine; Idx < LineIdx; ++Idx)
								{
									check(NameIdx < CurrentNameAsStrArray.Num());
									FormattedPlainLines[Idx].Append(CurrentNameAsStrArray[NameIdx]);
									FormattedRichLines[Idx].Append(CurrentNameAsStrArray[NameIdx]);
									NameIdx++;
								}
							}
						}

						RunInfo = FRunTypeUtils::CreatePropertyRunInfo(FName(*VariableNameAsString), bVarExists ? &Variable : nullptr);
					}

					const FString FormattedString = bMultiLineError ?
						FString::Printf(TEXT("%s}"), *CurrentNameAsStr) : // Error -- finish printing.
						FString::Printf(TEXT("{%s}"), *VariableNameAsString); // Normal

					// Plain text is just the variable name in case it has changed.
					CurrentFormattedPlainString.Append(FormattedString);

					if (bMultiLineError || CurrentLine.Runs.ContainsByPredicate([&](const FTextRunParseResults& RichRun)
					{
						return RichRun.ContentRange.Contains(CharIdx);
					}))
					{
						// This is part of the a rich text run already. We can't display our custom rich because UE doesn't
						// support nested runs.
						CurrentFormattedRichString.Append(FormattedString);
					}
					else
					{
						// Display fancy rich text for our variable or function.

						IRichTextMarkupWriter::FRichTextLine RichLine;
						TArray<IRichTextMarkupWriter::FRichTextLine> RichLines;

						RichLine.Runs.Add(IRichTextMarkupWriter::FRichTextRun(RunInfo, TEXT("")));
						RichLines.Add(RichLine);

						MarkupWriter->Write(RichLines, CurrentFormattedRichString);
					}
					OpeningBracket = INDEX_NONE;
					continue;
				}

				CurrentNameAsStr += CurrentChar;
			}
			// Regular character.
			else
			{
				CurrentFormattedPlainString += CurrentChar;
				CurrentFormattedRichString += CurrentChar;
			}
		}

		// If the name hasn't finished processing then a newline was added and the variable parsing will fail.
		// It will be printed out with standard text formatting and no run.
		CurrentNameAsStrArray.Add(CurrentNameAsStr);

		FormattedPlainLines.Add(MoveTemp(CurrentFormattedPlainString));
		FormattedRichLines.Add(MoveTemp(CurrentFormattedRichString));
	}

	if (OpeningBracket >= 0)
	{
		// Bracket never closed.
		HandleError();
		return Result;
	}

	// Avoid using platform specific LINE_TERMINATOR macro. We want the stored data to be consistent across platforms.
	// This should keep behavior similar to older versions where the string wasn't broken apart and rejoined.
	Result.PlainText = FText::FromString(FString::Join(FormattedPlainLines, NewLineCharacter));
	Result.RichText = FText::FromString(FString::Join(FormattedRichLines, NewLineCharacter));
	return Result;
}

bool SMTextParser::FindVariable(const FName& VarName, UBlueprint* InBlueprint, TMap<FName, FGuid>* ExistingVariables,
	FBPVariableDescription& OutVariable, bool& bIsNameInvalid)
{
	bool bVarExists = false;
	if (ExistingVariables != nullptr)
	{
		// Find a saved variable for the current written var name.
		if (FGuid* ExistingGuid = ExistingVariables->Find(VarName))
		{
			// We found one, let's check if this original guid still exists.
			bVarExists = FSMBlueprintEditorUtils::TryGetVariableByGuid(InBlueprint, *ExistingGuid, OutVariable);
		}
	}

	// Find the current variable based on the current name.
	if (!bVarExists)
	{
		if (IsVariableNameValid(VarName.ToString()))
		{
			bVarExists = FSMBlueprintEditorUtils::TryGetVariableByName(InBlueprint, VarName, OutVariable);
			if (!bVarExists)
			{
				// Check for a native property and create a FBPVariableDescription if it exists.
				if (const FProperty* NativeProperty = FSMBlueprintEditorUtils::GetPropertyByName(InBlueprint, VarName))
				{
					OutVariable.VarName = NativeProperty->GetFName();
					// Provide a guid so IsRunValid returns true.
					OutVariable.VarGuid = FSMTextGraphUtils::GetNativePropertyGuid();
					{
						FEdGraphPinType PinType;
						const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
						Schema->ConvertPropertyToPinType(NativeProperty, PinType);
						OutVariable.VarType = MoveTemp(PinType);
					}
					bVarExists = true;
				}
			}
		}
		else
		{
			bIsNameInvalid = true;
		}
	}

	return bVarExists;
}

UFunction* SMTextParser::FindFunction(const FName& Name, UBlueprint* InBlueprint, TMap<FName, FGuid>* ExistingFunctions)
{
	// Look for an existing function which may have been renamed.
	if (ExistingFunctions != nullptr)
	{
		if (FGuid* ExistingGuid = ExistingFunctions->Find(Name))
		{
			const FName FunctionName = InBlueprint->GetFunctionNameFromClassByGuid(InBlueprint->SkeletonGeneratedClass, *ExistingGuid);
			if (!FunctionName.IsNone())
			{
				if (UFunction* Function = FindUField<UFunction>(InBlueprint->SkeletonGeneratedClass, FunctionName))
				{
					return Function;
				}
			}
		}
	}
	
	UFunction* Function = FindUField<UFunction>(InBlueprint->SkeletonGeneratedClass, Name);
	if (!Function)
	{
		Function = FMemberReference::FindRemappedField<UFunction>(InBlueprint->GeneratedClass, Name);
	}
	
	return Function;
}

bool SMTextParser::IsVariableNameValid(const FString& InName)
{
	return FName::IsValidXName(InName, UE_BLUEPRINT_INVALID_NAME_CHARACTERS);
}
