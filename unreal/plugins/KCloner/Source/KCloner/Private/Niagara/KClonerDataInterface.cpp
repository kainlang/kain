// Copyright 2026 K-Studio. All Rights Reserved.

#include "Niagara/KClonerDataInterface.h"
#include "KClonerActor.h"
#include "NiagaraDataInterface.h"
#include "NiagaraShader.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraTypes.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "ShaderParameterUtils.h"

#define LOCTEXT_NAMESPACE "KClonerDataInterface"

static const FName GetCloneCountName(TEXT("GetCloneCount"));
static const FName GetCloneTransformName(TEXT("GetCloneTransform"));
static const FName GetCloneColorName(TEXT("GetCloneColor"));

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------
// RHI / Render Thread Structures
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct FNDIKClonerData {
  int32 InstanceCount = 0;
  TArray<FVector3f> PositionData;
  TArray<FVector4f> RotationData;
  TArray<FVector3f> ScaleData;
  TArray<FLinearColor> ColorData;
};

struct FKClonerDataInterfaceProxy : public FNiagaraDataInterfaceProxy {
  int32 InstanceCount = 0;
  FReadBuffer PositionBuffer;
  FReadBuffer RotationBuffer;
  FReadBuffer ScaleBuffer;
  FReadBuffer ColorBuffer;

  virtual void ConsumePerInstanceDataFromGameThread(
      void *PerInstanceData,
      const FNiagaraSystemInstanceID &Instance) override {
    if (!PerInstanceData)
      return;

    FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

    FNDIKClonerData *GameThreadData =
        static_cast<FNDIKClonerData *>(PerInstanceData);
    InstanceCount = GameThreadData->InstanceCount;

    // update the SRV buffers on the GPU
    // this is where the magic happens for Niagara support
    auto UpdateBuffer = [&](FReadBuffer &Buffer, const void *Data,
                            int32 NumElements, int32 Stride,
                            EPixelFormat Format, const TCHAR *Name) {
      if (NumElements > 0) {
        if (Buffer.NumBytes != NumElements * Stride) {
          Buffer.Release();
          Buffer.Initialize(RHICmdList, Name, Stride, NumElements, Format,
                            BUF_Static);
        }

        if (Data) {
          void *Dest = RHICmdList.LockBuffer(
              Buffer.Buffer, 0, NumElements * Stride, RLM_WriteOnly);
          FMemory::Memcpy(Dest, Data, NumElements * Stride);
          RHICmdList.UnlockBuffer(Buffer.Buffer);
        }
      } else {
        if (Buffer.NumBytes > 0)
          Buffer.Release();
      }
    };

    UpdateBuffer(PositionBuffer, GameThreadData->PositionData.GetData(),
                 InstanceCount, sizeof(FVector3f), PF_R32G32B32F,
                 TEXT("KClonerPos"));
    UpdateBuffer(RotationBuffer, GameThreadData->RotationData.GetData(),
                 InstanceCount, sizeof(FVector4f), PF_A32B32G32R32F,
                 TEXT("KClonerRot"));
    UpdateBuffer(ScaleBuffer, GameThreadData->ScaleData.GetData(),
                 InstanceCount, sizeof(FVector3f), PF_R32G32B32F,
                 TEXT("KClonerScale"));
    UpdateBuffer(ColorBuffer, GameThreadData->ColorData.GetData(),
                 InstanceCount, sizeof(FLinearColor), PF_A32B32G32R32F,
                 TEXT("KClonerColor"));

    delete GameThreadData;
  }

