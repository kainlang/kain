// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGraphOutputs.h"
#include "VoxelPinCategory.h"

struct FVoxelGraphFunctionInfo;
class FVoxelGraphErrorReporter;
class FVoxelComputeNode;

class VOXELGRAPH_API FVoxelCppConstructor
{
public:
	const FVoxelGraphPermutationArray Permutation;
	FVoxelGraphErrorReporter& ErrorReporter;

	FVoxelCppConstructor(const FVoxelGraphPermutationArray& Permutation, FVoxelGraphErrorReporter& ErrorReporter)
		: Permutation(Permutation)
		, ErrorReporter(ErrorReporter)
	{
	}
	
	inline const TArray<FString>& GetLines() const { return Lines; }
	void GetCode(FString& OutCode) const;

public:
	template <typename FmtType, typename... Types>
	inline void AddLinef(const FmtType& Fmt, Types... Args)
	{
		AddLine(FString::Printf(Fmt, Args...));
	}
	inline void AddLine(const FString& Line)
	{
		if (!QueuedComment.IsEmpty())
		{
			AddLineInternal(QueuedComment);
			QueuedComment.Empty();
		}
		AddLineInternal(Line);
	}
	inline void NewLine()
	{
		AddLineInternal("");
	}
	inline void AddPreprocessorLine(const FString& Line)
	{
		const int32 IndentCopy = CurrentIndent;
		CurrentIndent = 0;
		AddLine(Line);
		CurrentIndent = IndentCopy;
	}

	inline void Indent()
	{
		CurrentIndent++;
		check(CurrentIndent >= 0);
	}
	inline void Unindent()
	{
		CurrentIndent--;
		check(CurrentIndent >= 0);
	}
	
	inline void StartBlock()
	{
		AddLine("{");
		Indent();
	}
	inline void EndBlock(bool bSemicolon = false)
	{
		Unindent();
		AddLine(FString("}") + (bSemicolon ? ";" : ""));
	}

	inline void Private()
	{
		Unindent();
		AddLine("private:");
		Indent();
	}
	inline void Public()
	{
		Unindent();
		AddLine("public:");
		Indent();
	}

	inline void EnterNamedScope(const FString& Scope)
	{
		Scopes.Add(Scope);
	}
	inline void ExitNamedScope(const FString& Scope)
	{
		ensureAlways(Scope == Scopes.Pop());
	}
	inline FString GetScopeAccessor() const
	{
		FString Result;
		for (auto& Scope : Scopes)
		{
			Result += Scope + "::";
		}
		return Result;
	}

	// Queue comment and add it before next AddLine
	inline void QueueComment(const FString& Comment)
	{
		QueuedComment = Comment;
	}
	// Add new line if comment has been done
	inline void EndComment()
	{
		if (QueuedComment.IsEmpty())
		{
			NewLine();
		}
		else
		{
			QueuedComment.Empty();
		}
	}

public:
	void AddOtherConstructor(const FVoxelCppConstructor& Other);

	void AddFunctionCall(const FVoxelGraphFunctionInfo& Info, const TArray<FString>& Args);
	void AddFunctionDeclaration(const FVoxelGraphFunctionInfo& Info, const TArray<FString>& Args);

	FString GetTypeString(EVoxelPinCategory Category) const;
	FString GetTypeString(EVoxelDataPinCategory Category) const;

	FString GetContextTypeString() const;

public:
	void AddVariable(int32 Id, const FString& Value);	
	bool HasVariable(int32 Id);
	bool CurrentScopeHasVariable(int32 Id);

	FString GetVariable(int32 Id, const FVoxelComputeNode* Node);

	void StartScope();
	void EndScope();

	bool IsNodeInit(FVoxelComputeNode* Node) const;
	void SetNodeAsInit(FVoxelComputeNode* Node);

private:
	struct FVoxelVariableScope
	{
		TMap<int32, FString> Variables;
		TSet<FVoxelComputeNode*> NodesAlreadyInit;

		FVoxelVariableScope() = default;
		
		FVoxelVariableScope* GetChild()
		{
			check(!Child);
			Child = MakeUnique<FVoxelVariableScope>();
			Child->Parent = this;
			return Child.Get();
		}

		const FString* GetVariable(int32 Id) const
		{
			if (auto* Result = Variables.Find(Id))
			{
				return Result;
			}
			else
			{
				return Parent ? Parent->GetVariable(Id) : nullptr;
			}
		}

		bool IsNodeInit(FVoxelComputeNode* Node) const
		{
			return NodesAlreadyInit.Contains(Node) || (Parent && Parent->IsNodeInit(Node));
		}

		FVoxelVariableScope* GetParent() const { return Parent; }
		void RemoveChild() { Child = nullptr; }

	private:
		FVoxelVariableScope* Parent = nullptr;
		TUniquePtr<FVoxelVariableScope> Child;
	};

	inline void AddLineInternal(const FString& Line)
	{		
		FString FinalLine;
		check(CurrentIndent >= 0);
		for (int32 I = 0; I < CurrentIndent; I++)
		{
			FinalLine += "\t";
		}
		FinalLine.Append(Line);
		Lines.Add(FinalLine);
	}
	
private:
	TArray<FString> Lines;
	int32 CurrentIndent = 0;

	FVoxelVariableScope MainScope;
	FVoxelVariableScope* CurrentScope = &MainScope;

	FString QueuedComment;
	TArray<FString> Scopes;
};

struct FVoxelCppVariableScope
{
	FVoxelCppVariableScope(FVoxelCppConstructor& Constructor)
		: Constructor(Constructor)
	{
		Constructor.StartScope();
	}

	~FVoxelCppVariableScope()
	{
		Constructor.EndScope();
	}

private:
	FVoxelCppConstructor& Constructor;
};
