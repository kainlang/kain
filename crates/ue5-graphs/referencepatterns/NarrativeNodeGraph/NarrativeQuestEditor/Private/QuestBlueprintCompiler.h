// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "KismetCompiler.h"
#include "QuestBlueprint.h"

#include "KismetCompilerModule.h"

//////////////////////////////////////////////////////////////////////////
// FQuestBlueprintCompiler 
class NARRATIVEQUESTEDITOR_API FQuestBlueprintCompiler : public IBlueprintCompiler
{

public:

	FQuestBlueprintCompiler();

	bool CanCompile(const UBlueprint* Blueprint) override;
	void PreCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions) override;
	void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) override;
	void PostCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions) override;
	virtual bool GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const override;

};


/**
 * Quest compiler context, i'm implementing this because i think this is neccessary for copying objects from the 
 * old blueprint generated class to the new regenerated class after a compile happens  
 */
class NARRATIVEQUESTEDITOR_API FQuestBlueprintCompilerContext : public FKismetCompilerContext
{
public:

FQuestBlueprintCompilerContext(UQuestBlueprint* SourceQuestBP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions);
virtual ~FQuestBlueprintCompilerContext();

protected:

	UQuestBlueprint* GetQuestBlueprint() const { return Cast<UQuestBlueprint>(Blueprint); }

	typedef FKismetCompilerContext Super;

	// FKismetCompilerContext
	//virtual void CreateFunctionList() override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) override;
	virtual void PrecompileFunction(FKismetFunctionContext& Context, EInternalCompilerFlags InternalFlags) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOutOldCDO) override;
	virtual void SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean) override;
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass) override;
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject);
	virtual void FinishCompilingClass(UClass* Class) override;
	virtual bool ValidateGeneratedClass(UBlueprintGeneratedClass* Class) override;
	virtual bool IsNodePure(const UEdGraphNode* Node) const;
	// End FKismetCompilerContext

protected:

	void FixAbandonedQuestTemplate(UQuestBlueprint* QuestBP);

	class UQuestBlueprintGeneratedClass* NewQuestBlueprintClass;

	class UQuestGraphSchema* QuestSchema;

};