  virtual int32 PerInstanceDataPassedToRenderThreadSize() const override {
    return 0;
  }
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Shader Parameters
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------

BEGIN_SHADER_PARAMETER_STRUCT(FKClonerShaderParameters, )
SHADER_PARAMETER(int32, InstanceCount)
SHADER_PARAMETER_SRV(Buffer<float3>, PositionBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, RotationBuffer)
SHADER_PARAMETER_SRV(Buffer<float3>, ScaleBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, ColorBuffer)
END_SHADER_PARAMETER_STRUCT()

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------
// UKClonerDataInterface Implementation
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------

UKClonerDataInterface::UKClonerDataInterface(
    const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer) {
  Proxy.Reset(new FKClonerDataInterfaceProxy());
}

void UKClonerDataInterface::PostInitProperties() {
  Super::PostInitProperties();
  if (HasAnyFlags(RF_ClassDefaultObject)) {
    ENiagaraTypeRegistryFlags Flags =
        ENiagaraTypeRegistryFlags::AllowAnyVariable |
        ENiagaraTypeRegistryFlags::AllowParameter;
    FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
  }
}

void UKClonerDataInterface::GetFunctions(
    TArray<FNiagaraFunctionSignature> &OutFunctions) {
  // GetCloneCount
  FNiagaraFunctionSignature SigCount;
  SigCount.Name = GetCloneCountName;
  SigCount.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()),
                                       TEXT("K-Cloner DI")));
  SigCount.Outputs.Add(
      FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
  SigCount.bMemberFunction = true;
  SigCount.bRequiresContext = false;
  SigCount.SetDescription(
      LOCTEXT("GetCloneCountDesc", "Returns the active clone count."));
  OutFunctions.Add(SigCount);

  // GetCloneTransform
  FNiagaraFunctionSignature SigTransform;
  SigTransform.Name = GetCloneTransformName;
  SigTransform.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()),
                                           TEXT("K-Cloner DI")));
  SigTransform.Inputs.Add(
      FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
  SigTransform.Outputs.Add(
      FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
  SigTransform.Outputs.Add(
      FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Rotation")));
  SigTransform.Outputs.Add(
      FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));
  SigTransform.bMemberFunction = true;
  SigTransform.bRequiresContext = false;
  SigTransform.SetDescription(
      LOCTEXT("GetCloneTransformDesc",
              "Returns the transform (Pos, Rot, Scale) of a clone by index."));
  OutFunctions.Add(SigTransform);

  // GetCloneColor
  FNiagaraFunctionSignature SigColor;
  SigColor.Name = GetCloneColorName;
  SigColor.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()),
                                       TEXT("K-Cloner DI")));
  SigColor.Inputs.Add(
      FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
  SigColor.Outputs.Add(
      FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
  SigColor.bMemberFunction = true;
  SigColor.bRequiresContext = false;
  SigColor.SetDescription(
      LOCTEXT("GetCloneColorDesc",
              "Returns the calculated color of a clone by index."));
  OutFunctions.Add(SigColor);
}

void UKClonerDataInterface::VMGetCloneCount(
    FVectorVMExternalFunctionContext &Context) {
  VectorVM::FUserPtrHandler<UKClonerDataInterface> InstData(Context);
  FNDIOutputParam<int32> OutCount(Context);

  int32 Count = 0;
  if (AKClonerActor *Actor = InstData->ClonerActor.Get()) {
    Count = Actor->GetInstanceCount();
  }

  for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
    OutCount.SetAndAdvance(Count);
  }
}

void UKClonerDataInterface::VMGetCloneTransform(
    FVectorVMExternalFunctionContext &Context) {
  VectorVM::FUserPtrHandler<UKClonerDataInterface> InstData(Context);
  FNDIInputParam<int32> InIndex(Context);
  FNDIOutputParam<FVector3f> OutPos(Context);
  FNDIOutputParam<FVector4f> OutRot(Context);
  FNDIOutputParam<FVector3f> OutScale(Context);

  AKClonerActor *Actor = InstData->ClonerActor.Get();
  const TArray<FTransform> *Transforms =
      Actor ? &Actor->GetCachedTransforms() : nullptr;

  for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
    int32 Index = InIndex.GetAndAdvance();
    FVector3f Pos = FVector3f::ZeroVector;
    FVector4f Rot = FVector4f(0, 0, 0, 1);
    FVector3f Scale = FVector3f::OneVector;

    if (Transforms && Transforms->IsValidIndex(Index)) {
      const FTransform &T = (*Transforms)[Index];
      Pos = (FVector3f)T.GetLocation();
      FQuat4f Q = FQuat4f(T.GetRotation());
      Rot = FVector4f(Q.X, Q.Y, Q.Z, Q.W);
      Scale = (FVector3f)T.GetScale3D();
    }

    OutPos.SetAndAdvance(Pos);
    OutRot.SetAndAdvance(Rot);
    OutScale.SetAndAdvance(Scale);
  }
}

