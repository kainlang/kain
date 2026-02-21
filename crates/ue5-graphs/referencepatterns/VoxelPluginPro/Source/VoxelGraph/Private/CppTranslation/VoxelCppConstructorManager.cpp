// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CppTranslation/VoxelCppConstructorManager.h"
#include "VoxelGraphOutputs.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelGraphConstants.h"

#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConfig.h"

#include "Runtime/VoxelGraph.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelCompiledGraphs.h"

struct FVoxelCppStructConfig
{
	const FVoxelGraphPermutationArray Permutation;
	const FString Name;
	const FString StructName;
	const TArray<FVoxelGraphOutput> Outputs;

	FVoxelCppStructConfig(const FVoxelGraphPermutationArray& Permutation, const FString& InName, const TArray<FVoxelGraphOutput>& Outputs)
		: Permutation(Permutation)
		, Name(InName)
		, StructName("FLocalComputeStruct_" + Name)
		, Outputs(Outputs)
	{
	}

	inline bool IsSingleOutput(EVoxelDataPinCategory Category) const
	{
		return Outputs.Num() == 1 && Outputs[0].Category == Category;
	}
	inline bool IsSingleOutputRange(EVoxelDataPinCategory Category) const
	{
		return
			Outputs.Num() == 2 &&
			Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex) &&
			GetRangeGraphOutput().Category == Category;
	}
	// With range analysis there's a dummy output, ignore it
	inline const FVoxelGraphOutput& GetRangeGraphOutput() const
	{
		check(Outputs.Num() == 2 && Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex));
		if (Outputs[0].Index == FVoxelGraphOutputsIndices::RangeAnalysisIndex)
		{
			return Outputs[1];
		}
		else
		{
			check(Outputs[1].Index == FVoxelGraphOutputsIndices::RangeAnalysisIndex);
			return Outputs[0];
		}
	}
};

