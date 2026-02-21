// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "VoxelRange.h"
#include "VoxelNode.h"
#include "VoxelNodeHelper.h"
#include "VoxelNodeHelperMacros.h"
#include "FastNoise/VoxelFastNoise.h"
#include "CppTranslation/VoxelVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "VoxelNoiseNodesBase.generated.h"

class FVoxelCppConstructor;
class IVoxelNoiseNodeHelper;
struct FVoxelGeneratorInit;

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNode : public UVoxelNodeWithContext // For LOD for octaves
{
	GENERATED_BODY()

public:
	// Do not use here, exposed as pin now
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Noise settings", meta = (DisplayName = "Old Frequency"))
	float Frequency = 0.02;

	UPROPERTY(EditAnywhere, Category = "Noise settings")
	EVoxelNoiseInterpolation Interpolation = EVoxelNoiseInterpolation::Quintic;

	// To find the output range, NumberOfSamples random samples are computed
	// Increase this if the output range is too irregular, and if you start to see flat areas in your noise
	UPROPERTY(EditAnywhere, Category = "Range analysis settings")
	uint32 NumberOfSamples = 1000000;

	// The range analysis interval will be expended by this much (relatively). Increase if you see flat areas in your noise
	UPROPERTY(EditAnywhere, Category = "Range analysis settings", meta = (UIMin = 0, UIMax = 1))
	float Tolerance = 0.1f;
	
	UPROPERTY(VisibleAnywhere, Category = "Range analysis settings")
	TArray<FVoxelRange> OutputRanges;

	//~ Begin UVoxelNode Interface
	virtual FName GetInputPinName(int32 PinIndex) const override;
	virtual FName GetOutputPinName(int32 PinIndex) const override;
	virtual FString GetInputPinToolTip(int32 PinIndex) const override;
	virtual FString GetOutputPinToolTip(int32 PinIndex) const override;
	virtual int32 GetMinInputPins() const override { return GetBaseInputPinsCount() + CustomNoisePins.InputPins.Num(); }
	virtual int32 GetMaxInputPins() const override { return GetMinInputPins(); }
	virtual int32 GetOutputPinsCount() const override { return GetBaseOutputPinsCount() + CustomNoisePins.OutputPins.Num(); }
	virtual EVoxelPinCategory GetInputPinCategory(int32 PinIndex) const override;
	virtual EVoxelPinCategory GetOutputPinCategory(int32 PinIndex) const override;
	virtual FString GetInputPinDefaultValue(int32 PinIndex) const override;
	virtual TSharedPtr<FVoxelCompilationNode> GetCompilationNode() const override;
	//~ End UVoxelNode Interface

	//~ Begin UVoxelNode_NoiseNode Interface
	virtual uint32 GetDimension() const { unimplemented(); return 0; }
	virtual bool IsDerivative() const { return false; }
	virtual bool IsFractal() const { return false; }
	virtual bool NeedRangeAnalysis() const { return true; }
	virtual void ComputeOutputRanges();
	// Returns num input pins to detect errors
	virtual int32 SetupInputsForComputeOutputRanges(TVoxelStaticArray<FVoxelNodeType, MAX_VOXELNODE_PINS>& Inputs) const;
	//~ End UVoxelNode_NoiseNode Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface

protected:
	FVoxelPinsHelper CustomNoisePins;

	int32 GetBaseInputPinsCount() const { return GetDimension() + 2; }
	int32 GetBaseOutputPinsCount() const { return IsDerivative() ? GetDimension() + 1 : 1; }
};

class FVoxelNoiseComputeNode : public FVoxelDataComputeNode
{
public:
	const uint32 Dimension;
	const bool bIsDerivative;
	const bool bIsFractal;

	FVoxelNoiseComputeNode(const UVoxelNode_NoiseNode& Node, const FVoxelCompilationNode& CompilationNode);

	void Init(FVoxelGraphSeed Inputs[], const FVoxelGeneratorInit& InitStruct) override;
	void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override;

	void ComputeRange(FVoxelNodeInputRangeBuffer Inputs, FVoxelNodeOutputRangeBuffer Outputs, const FVoxelContextRange& Context) const override;
	void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override;
	void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override;

	void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override;

	//~ Begin FVoxelNoiseComputeNode Interface
	virtual FString GetFunctionName() const { return ""; }
	virtual int32 GetSeedInputIndex() const { return Dimension + 1; }

	virtual void InitNoise(const IVoxelNoiseNodeHelper& Noise) const;
	//~ End FVoxelNoiseComputeNode Interface

protected:
	const FVoxelFastNoise& GetNoise() const { return PrivateNoise; }
	const FString& GetNoiseName() const { return NoiseVariable.CppName; }
	const TVoxelStaticArray<TVoxelRange<v_flt>, 4>& GetOutputRanges() const { return OutputRanges; }

