// Copyright 2026 K-Studio. All Rights Reserved.

// clang-format off
#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "KClonerDataInterface.generated.h"

class AKClonerActor;
class FNiagaraRenderGraphBuilder;

/**
 * Data Interface to read K-Cloner instance data (Transforms, Colors) directly into Niagara.
 * Supports both CPU (for spawning/logic) and GPU (for massive particle counts).
 */
UCLASS(EditInlineNew, Category = "K-Cloner", meta = (DisplayName = "K-Cloner Data Interface"))
class KCLONER_API UKClonerDataInterface : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	/** The Cloner Actor to read data from. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (AllowedClasses = "/Script/KCloner.KClonerActor"))
	TSoftObjectPtr<AKClonerActor> ClonerActor;

	//----------------------------------------------------------------------------
	// UObject Interface
	virtual void PostInitProperties() override;
	//----------------------------------------------------------------------------

	//----------------------------------------------------------------------------
	// UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	// virtual bool CopyTo(UNiagaraDataInterface* Destination) const override; // Disabled to avoid signature mismatch
	//----------------------------------------------------------------------------

	//----------------------------------------------------------------------------
	// GPU Interface
#if WITH_EDITORONLY_DATA
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif
	
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;

	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	/** VM Functions (CPU) */
	void VMGetCloneCount(FVectorVMExternalFunctionContext& Context);
	void VMGetCloneTransform(FVectorVMExternalFunctionContext& Context);
	void VMGetCloneColor(FVectorVMExternalFunctionContext& Context);

protected:
	// Helper (can't override if signature mismatch, so just implement matching DI spec)
};
// clang-format on
