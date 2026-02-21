// Copyright Narrative Tools 2022. 


#include "DialogueBlueprintCompiler.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "DialogueGraphSchema.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "DialogueEditorSettings.h"

#define LOCTEXT_NAMESPACE "DialogueBlueprintCompiler"

FDialogueBlueprintCompilerContext::FDialogueBlueprintCompilerContext(UDialogueBlueprint* SourceDialogueBP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
	: Super(SourceDialogueBP, InMessageLog, InCompilerOptions)
	, NewDialogueBlueprintClass(nullptr)
	, DialogueSchema(nullptr)
{
}

FDialogueBlueprintCompilerContext::~FDialogueBlueprintCompilerContext()
{
}

void FDialogueBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{

	NewDialogueBlueprintClass = FindObject<UDialogueBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (NewDialogueBlueprintClass == nullptr)
	{
		NewDialogueBlueprintClass = NewObject<UDialogueBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewDialogueBlueprintClass);
	}
	NewClass = NewDialogueBlueprintClass;
}

void FDialogueBlueprintCompilerContext::OnNewClassSet(UBlueprintGeneratedClass* ClassToUse)
{
	NewDialogueBlueprintClass = Cast<UDialogueBlueprintGeneratedClass>(ClassToUse);
}

void FDialogueBlueprintCompilerContext::PrecompileFunction(FKismetFunctionContext& Context, EInternalCompilerFlags InternalFlags)
{
	Super::PrecompileFunction(Context, InternalFlags);
}

void FDialogueBlueprintCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOutOldCDO)
{
	UDialogueBlueprint* DialogueBP = GetDialogueBlueprint();

	const bool bRecompilingOnLoad = Blueprint->bIsRegeneratingOnLoad;
	auto RenameObjectToTransientPackage = [bRecompilingOnLoad](UObject* ObjectToRename, const FName BaseName, bool bClearFlags)
	{
		const ERenameFlags RenFlags = REN_DontCreateRedirectors | (bRecompilingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_NonTransactional | REN_DoNotDirty;

		if (BaseName.IsNone())
		{
			ObjectToRename->Rename(nullptr, GetTransientPackage(), RenFlags);
		}
		else
		{
			FName TransientArchetypeName = MakeUniqueObjectName(GetTransientPackage(), ObjectToRename->GetClass(), BaseName);
			ObjectToRename->Rename(*TransientArchetypeName.ToString(), GetTransientPackage(), RenFlags);
		}

		ObjectToRename->SetFlags(RF_Transient);

		if (bClearFlags)
		{
			ObjectToRename->ClearFlags(/*RF_Public | */RF_Standalone | RF_ArchetypeObject);
		}
		FLinkerLoad::InvalidateExport(ObjectToRename);
	};

	if (!Blueprint->bIsRegeneratingOnLoad && bIsFullCompile)
	{
		if (UDialogueBlueprintGeneratedClass* WBC_ToClean = Cast<UDialogueBlueprintGeneratedClass>(ClassToClean))
		{
			if (UDialogue* OldDialogueTemplate = WBC_ToClean->GetDialogueTemplate())
			{
				FString TransientArchetypeString = FString::Printf(TEXT("OLD_Dialogue_TEMPLATE%s"), *OldDialogueTemplate->GetName());
				RenameObjectToTransientPackage(OldDialogueTemplate, *TransientArchetypeString, true);


				WBC_ToClean->SetDialogueTemplate(nullptr);
			}
		}
	}

	//TODO maybe remove unused states and branches
	Super::CleanAndSanitizeClass(ClassToClean, InOutOldCDO);
}

void FDialogueBlueprintCompilerContext::SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean)
{
	Super::SaveSubObjectsFromCleanAndSanitizeClass(SubObjectsToSave, ClassToClean);

	check(ClassToClean == NewClass);
	NewDialogueBlueprintClass = Cast<UDialogueBlueprintGeneratedClass>((UObject*)NewClass);

	UDialogueBlueprint* DialogueBP = GetDialogueBlueprint();
	SubObjectsToSave.AddObject(DialogueBP->DialogueTemplate);
}

void FDialogueBlueprintCompilerContext::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if (TargetUClass && !((UObject*)TargetUClass)->IsA(UDialogueBlueprintGeneratedClass::StaticClass()))
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = nullptr;
	}
}


void FDialogueBlueprintCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	FKismetCompilerContext::CopyTermDefaultsToDefaultObject(DefaultObject);
}