	FORCEINLINE void Clamp(FVoxelNodeOutputBuffer Outputs, int32 OutputIndex) const
	{
		Outputs[OutputIndex].Get<v_flt>() = FMath::Clamp<v_flt>(Outputs[OutputIndex].Get<v_flt>(), OutputRanges[OutputIndex].Min, OutputRanges[OutputIndex].Max);
	}
	void Clamp(FVoxelCppConstructor& Constructor, const TArray<FString>& Outputs, int32 OutputIndex) const;

private:
	const EVoxelNoiseInterpolation Interpolation;
	const TVoxelStaticArray<TVoxelRange<v_flt>, 4> OutputRanges;
	const FVoxelVariable NoiseVariable;
	FVoxelFastNoise PrivateNoise;

public:
	static void ComputeOutputRanges(UVoxelNode_NoiseNode& Node);
	static void ExpandRanges(TArray<FVoxelRange>& Ranges, float Tolerance);
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNodeFractal : public UVoxelNode_NoiseNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings", meta = (UIMin = 1, ClampMin = 1))
	int32 FractalOctaves = 3;

	// A multiplier that determines how quickly the frequency increases for each successive octave
	// The frequency of each successive octave is equal to the product of the previous octave's frequency and the lacunarity value.
	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings", meta = (UIMin = 0))
	float FractalLacunarity = 2;

	// A multiplier that determines how quickly the amplitudes diminish for each successive octave
	// The amplitude of each successive octave is equal to the product of the previous octave's amplitude and the gain value. Increasing the gain produces "rougher" Perlin noise.
	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings", meta = (UIMin = 0, UIMax = 1))
	float FractalGain = 0.5;

	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings")
	EVoxelNoiseFractalType FractalType = EVoxelNoiseFractalType::FBM;

	// To use lower quality noise for far LODs
	UPROPERTY(EditAnywhere, Category = "LOD settings", meta = (DisplayName = "LOD to Octaves map"))
	TMap<FString, uint8> LODToOctavesMap;

	//~ Begin UVoxelNode Interface
	virtual FLinearColor GetNodeBodyColor() const override;
	virtual FLinearColor GetColor() const override;
	//~ End UVoxelNode Interface

	//~ Begin UVoxelNode_NoiseNode Interface
	virtual bool IsFractal() const override final { return true; }
	//~ End UVoxelNode_NoiseNode Interface
	
protected:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface
};

class FVoxelNoiseFractalComputeNode : public FVoxelNoiseComputeNode
{
public:
	FVoxelNoiseFractalComputeNode(const UVoxelNode_NoiseNodeFractal& Node, const FVoxelCompilationNode& CompilationNode);
	
	virtual void InitNoise(const IVoxelNoiseNodeHelper& Noise) const override;

	virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override;
	virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override;
	
	const uint8 FractalOctaves;
	const float FractalLacunarity;
	const float FractalGain;
	const EVoxelNoiseFractalType FractalType;
	const TStaticArray<uint8, 32> LODToOctaves;
	const FVoxelVariable LODToOctavesVariable;
};

#define GET_OCTAVES LODToOctaves[FMath::Clamp(Context.LOD, 0, 31)]
#define GET_OCTAVES_CPP FString(static_cast<const FVoxelNoiseFractalComputeNode&>(*this).LODToOctavesVariable.CppName + "[FMath::Clamp(" + FVoxelCppIds::Context + ".LOD, 0, 31)]")

//////////////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNodeWithDerivative : public UVoxelNode_NoiseNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Noise settings", meta = (ReconstructNode))
	bool bComputeDerivative = false;

	virtual bool IsDerivative() const override { return bComputeDerivative; }
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNodeWithDerivativeFractal : public UVoxelNode_NoiseNodeFractal
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Noise settings", meta = (ReconstructNode))
	bool bComputeDerivative = false;

	virtual bool IsDerivative() const override { return bComputeDerivative; }
};

//////////////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_IQNoiseBase : public UVoxelNode_NoiseNodeWithDerivativeFractal
{
	GENERATED_BODY()

public:
	UVoxelNode_IQNoiseBase();

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	//~ End UObject Interface
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_2DIQNoiseBase : public UVoxelNode_IQNoiseBase
{
	GENERATED_BODY()

public:
	// Rotation (in degrees) applied to the position between each octave
	UPROPERTY(EditAnywhere, Category = "IQ Noise settings")
	float Rotation = 40;
};

class FVoxel2DIQNoiseComputeNode : public FVoxelNoiseFractalComputeNode
{
public:
	FVoxel2DIQNoiseComputeNode(const UVoxelNode_2DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode);

	void InitNoise(const IVoxelNoiseNodeHelper& Noise) const override;

private:
	const float Rotation;
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_3DIQNoiseBase : public UVoxelNode_IQNoiseBase
{
	GENERATED_BODY()

public:
	// Rotation (in degrees) applied to the position between each octave
	UPROPERTY(EditAnywhere, Category = "IQ Noise settings")
	FRotator Rotation = { 40, 45, 50 };
};

class FVoxel3DIQNoiseComputeNode : public FVoxelNoiseFractalComputeNode
{
public:
	FVoxel3DIQNoiseComputeNode(const UVoxelNode_3DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode);

	void InitNoise(const IVoxelNoiseNodeHelper& Noise) const override;

private:
	const FRotator Rotation;
};