void UKClonerDataInterface::VMGetCloneColor(
    FVectorVMExternalFunctionContext &Context) {
  VectorVM::FUserPtrHandler<UKClonerDataInterface> InstData(Context);
  FNDIInputParam<int32> InIndex(Context);
  FNDIOutputParam<FLinearColor> OutColor(Context);

  AKClonerActor *Actor = InstData->ClonerActor.Get();
  const TArray<FKClonerInstanceCache> *Data =
      Actor ? &Actor->GetCachedInstanceData() : nullptr;

  for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
    int32 Index = InIndex.GetAndAdvance();
    FLinearColor Color = FLinearColor::White;

    if (Data && Data->IsValidIndex(Index)) {
      const FKClonerInstanceCache &Cache = (*Data)[Index];
      Color = FLinearColor(Cache.R, Cache.G, Cache.B, 1.0f);
    }

    OutColor.SetAndAdvance(Color);
  }
}

// BIND HLSL FUNCTIONS
// maps Niagara script function names to C++ methods
void UKClonerDataInterface::GetVMExternalFunction(
    const FVMExternalFunctionBindingInfo &BindingInfo, void *InstanceData,
    FVMExternalFunction &OutFunc) {
  if (BindingInfo.Name == GetCloneCountName) {
    OutFunc = FVMExternalFunction::CreateUObject(
        this, &UKClonerDataInterface::VMGetCloneCount);
  } else if (BindingInfo.Name == GetCloneTransformName) {
    OutFunc = FVMExternalFunction::CreateUObject(
        this, &UKClonerDataInterface::VMGetCloneTransform);
  } else if (BindingInfo.Name == GetCloneColorName) {
    OutFunc = FVMExternalFunction::CreateUObject(
        this, &UKClonerDataInterface::VMGetCloneColor);
  }
}

bool UKClonerDataInterface::Equals(const UNiagaraDataInterface *Other) const {
  if (const UKClonerDataInterface *OtherDI =
          Cast<UKClonerDataInterface>(Other)) {
    return ClonerActor == OtherDI->ClonerActor;
  }
  return false;
}

void UKClonerDataInterface::ProvidePerInstanceDataForRenderThread(
    void *DataForRenderThread, void *PerInstanceData,
    const FNiagaraSystemInstanceID &SystemInstance) {
  FNDIKClonerData *Data = new FNDIKClonerData();

  if (AKClonerActor *Actor = ClonerActor.Get()) {
    const TArray<FTransform> &Transforms = Actor->GetCachedTransforms();
    const TArray<FKClonerInstanceCache> &InstanceData =
        Actor->GetCachedInstanceData();
    Data->InstanceCount = Transforms.Num();

    if (Data->InstanceCount > 0) {
      Data->PositionData.SetNumUninitialized(Data->InstanceCount);
      Data->RotationData.SetNumUninitialized(Data->InstanceCount);
      Data->ScaleData.SetNumUninitialized(Data->InstanceCount);
      Data->ColorData.SetNumUninitialized(Data->InstanceCount);

      for (int32 i = 0; i < Data->InstanceCount; ++i) {
        const FTransform &T = Transforms[i];
        Data->PositionData[i] = (FVector3f)T.GetLocation();

        FQuat4f Q = FQuat4f(T.GetRotation());
        Data->RotationData[i] = FVector4f(Q.X, Q.Y, Q.Z, Q.W);

        Data->ScaleData[i] = (FVector3f)T.GetScale3D();

        if (InstanceData.IsValidIndex(i)) {
          const FKClonerInstanceCache &Cache = InstanceData[i];
          Data->ColorData[i] = FLinearColor(Cache.R, Cache.G, Cache.B, 1.0f);
        } else {
          Data->ColorData[i] = FLinearColor::White;
        }
      }
    }
  }

  GetProxyAs<FKClonerDataInterfaceProxy>()
      ->ConsumePerInstanceDataFromGameThread(Data, SystemInstance);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------
// GPU Support
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UKClonerDataInterface::BuildShaderParameters(
    FNiagaraShaderParametersBuilder &ShaderParametersBuilder) const {
  ShaderParametersBuilder.AddNestedStruct<FKClonerShaderParameters>();
}

void UKClonerDataInterface::SetShaderParameters(
    const FNiagaraDataInterfaceSetShaderParametersContext &Context) const {
  FKClonerDataInterfaceProxy &DIProxy =
      Context.GetProxy<FKClonerDataInterfaceProxy>();
  FKClonerShaderParameters *Parameters =
      Context.GetParameterNestedStruct<FKClonerShaderParameters>();

  if (Parameters) {
    Parameters->InstanceCount = DIProxy.InstanceCount;
    Parameters->PositionBuffer = DIProxy.PositionBuffer.SRV;
    Parameters->RotationBuffer = DIProxy.RotationBuffer.SRV;
    Parameters->ScaleBuffer = DIProxy.ScaleBuffer.SRV;
    Parameters->ColorBuffer = DIProxy.ColorBuffer.SRV;
  }
}

#if WITH_EDITORONLY_DATA
void UKClonerDataInterface::GetParameterDefinitionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo &ParamInfo, FString &OutHLSL) {
  OutHLSL += TEXT("int InstanceCount;\n");
  OutHLSL += TEXT("Buffer<float3> PositionBuffer;\n");
  OutHLSL += TEXT("Buffer<float4> RotationBuffer;\n");
  OutHLSL += TEXT("Buffer<float3> ScaleBuffer;\n");
  OutHLSL += TEXT("Buffer<float4> ColorBuffer;\n");
}

bool UKClonerDataInterface::GetFunctionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo &ParamInfo,
    const FNiagaraDataInterfaceGeneratedFunction &FunctionInfo,
    int FunctionInstanceIndex, FString &OutHLSL) {
  if (FunctionInfo.DefinitionName == GetCloneCountName) {
    OutHLSL += TEXT(
        "void GetCloneCount(out int OutCount) { OutCount = InstanceCount; }");
    return true;
  } else if (FunctionInfo.DefinitionName == GetCloneTransformName) {
    OutHLSL += TEXT("void GetCloneTransform(int Index, out float3 OutPos, out "
                    "float4 OutRot, out float3 OutScale) \n{\n");
    OutHLSL += TEXT("	if(Index >= 0 && Index < InstanceCount) {\n");
    OutHLSL += TEXT("		OutPos = PositionBuffer[Index];\n");
    OutHLSL += TEXT("		OutRot = RotationBuffer[Index];\n");
    OutHLSL += TEXT("		OutScale = ScaleBuffer[Index];\n");
    OutHLSL += TEXT("	} else {\n");
    OutHLSL += TEXT("		OutPos = float3(0,0,0);\n");
    OutHLSL += TEXT("		OutRot = float4(0,0,0,1);\n");
    OutHLSL += TEXT("		OutScale = float3(1,1,1);\n");
    OutHLSL += TEXT("	}\n}\n");
    return true;
  } else if (FunctionInfo.DefinitionName == GetCloneColorName) {
    OutHLSL += TEXT("void GetCloneColor(int Index, out float4 OutColor) \n{\n");
    OutHLSL += TEXT("	if(Index >= 0 && Index < InstanceCount) {\n");
    OutHLSL += TEXT("		OutColor = ColorBuffer[Index];\n");
    OutHLSL += TEXT("	} else {\n");
    OutHLSL += TEXT("		OutColor = float4(1,1,1,1);\n");
    OutHLSL += TEXT("	}\n}\n");
    return true;
  }
  return false;
}
#endif

#undef LOCTEXT_NAMESPACE