void FDialogueBlueprintCompilerContext::FinishCompilingClass(UClass* Class)
{
	UDialogueBlueprint* DialogueBP = GetDialogueBlueprint();
	UDialogueBlueprintGeneratedClass* BPGClass = Cast<UDialogueBlueprintGeneratedClass>(Class); // why is this not a Dialogue blueprint generated class! 

	UClass* ParentClass = DialogueBP->ParentClass;
	const bool bIsSkeletonOnly = CompileOptions.CompileType == EKismetCompileType::SkeletonOnly;

	if (!bIsSkeletonOnly)
	{
		if (!DialogueBP->bHasBeenRegenerated)
		{
			UBlueprint::ForceLoadMembers(DialogueBP->DialogueTemplate);
		}

		FixAbandonedDialogueTemplate(DialogueBP);

		if (BPGClass && DialogueBP->DialogueTemplate)
		{
			// Need to clear archetype flag before duplication as we check during dup to see if we should postload
			EObjectFlags PreviousFlags = DialogueBP->DialogueTemplate->GetFlags();
			DialogueBP->DialogueTemplate->ClearFlags(RF_ArchetypeObject);

			UDialogue* NewDialogueTemplate = Cast<UDialogue>(StaticDuplicateObject(DialogueBP->DialogueTemplate, BPGClass, NAME_None, RF_AllFlags & ~RF_DefaultSubObject));
			BPGClass->SetDialogueTemplate(NewDialogueTemplate);

			DialogueBP->DialogueTemplate->SetFlags(PreviousFlags);


			if (const UDialogueEditorSettings* DialogueSettings = GetDefault<UDialogueEditorSettings>())
			{
				//Ensure we don't have duplicate IDs on any nodes. Map ID -> Node. If we find a key that already exists we have a duplicate
				TMap<FName, UDialogueNode*> NodeMap;

				for (auto& Node : DialogueBP->DialogueTemplate->GetNodes())
				{
					if (Node)
					{
						//Always warn duplicate IDs even if warnings are disabled since this is a critical error 
						if (NodeMap.Find(Node->GetID()))
						{
							FText DuplicateIDError = FText::Format(LOCTEXT("DuplicateDialogueIDsFound", "Found duplicate ID {0} on nodes {1} and {2}."), FText::FromString(Node->GetID().ToString()), FText::FromString(GetNameSafe(Node)), FText::FromString(GetNameSafe(NodeMap[Node->GetID()])));
							MessageLog.Error(*DuplicateIDError.ToString());
						}
						else
						{
							NodeMap.Add(Node->GetID(), Node);
						}

						if (DialogueSettings->bEnableWarnings)
						{
							if (DialogueSettings->bWarnMissingSoundCues)
							{
								if (!Node->Line.Text.IsEmpty() && !Node->Line.DialogueSound)
								{
									FText MissingSoundWarning = FText::Format(LOCTEXT("MissingSoundWarning", "Found node {0} that is missing audio cues. Turn off bWarnMissingSoundCues in your Project Settings to disable this warning."), FText::FromString(Node->GetID().ToString()));
									MessageLog.Warning(*MissingSoundWarning.ToString());
								}
							}
						}
					}
				}
			}
		}
		
	}

	Super::FinishCompilingClass(Class);
}

bool FDialogueBlueprintCompilerContext::ValidateGeneratedClass(UBlueprintGeneratedClass* Class)
{
	bool SuperResult = Super::ValidateGeneratedClass(Class);
	bool Result = UDialogueBlueprint::ValidateGeneratedClass(Class);

	return SuperResult && Result;
}


bool FDialogueBlueprintCompilerContext::IsNodePure(const UEdGraphNode* Node) const 
{
	if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
	{
		return K2Node->IsNodePure();
	}
	// Uncomment this line from engine as it causes ensure on load which is annoying 
	//ensure(Node->IsA(UEdGraphNode_Comment::StaticClass()) || Node->IsA(UEdGraphNode_Documentation::StaticClass()));
	return true;
}

void FDialogueBlueprintCompilerContext::FixAbandonedDialogueTemplate(UDialogueBlueprint* DialogueBP)
{
	UDialogue* DialogueTemplate = DialogueBP->DialogueTemplate;

	if (ensure(DialogueTemplate))
	{
		if (DialogueTemplate->GetName() != TEXT("DialogueTemplate"))
		{
			if (UDialogue* AbandonedDialogueTemplate = static_cast<UDialogue*>(FindObjectWithOuter(DialogueBP, UDialogue::StaticClass(), TEXT("DialogueTemplate"))))
			{
				AbandonedDialogueTemplate->ClearFlags(RF_DefaultSubObject);
				AbandonedDialogueTemplate->SetFlags(RF_Transient);
				AbandonedDialogueTemplate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_DoNotDirty);
			}

			DialogueTemplate->Rename(TEXT("DialogueTemplate"), nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_DoNotDirty);
			DialogueTemplate->SetFlags(RF_DefaultSubObject);
		}
	}
}

FDialogueBlueprintCompiler::FDialogueBlueprintCompiler()
{

}

bool FDialogueBlueprintCompiler::CanCompile(const UBlueprint* Blueprint)
{
	return Cast<UDialogueBlueprint>(Blueprint) != nullptr;
}

void FDialogueBlueprintCompiler::PreCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions)
{

}

void FDialogueBlueprintCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
{
	if (UDialogueBlueprint* DialogueBlueprint = CastChecked<UDialogueBlueprint>(Blueprint))
	{
		FDialogueBlueprintCompilerContext Compiler(DialogueBlueprint, Results, CompileOptions);
		Compiler.Compile();
		check(Compiler.NewClass);
	}
}

void FDialogueBlueprintCompiler::PostCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions)
{

}

bool FDialogueBlueprintCompiler::GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const
{
	if (ParentClass == UDialogue::StaticClass() || ParentClass->IsChildOf(UDialogue::StaticClass()))
	{
		OutBlueprintClass = UDialogueBlueprint::StaticClass();
		OutBlueprintGeneratedClass = UDialogueBlueprintGeneratedClass::StaticClass();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE