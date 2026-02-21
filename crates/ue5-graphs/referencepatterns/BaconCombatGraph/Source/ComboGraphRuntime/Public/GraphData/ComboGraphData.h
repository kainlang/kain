// © 2025 mrbaconvn. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 
#include "ComboGraphData.generated.h"

class UComboGraphInstance;
class UComboNodeData;
class UComboGraphNodeData;
struct FGameplayTag;

UCLASS()
class COMBOGRAPHRUNTIME_API UComboPinData : public UObject 
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FName PinName;

    UPROPERTY()
    FGuid PinId;

    UPROPERTY()
    TArray<UComboPinData*> ConnectToPins;

    UPROPERTY()
    UComboGraphNodeData* Parent = nullptr;
};

UCLASS()
class COMBOGRAPHRUNTIME_API UComboGraphNodeData : public UObject 
{
    GENERATED_BODY()

public:
    UComboGraphNodeData* GetNextOutputNodeByPinIndex(int OutputPinIndex) const; 
    virtual const UComboGraphNodeData* ExecuteNode(UComboGraphInstance* GraphInstance) const; 

public:
    UPROPERTY()
    TArray<UComboPinData*> InputPins;

    UPROPERTY()
    TArray<UComboPinData*> OutputPins;

    UPROPERTY()
	FIntPoint NodePosition;

    UPROPERTY()
    FGuid NodeGuid; 
};

UCLASS()
class COMBOGRAPHRUNTIME_API UComboGraphData : public UObject {
    GENERATED_BODY()

public:
	UComboPinData* FindPinById(FGuid PinId);  

    template <typename T>
    T* GetNodeData()
    {
        static_assert(TIsDerivedFrom<T, UComboGraphNodeData>::IsDerived, "T must be derived from UComboGraphNodeData");

        for (int i = 0; i < Nodes.Num(); i++)
        {
            if (!IsValid(Nodes[i]))
            {
                continue;
            }
            if (Nodes[i]->IsA<T>())
            {
                return Cast<T>(Nodes[i]);
            }
        }

        return nullptr;
    }

    template <typename T>
    TArray<T*> GetListNodeData()
    {
        static_assert(TIsDerivedFrom<T, UComboGraphNodeData>::IsDerived, "T must be derived from UComboGraphNodeData");

        TArray<T*> Result;
        for (int i = 0; i < Nodes.Num(); i++)
        {
            if (!IsValid(Nodes[i]))
            {
                continue;
            }
            if (Nodes[i]->IsA<T>())
            {
                Result.Add(Cast<T>(Nodes[i]));
            }
        }

        return Result;
    }

public:
    UPROPERTY();
    TArray<UComboGraphNodeData*> Nodes;
};
