// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNodes/VoxelNoiseNodes.h"
#include "VoxelNodes/VoxelNoiseNodeHelper.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelGraphGenerator.h"
#include "VoxelUtilities/VoxelMathUtilities.h"

#define NOISE_SAMPLE_RANGE 10

FName UVoxelNode_NoiseNode::GetInputPinName(int32 PinIndex) const
{
	if (GetDimension() == 2)
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "Frequency";
		}
		else if (PinIndex == 3)
		{
			return "Seed";
		}
	}
	else
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "Z";
		}
		else if (PinIndex == 3)
		{
			return "Frequency";
		}
		else if (PinIndex == 4)
		{
			return "Seed";
		}
	}
	
	return CustomNoisePins.GetInputPin(PinIndex - GetBaseInputPinsCount(), false).Name;
}

FName UVoxelNode_NoiseNode::GetOutputPinName(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return IsDerivative() ? "Value" : "";
	}

	if (IsDerivative())
	{
		if (PinIndex == 1)
		{
			return "DX";
		}
		else if (PinIndex == 2)
		{
			return "DY";
		}
		else if (PinIndex == 3)
		{
			return "DZ";
		}
	}

	return CustomNoisePins.GetOutputPin(PinIndex - GetBaseOutputPinsCount(), false).Name;
}

FString UVoxelNode_NoiseNode::GetInputPinToolTip(int32 PinIndex) const
{
	if (GetDimension() == 2)
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "The frequency of the noise";
		}
		else if (PinIndex == 3)
		{
			return "The seed to use";
		}
	}
	else
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "Z";
		}
		else if (PinIndex == 3)
		{
			return "The frequency of the noise";
		}
		else if (PinIndex == 4)
		{
			return "The seed to use";
		}
	}
	
	return CustomNoisePins.GetInputPin(PinIndex - GetBaseInputPinsCount(), false).ToolTip;
}

FString UVoxelNode_NoiseNode::GetOutputPinToolTip(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return "The noise value";
	}
	else if (PinIndex == 1)
	{
		return "The derivative along the X axis. Can be used to compute the slope of the noise using GetSlopeFromDerivatives.";
	}
	else if (PinIndex == 2)
	{
		return "The derivative along the Y axis. Can be used to compute the slope of the noise using GetSlopeFromDerivatives.";
	}
	else if (PinIndex == 3)
	{
		return "The derivative along the Z axis. Can be used to compute the slope of the noise using GetSlopeFromDerivatives.";
	}

	return CustomNoisePins.GetOutputPin(PinIndex - GetBaseOutputPinsCount(), false).ToolTip;
}

EVoxelPinCategory UVoxelNode_NoiseNode::GetInputPinCategory(int32 PinIndex) const
{
	return PinIndex == GetDimension() + 1 ? EVoxelPinCategory::Seed : EVoxelPinCategory::Float;
}

EVoxelPinCategory UVoxelNode_NoiseNode::GetOutputPinCategory(int32 PinIndex) const
{
	return EVoxelPinCategory::Float;
}

FString UVoxelNode_NoiseNode::GetInputPinDefaultValue(int32 PinIndex) const
{
	if (PinIndex == GetDimension())
	{
		return FString::SanitizeFloat(Frequency);
	}
	
	return CustomNoisePins.GetInputPin(PinIndex - GetBaseInputPinsCount(), false).DefaultValue;
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_NoiseNode::GetCompilationNode() const
{
	if (NeedRangeAnalysis())
	{
		// We handle range analysis ourselves, without using the inputs
		return MakeShared<FVoxelRangeAnalysisConstantCompilationNode>(*this);
	}

	// eg for gradient perturb
	return Super::GetCompilationNode();
}

///////////////////////////////////////////////////////////////////////////////

void UVoxelNode_NoiseNode::ComputeOutputRanges()
{
	FVoxelNoiseComputeNode::ComputeOutputRanges(*this);
}

int32 UVoxelNode_NoiseNode::SetupInputsForComputeOutputRanges(TVoxelStaticArray<FVoxelNodeType, MAX_VOXELNODE_PINS>& Inputs) const
{
	// Frequency
	Inputs[GetDimension()].Get<v_flt>() = 1;

	return GetBaseInputPinsCount();
}

///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UVoxelNode_NoiseNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty &&
		PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		ComputeOutputRanges();
	}
}

bool UVoxelNode_NoiseNode::CanEditChange(const FProperty* InProperty) const
{
	return
		Super::CanEditChange(InProperty) && 
		(NeedRangeAnalysis() || InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_NoiseNode, NumberOfSamples));
}
#endif

void UVoxelNode_NoiseNode::PostLoad()
{
	Super::PostLoad();
	if (OutputRanges.Num() == 0)
	{
		ComputeOutputRanges();
	}
}

void UVoxelNode_NoiseNode::PostInitProperties()
{
	Super::PostInitProperties();
	if (OutputRanges.Num() == 0 && !HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		ComputeOutputRanges();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelNoiseComputeNode::FVoxelNoiseComputeNode(const UVoxelNode_NoiseNode& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelDataComputeNode(Node, CompilationNode)
	, Dimension(Node.GetDimension())
	, bIsDerivative(Node.IsDerivative())
	, bIsFractal(Node.IsFractal())
	, Interpolation(Node.Interpolation)
	, NoiseVariable(FVoxelVariable("FVoxelFastNoise", UniqueName.ToString() + "_Noise"))
{
	const_cast<TVoxelStaticArray<TVoxelRange<v_flt>, 4>&>(OutputRanges).CopyFromArray(Node.OutputRanges);
}

void FVoxelNoiseComputeNode::Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct)
{
	PrivateNoise.SetSeed(Inputs[GetSeedInputIndex()]);
	InitNoise(FVoxelNoiseNodeHelper_Runtime(PrivateNoise));
}

void FVoxelNoiseComputeNode::InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const
{
	Constructor.AddLine(NoiseVariable.CppName + ".SetSeed(" + Inputs[GetSeedInputIndex()] + ");");
	InitNoise(FVoxelNoiseNodeHelper_Cpp(NoiseVariable.CppName, Constructor));
}

void FVoxelNoiseComputeNode::ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const
{
	for (int32 Index = 0; Index < OutputCount; Index++)
	{
		Outputs[Index].Get<v_flt>() = OutputRanges[Index];
	}
}

void FVoxelNoiseComputeNode::ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const
{
	const FString& X = Inputs[0];
	const FString& Y = Inputs[1];
	const FString& Z = Inputs[2];
	const FString& Freq = Dimension == 2 ? Inputs[2] : Inputs[3];
	
	FString Line;
	Line += Outputs[0] + " = ";
	Line += NoiseVariable.CppName + "." + GetFunctionName();
	Line += "(" + X + ", " + Y;
	if (Dimension == 3)
	{
		Line += ", " + Z;
	}
	Line += ", " + Freq;
	if (bIsFractal)
	{
		Line += ", " + GET_OCTAVES_CPP;
	}
	if (bIsDerivative)
	{
		for (uint32 Index = 0; Index < Dimension; Index++)
		{
			Line += "," + Outputs[Index + 1];
		}
	}
	Line += ");";
	Constructor.AddLine(Line);

	for (int32 Index = 0; Index < OutputCount; Index++)
	{
		Clamp(Constructor, Outputs, Index);
	}
}

void FVoxelNoiseComputeNode::ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const
{
	for (int32 Index = 0; Index < OutputCount; Index++)
	{
		Constructor.AddLinef(TEXT("%s = { %ff, %ff };"), *Outputs[Index], OutputRanges[Index].Min, OutputRanges[Index].Max);
	}
}

void FVoxelNoiseComputeNode::GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const
{
	PrivateVariables.Add(NoiseVariable);
}

void FVoxelNoiseComputeNode::InitNoise(const IVoxelNoiseNodeHelper& Noise) const
{
	Noise.SetInterpolation(Interpolation);
}

void FVoxelNoiseComputeNode::Clamp(FVoxelCppConstructor& Constructor,  const TArray<FString>& Outputs, int32 OutputIndex) const
{
	Constructor.AddLinef(TEXT("%s = FMath::Clamp<v_flt>(%s, %f, %f);"), *Outputs[OutputIndex], *Outputs[OutputIndex], OutputRanges[OutputIndex].Min, OutputRanges[OutputIndex].Max);
}

void FVoxelNoiseComputeNode::ComputeOutputRanges(UVoxelNode_NoiseNode& Node)
{
	auto& OutputRanges = Node.OutputRanges;
	OutputRanges.Reset();
	// Init to inf so the values aren't clamped
	OutputRanges.Init(TVoxelRange<v_flt>::Infinite(), 4);

	const auto CompilationNode = Node.GetCompilationNode();
	auto ComputeNode = StaticCastSharedPtr<FVoxelNoiseComputeNode>(Node.GetComputeNode(*CompilationNode));
	const int32 Dimension = ComputeNode->Dimension;

	ComputeNode->PrivateNoise.SetSeed(0);
	ComputeNode->InitNoise(FVoxelNoiseNodeHelper_Runtime(ComputeNode->PrivateNoise));
	
	TVoxelStaticArray<v_flt, MAX_VOXELNODE_PINS> Min;
	TVoxelStaticArray<v_flt, MAX_VOXELNODE_PINS> Max;
	TVoxelStaticArray<FVoxelNodeType, MAX_VOXELNODE_PINS> Inputs;
	TVoxelStaticArray<FVoxelNodeType, MAX_VOXELNODE_PINS> Outputs;

	Inputs.Memzero();
	Outputs.Memzero();

	ensure(ComputeNode->InputCount == Node.SetupInputsForComputeOutputRanges(Inputs));
	
	ComputeNode->Compute(Inputs.GetData(), Outputs.GetData(), FVoxelContext::EmptyContext);
	for (int32 OutputIndex = 0; OutputIndex < ComputeNode->OutputCount; OutputIndex++)
	{
		Min[OutputIndex] = Outputs[OutputIndex].Get<v_flt>();
		Max[OutputIndex] = Outputs[OutputIndex].Get<v_flt>();
	}

	for (uint32 Index = 0; Index < Node.NumberOfSamples; Index++)
	{
		for (int32 InputIndex = 0; InputIndex < Dimension; InputIndex++)
		{
			Inputs[InputIndex].Get<v_flt>() = FMath::FRandRange<v_flt>(-NOISE_SAMPLE_RANGE, NOISE_SAMPLE_RANGE);
		}
		ComputeNode->Compute(Inputs.GetData(), Outputs.GetData(), FVoxelContext::EmptyContext);
		for (int32 OutputIndex = 0; OutputIndex < ComputeNode->OutputCount; OutputIndex++)
		{
			Min[OutputIndex] = FMath::Min(Min[OutputIndex], Outputs[OutputIndex].Get<v_flt>());
			Max[OutputIndex] = FMath::Max(Max[OutputIndex], Outputs[OutputIndex].Get<v_flt>());
		}
	}

	OutputRanges.Reset();
	for (int32 OutputIndex = 0; OutputIndex < ComputeNode->OutputCount; OutputIndex++)
	{
		OutputRanges.Emplace(FVoxelRange(Min[OutputIndex], Max[OutputIndex]));
	}
	ExpandRanges(OutputRanges, Node.Tolerance);
}

void FVoxelNoiseComputeNode::ExpandRanges(TArray<FVoxelRange>& Ranges, float Tolerance)
{
	for (auto& Range : Ranges)
	{
		const float Delta = FMath::Abs(Range.Max - Range.Min) * Tolerance;
		Range.Min -= Delta;
		Range.Max += Delta;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FLinearColor UVoxelNode_NoiseNodeFractal::GetNodeBodyColor() const
{
	return FMath::Lerp<FLinearColor>(FColorList::White, FColorList::Orange, FMath::Clamp<float>((FractalOctaves - 1.f) / 10, 0, 1));
}

FLinearColor UVoxelNode_NoiseNodeFractal::GetColor() const
{
	return FMath::Lerp<FLinearColor>(FColorList::Black, FColorList::Orange, FMath::Clamp<float>((FractalOctaves - 1.f) / 10, 0, 1));
}

///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UVoxelNode_NoiseNodeFractal::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		LODToOctavesMap.Add("0", FractalOctaves);
		int32 MaxInt = 0;
		for (auto& It : LODToOctavesMap)
		{
			if (!It.Key.IsEmpty())
			{
				const int32 Int = FVoxelUtilities::ClampDepth<RENDER_CHUNK_SIZE>(TCString<TCHAR>::Atoi(*It.Key));
				MaxInt = FMath::Max(MaxInt, Int);
				It.Key = FString::FromInt(Int);
			}
		}
		for (auto& It : LODToOctavesMap)
		{
			if (It.Key.IsEmpty())
			{
				if (uint8 * LOD = LODToOctavesMap.Find(FString::FromInt(MaxInt)))
				{
					It.Value = *LOD - 1;
				}
				It.Key = FString::FromInt(++MaxInt);
			}
		}
		LODToOctavesMap.KeySort([](const FString& A, const FString& B) { return TCString<TCHAR>::Atoi(*A) < TCString<TCHAR>::Atoi(*B); });
	}
}
#endif

void UVoxelNode_NoiseNodeFractal::PostLoad()
{
	LODToOctavesMap.Add("0", FractalOctaves);
	Super::PostLoad(); // Make sure to call ComputeRange after
}

void UVoxelNode_NoiseNodeFractal::PostInitProperties()
{
	LODToOctavesMap.Add("0", FractalOctaves);
	Super::PostInitProperties(); // Make sure to call ComputeRange after
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static TStaticArray<uint8, 32> InitializeLODToOctaves(const UVoxelNode_NoiseNodeFractal& Node)
{
	TMap<uint8, uint8> Map;
	for (auto& It : Node.LODToOctavesMap)
	{
		if (!It.Key.IsEmpty())
		{
			Map.Add(TCString<TCHAR>::Atoi(*It.Key), It.Value);
		}
	}
	Map.Add(0, Node.FractalOctaves);

	TStaticArray<uint8, 32> LODToOctavesArray;
	for (int32 LODIt = 0; LODIt < 32; LODIt++)
	{
		int32 LOD = LODIt;
		while (!Map.Contains(LOD))
		{
			LOD--;
			check(LOD >= 0);
		}
		LODToOctavesArray[LODIt] = Map[LOD];
	}
	return LODToOctavesArray;
}

FVoxelNoiseFractalComputeNode::FVoxelNoiseFractalComputeNode(
	const UVoxelNode_NoiseNodeFractal& Node,
	const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseComputeNode(Node, CompilationNode)
	, FractalOctaves(Node.FractalOctaves)
	, FractalLacunarity(Node.FractalLacunarity)
	, FractalGain(Node.FractalGain)
	, FractalType(Node.FractalType)
	, LODToOctaves(InitializeLODToOctaves(Node)) // Инициализация константного члена
	, LODToOctavesVariable("TStaticArray<uint8, 32>", UniqueName.ToString() + "_LODToOctaves")
{
}

void FVoxelNoiseFractalComputeNode::InitNoise(const IVoxelNoiseNodeHelper& Noise) const
{
	FVoxelNoiseComputeNode::InitNoise(Noise);

	Noise.SetFractalOctavesAndGain(FractalOctaves, FractalGain);
	Noise.SetFractalLacunarity(FractalLacunarity);
	Noise.SetFractalType(FractalType);
}

void FVoxelNoiseFractalComputeNode::InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const
{
	FVoxelNoiseComputeNode::InitCpp(Inputs, Constructor);
	
	for (int32 LOD = 0; LOD < 32; LOD++)
	{
		Constructor.AddLinef(TEXT("%s[%d] = %u;"), *LODToOctavesVariable.CppName, LOD, LODToOctaves[LOD]);
	}
}

void FVoxelNoiseFractalComputeNode::GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const
{
	FVoxelNoiseComputeNode::GetPrivateVariables(PrivateVariables);

	PrivateVariables.Add(LODToOctavesVariable);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_IQNoiseBase::UVoxelNode_IQNoiseBase()
{
	bComputeDerivative = true;
	FractalType = EVoxelNoiseFractalType::FBM;
	FractalOctaves = 15;
	Frequency = 0.001;
	NumberOfSamples = 1000000;
}

#if WITH_EDITOR
bool UVoxelNode_IQNoiseBase::CanEditChange(const FProperty* InProperty) const
{
	return
		Super::CanEditChange(InProperty) &&
		InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_IQNoiseBase, bComputeDerivative) &&
		InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_IQNoiseBase, FractalType);
}
#endif

FVoxel2DIQNoiseComputeNode::FVoxel2DIQNoiseComputeNode(const UVoxelNode_2DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseFractalComputeNode(Node, CompilationNode)
	, Rotation(Node.Rotation)
{
}

void FVoxel2DIQNoiseComputeNode::InitNoise(const IVoxelNoiseNodeHelper& Noise) const
{
	FVoxelNoiseFractalComputeNode::InitNoise(Noise);
	Noise.SetMatrixFromRotation_2D(Rotation);
}

FVoxel3DIQNoiseComputeNode::FVoxel3DIQNoiseComputeNode(const UVoxelNode_3DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseFractalComputeNode(Node, CompilationNode)
	, Rotation(Node.Rotation)
{

}

void FVoxel3DIQNoiseComputeNode::InitNoise(const IVoxelNoiseNodeHelper& Noise) const
{
	FVoxelNoiseFractalComputeNode::InitNoise(Noise);
	Noise.SetMatrixFromRotation_3D(Rotation);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelCellularNoiseComputeNode::FVoxelCellularNoiseComputeNode(const UVoxelNode_CellularNoise& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseComputeNode(Node, CompilationNode)
	, DistanceFunction(Node.DistanceFunction)
	, ReturnType(Node.ReturnType)
	, Jitter(Node.Jitter)
{

}

void FVoxelCellularNoiseComputeNode::InitNoise(const IVoxelNoiseNodeHelper& Noise) const
{
	FVoxelNoiseComputeNode::InitNoise(Noise);

	Noise.SetCellularJitter(Jitter);
	Noise.SetCellularDistanceFunction(DistanceFunction);
	Noise.SetCellularReturnType(ReturnType);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelCraterNoiseComputeNode::FVoxelCraterNoiseComputeNode(const UVoxelNode_CraterNoise& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseComputeNode(Node, CompilationNode)
	, Jitter(Node.Jitter)
	, FalloffExponent(Node.FalloffExponent)
{
}

void FVoxelCraterNoiseComputeNode::InitNoise(const IVoxelNoiseNodeHelper& Noise) const
{
	FVoxelNoiseComputeNode::InitNoise(Noise);

	Noise.SetCellularJitter(Jitter);
	Noise.SetCraterFalloffExponent(FalloffExponent);
}

///////////////////////////////////////////////////////////////////////////////

FVoxelCraterNoiseFractalComputeNode::FVoxelCraterNoiseFractalComputeNode(const UVoxelNode_CraterNoiseFractal& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseFractalComputeNode(Node, CompilationNode)
	, Jitter(Node.Jitter)
	, FalloffExponent(Node.FalloffExponent)
{
}

void FVoxelCraterNoiseFractalComputeNode::InitNoise(const IVoxelNoiseNodeHelper& Noise) const
{
	FVoxelNoiseFractalComputeNode::InitNoise(Noise);

	Noise.SetCellularJitter(Jitter);
	Noise.SetCraterFalloffExponent(FalloffExponent);
}