inline void AddCppStruct(FVoxelCppConstructor& Cpp, FVoxelCppConstructor& GlobalScopeCpp, const FVoxelCppConfig& Config, const FVoxelGraph& Graph, const FVoxelCppStructConfig& StructConfig)
{
	const bool bIsRangeAnalysis = StructConfig.Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex);
	
	bool bHasMaterial = false;
	TArray<FVoxelGraphOutput> Outputs;
	TArray<FString> GraphOutputs;
	for (auto& Output : StructConfig.Outputs)
	{
		if (Output.Index == FVoxelGraphOutputsIndices::RangeAnalysisIndex)
		{
			continue;
		}

		if (uint32(GraphOutputs.Num()) <= Output.Index)
		{
			GraphOutputs.SetNum(Output.Index + 1);
		}
		
		if (Output.Index == FVoxelGraphOutputsIndices::MaterialIndex)
		{
			bHasMaterial = true;
			// Do not add to Outputs, but do add an entry in GraphOutputs for SetMaterial to work properly
			GraphOutputs[Output.Index] = FVoxelCppIds::GraphOutputs + ".MaterialBuilder";
		}
		else
		{
			Outputs.Add(Output);
			GraphOutputs[Output.Index] = FVoxelCppIds::GraphOutputs + "." + Output.Name.ToString();
		}
	}

	Cpp.AddLine("class " + StructConfig.StructName);
	Cpp.EnterNamedScope(StructConfig.StructName);
	Cpp.StartBlock();
	Cpp.Public();
	{
		FVoxelCppVariableScope Scope(Cpp);

		// GraphOutputs struct
		Cpp.AddLine("struct " + FVoxelCppIds::GraphOutputsType);
		Cpp.EnterNamedScope(FVoxelCppIds::GraphOutputsType);
		Cpp.StartBlock();
		{
			Cpp.AddLine(FVoxelCppIds::GraphOutputsType + "() {}");
			Cpp.NewLine();
			Cpp.AddLine("void Init(const FVoxelGraphOutputsInit& Init)");
			Cpp.StartBlock();
			if (bHasMaterial)
			{
				Cpp.AddLine("MaterialBuilder.SetMaterialConfig(Init.MaterialConfig);");
			}
			Cpp.EndBlock();

			const FString TypeT = bIsRangeAnalysis ? "TVoxelRange<T>" : "T";
			
			Cpp.NewLine();
			Cpp.AddLine("template<typename T, uint32 Index>");
			Cpp.AddLine(TypeT + " Get() const;");
			Cpp.AddLine("template<typename T, uint32 Index>");
			Cpp.AddLine("void Set(" + TypeT + " Value);");

			for (auto& Output : Outputs)
			{
				const auto Type = FVoxelPinCategory::GetTypeString(Output.Category);
				const auto RealType = bIsRangeAnalysis ? "TVoxelRange<" + Type + ">" : Type;
				
				GlobalScopeCpp.AddLine("template<>");
				GlobalScopeCpp.AddLine("inline " + RealType + " " + Cpp.GetScopeAccessor() + "Get<" + Type + ", " + FString::FromInt(Output.Index) + ">() const");
				GlobalScopeCpp.StartBlock();
				GlobalScopeCpp.AddLine("return " + Output.Name.ToString() + ";");
				GlobalScopeCpp.EndBlock();
				GlobalScopeCpp.AddLine("template<>");
				GlobalScopeCpp.AddLine("inline void " + Cpp.GetScopeAccessor() + "Set<" + Type + ", " + FString::FromInt(Output.Index) + ">(" + RealType + " InValue)");
				GlobalScopeCpp.StartBlock();
				GlobalScopeCpp.AddLine(Output.Name.ToString() + " = InValue;");
				GlobalScopeCpp.EndBlock();
			}

			if (bHasMaterial)
			{
				GlobalScopeCpp.AddLine("template<>");
				GlobalScopeCpp.AddLine("inline FVoxelMaterial " + Cpp.GetScopeAccessor() + "Get<FVoxelMaterial, " + FString::FromInt(FVoxelGraphOutputsIndices::MaterialIndex) + ">() const");
				GlobalScopeCpp.StartBlock();
				GlobalScopeCpp.AddLine("return MaterialBuilder.Build();");
				GlobalScopeCpp.EndBlock();
				GlobalScopeCpp.AddLine("template<>");
				GlobalScopeCpp.AddLine("inline void " + Cpp.GetScopeAccessor() + "Set<FVoxelMaterial, " + FString::FromInt(FVoxelGraphOutputsIndices::MaterialIndex) + ">(FVoxelMaterial Material)");
				GlobalScopeCpp.StartBlock();
				GlobalScopeCpp.EndBlock();
			}

			Cpp.NewLine();

			for (auto& Output : Outputs)
			{
				Cpp.AddLine(Output.GetDeclaration(Cpp) + ";");
			}
			if (bHasMaterial)
			{
				Cpp.AddLine("FVoxelMaterialBuilder MaterialBuilder;");
			}
		}
		Cpp.EndBlock(true);
		Cpp.ExitNamedScope(FVoxelCppIds::GraphOutputsType);

		// Cache structs
		for (auto Dependency : {
			EVoxelAxisDependencies::Constant,
			EVoxelAxisDependencies::X,
			EVoxelAxisDependencies::XY })
		{
			Cpp.AddLine("struct " + FVoxelCppIds::GetCacheType(Dependency));
			Cpp.StartBlock();
			{
				Cpp.AddLine(FVoxelCppIds::GetCacheType(Dependency) + "() {}");
				Cpp.NewLine();

				if (Dependency == EVoxelAxisDependencies::Constant)
				{
					TSet<FVoxelComputeNode*> ConstantNodes;
					Graph.GetConstantNodes(ConstantNodes);
					for (auto* Node : ConstantNodes)
					{
						check(Node->Type == EVoxelComputeNodeType::Data);
						Node->DeclareOutputs(Cpp, FVoxelVariableAccessInfo::StructDeclaration(Dependency));
					}
				}
				else
				{
					TSet<FVoxelComputeNode*> Nodes;
					Graph.GetNotConstantNodes(Nodes);
					for (auto* Node : Nodes)
					{
						// We don't want to cache functions or seed outputs
						if (Node->Type == EVoxelComputeNodeType::Data)
						{
							Node->DeclareOutputs(Cpp, FVoxelVariableAccessInfo::StructDeclaration(Dependency));
						}
						else
						{
							check(Node->Type == EVoxelComputeNodeType::Exec || Node->Type == EVoxelComputeNodeType::Seed);
						}
					}
				}
			}
			Cpp.EndBlock(true);
			Cpp.NewLine();
		}

		// Constructor
		Cpp.AddLine(StructConfig.StructName + "(const " + FVoxelCppIds::ExposedVariablesStructType + "& In" + FVoxelCppIds::ExposedVariablesStruct + ")");
		Cpp.Indent();
		Cpp.AddLine(": " + FVoxelCppIds::ExposedVariablesStruct + "(In" + FVoxelCppIds::ExposedVariablesStruct + ")");
		Cpp.Unindent();
		Cpp.AddLine("{");
		Cpp.AddLine("}");
		Cpp.NewLine();

		// Init
		Cpp.AddLine("void Init(const FVoxelGeneratorInit& " + FVoxelCppIds::InitStruct + ")");
		Cpp.StartBlock();
		{
			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.AddLine("//////////////////// Init nodes ////////////////////");
			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.StartBlock();
			Graph.Init(Cpp);
			Cpp.EndBlock();
			Cpp.NewLine();

			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.AddLine("//////////////// Compute constants /////////////////");
			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.StartBlock();
			Graph.ComputeConstants(Cpp);
			Cpp.EndBlock();
		}
		Cpp.EndBlock();

		// Computes
		for (EVoxelFunctionAxisDependencies Dependencies : FVoxelAxisDependencies::GetAllFunctionDependencies())
		{
			if (bIsRangeAnalysis && Dependencies != EVoxelFunctionAxisDependencies::XYZWithoutCache)
			{
				continue;
			}

			FString Line = "void Compute" + FVoxelAxisDependencies::ToString(Dependencies) + "(";
			Line += "const " + Cpp.GetContextTypeString() + "& " + FVoxelCppIds::Context;

			if (Dependencies == EVoxelFunctionAxisDependencies::X ||
				Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache || // Still need to compute X variables
				Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
				Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
			{
				Line += ", ";
				if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
				{
					Line += "const ";
				}
				Line += FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::X) + "& " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::X);
			}

			if (Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache ||
				Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
				Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
			{
				Line += ", ";
				if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
				{
					Line += "const ";
				}
				Line += FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::XY) + "& " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::XY);
			}

			if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache || Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache)
			{
				Line += ", " + FVoxelCppIds::GraphOutputsType + "& " + FVoxelCppIds::GraphOutputs;
			}
			Line += ") const";
			Cpp.AddLine(Line);
			Cpp.StartBlock();
			Graph.Compute(Cpp, Dependencies);
			Cpp.EndBlock();
		}

		// Getters
		Cpp.NewLine();
		Cpp.AddLinef(TEXT("inline %s GetBufferX() const { return {}; }"), *FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::X));
		Cpp.AddLinef(TEXT("inline %s GetBufferXY() const { return {}; }"), *FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::XY));
		Cpp.AddLinef(TEXT("inline %s GetOutputs() const { return {}; }"), *FVoxelCppIds::GraphOutputsType);
		Cpp.NewLine();

		Cpp.Private();

		// Constant cache
		Cpp.AddLine(FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::Constant) + " " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::Constant) + ";");

		Cpp.NewLine();
		
		// Exposed variables ref
		Cpp.AddLine("const " + FVoxelCppIds::ExposedVariablesStructType + "& " + FVoxelCppIds::ExposedVariablesStruct + ";");
		
		Cpp.NewLine();
		
		// Private node variables
		{
			TSet<FVoxelComputeNode*> Nodes;
			Graph.GetAllNodes(Nodes);
			TArray<FVoxelVariable> PrivateVariables;
			for (auto* Node : Nodes)
			{
				Node->GetPrivateVariables(PrivateVariables);
			}
			for (auto& Variable : PrivateVariables)
			{
				Cpp.AddLine(Variable.GetDeclaration() + ";");
			}
		}
		
		// Functions
		Cpp.NewLine();
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.AddLine("//////////////////////////// Init functions ///////////////////////////");
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.NewLine();
		Graph.DeclareInitFunctions(Cpp);
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.AddLine("////////////////////////// Compute functions //////////////////////////");
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.NewLine();
		Graph.DeclareComputeFunctions(Cpp, GraphOutputs);
	}
	Cpp.EndBlock(true);
	Cpp.ExitNamedScope(StructConfig.StructName);
}

FVoxelCppConstructorManager::FVoxelCppConstructorManager(const FString& ClassName, UVoxelGraphGenerator* VoxelGraphGenerator)
	: ClassName(ClassName)
	, VoxelGraphGenerator(VoxelGraphGenerator)
	, Graphs(MakeUnique<FVoxelCompiledGraphs>())
	, ErrorReporter(MakeUnique<FVoxelGraphErrorReporter>(VoxelGraphGenerator))
{
	check(VoxelGraphGenerator);

	if (!VoxelGraphGenerator->CreateGraphs(*Graphs, false, false, false))
	{
		ErrorReporter->AddError("Compilation error!");
	}
}

FVoxelCppConstructorManager::~FVoxelCppConstructorManager()
{
	// UniquePtr forward decl
}

bool FVoxelCppConstructorManager::Compile(FString& OutHeader, FString& OutCpp)
{
	const bool bResult = CompileInternal(OutHeader, OutCpp);
	ErrorReporter->Apply(true);
	return bResult;
}

#define CHECK_FOR_ERRORS() if (ErrorReporter->HasError()) { return false; }

bool FVoxelCppConstructorManager::CompileInternal(FString& OutHeader, FString& OutCpp)
{
	CHECK_FOR_ERRORS();

	TArray<FVoxelCppStructConfig> AllStructConfigs;
	TMap<FVoxelGraphPermutationArray, FVoxelCppStructConfig> PermutationToStructConfigs;
	TSet<FVoxelComputeNode*> Nodes;
	{
		auto Outputs = VoxelGraphGenerator->GetOutputs();
		// Check outputs names
		{
			TSet<FName> Names;
			for (auto& It : Outputs)
			{
				auto& Name = It.Value.Name;
				if (Name.ToString().IsEmpty())
				{
					ErrorReporter->AddError("Empty Output name!");
				}
				CHECK_FOR_ERRORS();
				if (Names.Contains(Name))
				{
					ErrorReporter->AddError("Multiple Outputs have the same name! (" + Name.ToString() + ")");
				}
				CHECK_FOR_ERRORS();
				Names.Add(Name);
			}
		}
		for (const FVoxelGraphPermutationArray& Permutation : VoxelGraphGenerator->GetPermutations())
		{
			if (Permutation.Num() == 0)
			{
				continue;
			}

			const FString Name = "Local" + FVoxelGraphOutputsUtils::GetPermutationName(Permutation, Outputs);
			TArray<FVoxelGraphOutput> PermutationOutputs;
			for (uint32 Index : Permutation)
			{
				PermutationOutputs.Add(Outputs[Index]);
			}
			
			const auto& Graph = Graphs->Get(Permutation);
			Graph->GetAllNodes(Nodes);

			FVoxelCppStructConfig StructConfig(Permutation, Name, PermutationOutputs);
			AllStructConfigs.Add(StructConfig);
			PermutationToStructConfigs.Add(Permutation, StructConfig);
		}
	}

	PermutationToStructConfigs.KeySort([](const FVoxelGraphPermutationArray& A, const FVoxelGraphPermutationArray& B)
	{
		if (A.Num() < B.Num())
		{
			return true;
		}
		if (A.Num() > B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); Index++)
		{
			if (A[Index] > B[Index])
			{
				return false;
			}
		}
		return true;
	});
		
	FVoxelCppConfig Config(*ErrorReporter);
	Config.AddInclude("CoreMinimal.h");
	Config.AddInclude("VoxelGeneratedWorldGeneratorsIncludes.h");
	for (auto* Node : Nodes)
	{
		Node->CallSetupCpp(Config);
	}
	CHECK_FOR_ERRORS();
	Config.AddInclude(ClassName + ".generated.h");
	Config.BuildExposedVariablesArray();

	const FString InstanceClassName("F" + ClassName + "Instance");
	const FString MainClassName("U" + ClassName);

	FVoxelCppConstructor Header({}, *ErrorReporter);
	FVoxelCppConstructor Cpp({}, *ErrorReporter);

	// Header Intro
	Header.AddLine("// Copyright Voxel Plugin SAS. All Rights Reserved.");
	Header.NewLine();
	Header.AddLine("#pragma once");
	Header.NewLine();

	// Includes
	for (auto& Include : Config.GetIncludes())
	{
		Header.AddLine(Include.ToString());
	}
	Header.NewLine();

	// Cpp Intro
	Cpp.AddLine("// Copyright Voxel Plugin SAS. All Rights Reserved.");
	Cpp.NewLine();
	Cpp.AddLine(FVoxelCppInclude(ClassName + ".h").ToString());
	Cpp.NewLine();
	Cpp.AddLine("PRAGMA_GENERATED_VOXEL_GRAPH_START");
	Cpp.NewLine();
	Cpp.AddLine("using FVoxelGraphSeed = int32;");
	Cpp.NewLine();
	
	Cpp.AddLine("#if VOXEL_GRAPH_GENERATED_VERSION == " + FString::FromInt(VOXEL_GRAPH_GENERATED_VERSION));
	
	// Instance
	{
		// For the outputs templates specialization
		FVoxelCppConstructor GlobalScopeCpp({}, * ErrorReporter);

		Cpp.AddLine("class " + InstanceClassName + " : public TVoxelGraphGeneratorInstanceHelper<" + InstanceClassName + ", " + MainClassName + ">");
		Cpp.StartBlock();
		Cpp.Public();
		{
			// Define FParams
			{
				Cpp.AddLine("struct " + FVoxelCppIds::ExposedVariablesStructType);
				Cpp.StartBlock();
				for (auto& Variable : Config.GetExposedVariables())
				{
					Cpp.AddLine(Variable->GetConstDeclaration() + ";");
				}
				Cpp.EndBlock(true);
			}
			Cpp.NewLine();
			
			// Define structs
			for (auto& StructConfig : AllStructConfigs)
			{
				const auto& Graph = *Graphs->Get(StructConfig.Permutation);

				FVoxelCppConstructor LocalCpp(StructConfig.Permutation, *ErrorReporter);
				LocalCpp.EnterNamedScope(InstanceClassName);
				AddCppStruct(LocalCpp, GlobalScopeCpp, Config, Graph, StructConfig);
				LocalCpp.ExitNamedScope(InstanceClassName);
				CHECK_FOR_ERRORS();

				Cpp.AddOtherConstructor(LocalCpp);
			}
			Cpp.NewLine();

			// Constructor
			{
				const auto GetMaps = [&](bool bEndComma, EVoxelDataPinCategory Category, auto Lambda)
				{
					Cpp.AddLine("{");
					Cpp.Indent();
					for (auto& FlagConfig : AllStructConfigs)
					{
						if (FlagConfig.Outputs.Num() == 1)
						{
							auto& Output = FlagConfig.Outputs[0];
							if (Output.Category == Category)
							{
								Cpp.AddLine("{ \"" + Output.Name.ToString() + "\", " + Lambda(Output.Index) + " },");
							}
						}
					}
					Cpp.Unindent();
					Cpp.AddLine(FString("}") + (bEndComma ? "," : ""));
				};
				
				Cpp.AddLine(InstanceClassName + "(" + MainClassName + "& Object)");
				Cpp.Indent();

				{
					Cpp.Indent();
					Cpp.AddLine(": TVoxelGraphGeneratorInstanceHelper(");
					///////////////////////////////////////////////////////////////////////////////
					GetMaps(true, EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::FromInt(Index); });
					GetMaps(true, EVoxelDataPinCategory::Int, [](uint32 Index) { return FString::FromInt(Index); });
					GetMaps(true, EVoxelDataPinCategory::Color, [](uint32 Index) { return FString::FromInt(Index); });
					///////////////////////////////////////////////////////////////////////////////
					Cpp.AddLine("{");
					Cpp.Indent();
					GetMaps(true, EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("NoTransformAccessor<v_flt>::Get<%u, TOutputFunctionPtr<v_flt>>()"), Index); });
					GetMaps(true, EVoxelDataPinCategory::Int, [](uint32 Index) { return FString::Printf(TEXT("NoTransformAccessor<int32>::Get<%u, TOutputFunctionPtr<int32>>()"), Index); });
					GetMaps(true, EVoxelDataPinCategory::Color, [](uint32 Index) { return FString::Printf(TEXT("NoTransformAccessor<FColor>::Get<%u, TOutputFunctionPtr<FColor>>()"), Index); });
					GetMaps(false, EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("NoTransformRangeAccessor<v_flt>::Get<%u, TRangeOutputFunctionPtr<v_flt>>()"), Index); });
					Cpp.Unindent();
					Cpp.AddLine("},");
					///////////////////////////////////////////////////////////////////////////////
					Cpp.AddLine("{");
					Cpp.Indent();
					GetMaps(true, EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("WithTransformAccessor<v_flt>::Get<%u, TOutputFunctionPtr_Transform<v_flt>>()"), Index); });
					GetMaps(true, EVoxelDataPinCategory::Int, [](uint32 Index) { return FString::Printf(TEXT("WithTransformAccessor<int32>::Get<%u, TOutputFunctionPtr_Transform<int32>>()"), Index); });
					GetMaps(true, EVoxelDataPinCategory::Color, [](uint32 Index) { return FString::Printf(TEXT("WithTransformAccessor<FColor>::Get<%u, TOutputFunctionPtr_Transform<FColor>>()"), Index); });
					GetMaps(false, EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("WithTransformRangeAccessor<v_flt>::Get<%u, TRangeOutputFunctionPtr_Transform<v_flt>>()"), Index); });
					Cpp.Unindent();
					Cpp.AddLine("},");
					///////////////////////////////////////////////////////////////////////////////
					Cpp.AddLine("Object)");
					Cpp.Unindent();
				}

				// Init params
				auto& Variables = Config.GetExposedVariables();
				if (Variables.Num() > 0)
				{
					Cpp.AddLine(", Params(FParams");
					Cpp.AddLine("{");
					Cpp.Indent();
					for (int32 Index = 0; Index < Variables.Num(); Index++)
					{
						auto& Variable = *Variables[Index];
						Cpp.AddLine(Variable.GetLocalVariableFromExposedOne("Object." + Variable.ExposedName) + (Index == Variables.Num() - 1 ? "" : ","));
					}
					Cpp.Unindent();
					Cpp.AddLine("})");
				}

				// Pass params to structs
				for (auto& FlagConfig : AllStructConfigs)
				{
					Cpp.AddLine(", " + FlagConfig.Name + "(" + FVoxelCppIds::ExposedVariablesStruct + ")");
				}

				Cpp.Unindent();

				Cpp.StartBlock();
				Cpp.EndBlock();
			}
			Cpp.NewLine();

			// Init
			Cpp.AddLine("virtual void InitGraph(const FVoxelGeneratorInit& " + FVoxelCppIds::InitStruct + ") override final");
			Cpp.StartBlock();
			{
				for (auto& FlagConfig : AllStructConfigs)
				{
					Cpp.AddLine(FlagConfig.Name + ".Init(" + FVoxelCppIds::InitStruct + ");");
				}
			}
			Cpp.EndBlock();

			const auto PermutationToString = [](FVoxelGraphPermutationArray Permutation)
			{
				Permutation.Sort();
				FString String;
				for (uint32 Index : Permutation)
				{
					if (!String.IsEmpty())
					{
						String += ", ";
					}
					String += FString::FromInt(Index);
				}
				return String;
			};

			for (auto& PermutationIt : PermutationToStructConfigs)
			{
				const FVoxelGraphPermutationArray& Permutation = PermutationIt.Key;
				const FString PermutationString = PermutationToString(Permutation);
				const FString ScopeAccessor = InstanceClassName;
				
				GlobalScopeCpp.AddLine("template<>");
				if (Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
				{
					GlobalScopeCpp.AddLinef(TEXT("inline auto& %s::GetRangeTarget<%s>() const"), *ScopeAccessor, *PermutationString);
				}
				else
				{
					GlobalScopeCpp.AddLinef(TEXT("inline auto& %s::GetTarget<%s>() const"), *ScopeAccessor, *PermutationString);
				}
				GlobalScopeCpp.StartBlock();
				GlobalScopeCpp.AddLinef(TEXT("return %s;"), *PermutationIt.Value.Name);
				GlobalScopeCpp.EndBlock();
			}
			
			Cpp.NewLine();
			Cpp.AddLine("template<uint32... Permutation>");
			Cpp.AddLine("auto& GetTarget() const;");
			Cpp.NewLine();
			Cpp.AddLine("template<uint32... Permutation>");
			Cpp.AddLine("auto& GetRangeTarget() const;");
			Cpp.NewLine();
			Cpp.Private();

			Cpp.AddLine(FVoxelCppIds::ExposedVariablesStructType + " " + FVoxelCppIds::ExposedVariablesStruct + ";");
			for (auto& StructConfig : AllStructConfigs)
			{
				Cpp.AddLine(StructConfig.StructName + " " + StructConfig.Name + ";");
			}
			Cpp.NewLine();
		}
		Cpp.EndBlock(true);

		// Add the specializations
		Cpp.NewLine();
		Cpp.AddOtherConstructor(GlobalScopeCpp);
	}
	
	Cpp.AddLine("#endif");
	
	Cpp.NewLine();
	Cpp.AddLine("////////////////////////////////////////////////////////////");
	Cpp.AddLine("////////////////////////// UCLASS //////////////////////////");
	Cpp.AddLine("////////////////////////////////////////////////////////////");
	Cpp.NewLine();

	// UClass
	{
		Header.AddLine("UCLASS(Blueprintable)");
		Header.AddLine("class " + MainClassName + " : public UVoxelGraphGeneratorHelper");
		Header.StartBlock();
		{
			Header.AddLine("GENERATED_BODY()");
			Header.NewLine();
			Header.Public();

			for (auto& Variable : Config.GetExposedVariables())
			{
				Header.AddLinef(TEXT("// %s"), *Variable->Tooltip);
				FString MetadataString = Variable->GetMetadataString();
				if (!MetadataString.IsEmpty())
				{
					MetadataString = ", meta=(" + MetadataString + ")";
				}
				Header.AddLinef(TEXT("UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=\"%s\"%s)"), *Variable->Category, *MetadataString);
				Header.AddLine(Variable->ExposedType + " " + Variable->ExposedName + (!Variable->DefaultValue.IsEmpty() ? " = " + Variable->DefaultValue + ";" : ";"));
			}
			
			Header.NewLine();
			Header.AddLine(MainClassName + "();");

			Cpp.AddLine(MainClassName + "::" + MainClassName + "()");
			Cpp.StartBlock();
			Cpp.AddLine("bEnableRangeAnalysis = " + LexToString(VoxelGraphGenerator->bEnableRangeAnalysis) + ";");
			Cpp.EndBlock();
			
			// GetGenerator
			Header.AddLine("virtual TVoxelSharedRef<FVoxelTransformableGeneratorInstance> GetTransformableInstance() override;");
			Cpp.NewLine();
			Cpp.AddLine("TVoxelSharedRef<FVoxelTransformableGeneratorInstance> " + MainClassName + "::GetTransformableInstance()");
			Cpp.StartBlock();
			Cpp.AddPreprocessorLine("#if VOXEL_GRAPH_GENERATED_VERSION == " + FString::FromInt(VOXEL_GRAPH_GENERATED_VERSION));
			Cpp.AddLine("return MakeVoxelShared<" + InstanceClassName + ">(*this);");
			Cpp.AddPreprocessorLine("#else");
			Cpp.AddPreprocessorLine("#if VOXEL_GRAPH_GENERATED_VERSION > " + FString::FromInt(VOXEL_GRAPH_GENERATED_VERSION));
			{
				const FString Error = "\"Outdated generated voxel graph: " + ClassName + ". You need to regenerate it.\"";
				Cpp.AddLine("EMIT_CUSTOM_WARNING(" + Error + ");");
				Cpp.AddLine("FVoxelMessages::Warning(" + Error + ");");
			}
			Cpp.AddPreprocessorLine("#else");
			{
				const FString Error = "\"Generated voxel graph is more recent than the Voxel Plugin version: " + ClassName + ". You need to update the plugin.\"";
				Cpp.AddLine("EMIT_CUSTOM_WARNING(" + Error + ");");
				Cpp.AddLine("FVoxelMessages::Warning(" + Error + ");");
			}
			Cpp.AddPreprocessorLine("#endif");
			Cpp.AddLine("return MakeVoxelShared<FVoxelTransformableEmptyGeneratorInstance>();");
			Cpp.AddPreprocessorLine("#endif");
			Cpp.EndBlock();
		}
		Header.EndBlock(true);
	}

	Cpp.NewLine();
	Cpp.AddLine("PRAGMA_GENERATED_VOXEL_GRAPH_END");

	Header.GetCode(OutHeader);
	Cpp.GetCode(OutCpp);

	return !ErrorReporter->HasError();
}
