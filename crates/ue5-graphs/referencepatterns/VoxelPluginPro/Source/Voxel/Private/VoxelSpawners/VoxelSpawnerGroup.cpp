// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelSpawnerGroup.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelMessages.h"

#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

FVoxelSpawnerGroupProxyResult::FVoxelSpawnerGroupProxyResult(const FVoxelSpawnerProxy& Proxy)
	: FVoxelSpawnerProxyResult(EVoxelSpawnerProxyType::SpawnerGroup, Proxy)
{

}

void FVoxelSpawnerGroupProxyResult::Init(TArray<TUniquePtr<FVoxelSpawnerProxyResult>>&& InResults)
{
	Results = MoveTemp(InResults);
}

void FVoxelSpawnerGroupProxyResult::CreateImpl()
{
	for (auto& Result : Results)
	{
		check(Result.IsValid());
		Result->Create();
	}
}

void FVoxelSpawnerGroupProxyResult::DestroyImpl()
{
	for (auto& Result : Results)
	{
		check(Result.IsValid());
		Result->Destroy();
	}
}

void FVoxelSpawnerGroupProxyResult::SerializeImpl(FArchive& Ar, FVoxelSpawnersSaveVersion::Type Version)
{
	// TODO
#if 0
	if (Ar.IsSaving())
	{
		int32 NumResults = Results.Num();
		Ar << NumResults;
		
		for (auto& Result : Results)
		{
			EVoxelSpawnerProxyType ProxyType = Result->Type;
			Ar << ProxyType;
			
			Result->SerializeProxy(Ar, VoxelCustomVersion);
		}
	}
	else
	{
		check(Ar.IsLoading());
		int32 NumResults = -1;
		Ar << NumResults;
		check(NumResults >= 0);

		Results.SetNum(NumResults);
		for (int32 Index = 0; Index < NumResults; ++Index)
		{
			EVoxelSpawnerProxyType ProxyType = EVoxelSpawnerProxyType::Invalid;
			Ar << ProxyType;
			check(ProxyType != EVoxelSpawnerProxyType::Invalid);

			auto Result = CreateFromType(ProxyType, )
		}
	}
#endif
}

uint32 FVoxelSpawnerGroupProxyResult::GetAllocatedSize()
{
	return sizeof(*this) + Results.GetAllocatedSize();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSpawnerGroupProxy::FVoxelSpawnerGroupProxy(UVoxelSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager)
	: FVoxelSpawnerProxy(Spawner, Manager, EVoxelSpawnerProxyType::SpawnerGroup, 0)
	, SpawnerGroup(Spawner)
{

}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelSpawnerGroupProxy::ProcessHits(
	const FVoxelIntBox& Bounds,
	const TArray<FVoxelSpawnerHit>& Hits,
	const FVoxelConstDataAccelerator& Accelerator) const
{
	TArray<TUniquePtr<FVoxelSpawnerProxyResult>> Results;
	if (Children.Num() > 0)
	{
		TArray<TArray<FVoxelSpawnerHit>> ChildrenHits;
		ChildrenHits.SetNum(Children.Num());
		for (auto& ChildHits : ChildrenHits)
		{
			ChildHits.Reserve(Hits.Num());
		}

		const uint32 Seed = Bounds.GetMurmurHash() ^ SpawnerSeed;
		const FRandomStream Stream(Seed);

		for (auto& Hit : Hits)
		{
			ChildrenHits[GetChild(Stream.GetFraction())].Add(Hit);
		}

		for (int32 Index = 0; Index < Children.Num(); Index++)
		{
			if (ChildrenHits[Index].Num() == 0) continue;
			auto Result = Children[Index].Spawner->ProcessHits(Bounds, ChildrenHits[Index], Accelerator);
			if (Result.IsValid())
			{
				Results.Emplace(MoveTemp(Result));
			}
		}
	}
	if (Results.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		auto Result = MakeUnique<FVoxelSpawnerGroupProxyResult>(*this);
		Result->Init(MoveTemp(Results));
		return Result;
	}
}

void FVoxelSpawnerGroupProxy::PostSpawn()
{
	check(IsInGameThread());

	double ChildrenSum = 0;
	for (auto& Child : SpawnerGroup->Children)
	{
		ChildrenSum += Child.Probability;
	}
	if (ChildrenSum == 0)
	{
		ChildrenSum = 1;
	}

	double ProbabilitySum = 0;
	for (auto& Child : SpawnerGroup->Children)
	{
		const auto ChildSpawner = Manager.GetSpawner(Child.Spawner);
		if (!ChildSpawner.IsValid())
		{
			Children.Reset();
			return;
		}
		ProbabilitySum += Child.Probability / ChildrenSum;
		Children.Emplace(FChild{ ChildSpawner, float(ProbabilitySum) });
	}

	ensure(ChildrenSum == 1 || FMath::IsNearlyEqual(ProbabilitySum, 1));
}

int32 FVoxelSpawnerGroupProxy::GetChild(float RandomNumber) const
{
	for (int32 Index = 0; Index < Children.Num(); Index++)
	{
		if (RandomNumber < Children[Index].ProbabilitySum)
		{
			return Index;
		}
	}
	return Children.Num() - 1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelSpawnerGroup::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	return MakeVoxelShared<FVoxelSpawnerGroupProxy>(this, Manager);
}

bool UVoxelSpawnerGroup::GetSpawners(TSet<UVoxelSpawner*>& OutSpawners)
{
	static TArray<UVoxelSpawnerGroup*> Stack;

	struct FScopeStack
	{
		FScopeStack(UVoxelSpawnerGroup* This) : This(This)
		{
			Stack.Add(This);
		}
		~FScopeStack()
		{
			ensure(Stack.Pop() == This);
		}
		UVoxelSpawnerGroup* const This;
	};

	if (Stack.Contains(this))
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
		Message->AddToken(FTextToken::Create(VOXEL_LOCTEXT("Recursive spawner group! Spawners in stack: ")));
		for (auto* Spawner : Stack)
		{
			Message->AddToken(FUObjectToken::Create(Spawner));
		}
		FMessageLog("PIE").AddMessage(Message);
		return false;
	}

	FScopeStack ScopeStack(this);

	OutSpawners.Add(this);
	for (auto& Child : Children)
	{
		if (!Child.Spawner)
		{
			FVoxelMessages::Error("Invalid Child Spawner!", this);
			return false;
		}
		if (!Child.Spawner->GetSpawners(OutSpawners))
		{
			return false;
		}
	}

	return true;
}

FString UVoxelSpawnerGroup::GetDebugInfo() const
{
	FString Result;
	for (auto& Child : Children)
	{
		
		if (!Result.IsEmpty())
		{
			Result += ", ";
		}
		Result += "{ " + (Child.Spawner ? Child.Spawner->GetDebugInfo() : "NULL") + " }";
	}
	return Result;
}

#if WITH_EDITOR
void UVoxelSpawnerGroup::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (bNormalizeProbabilitiesOnEdit &&
		PropertyChangedEvent.Property &&
		(PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive ||
			PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet))
	{
		const int32 EditedIndex = PropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UVoxelSpawnerGroup, Children));
		if (Children.IsValidIndex(EditedIndex))
		{
			double Sum = 0;
			for (int32 Index = 0; Index < Children.Num(); Index++)
			{
				if (Index != EditedIndex)
				{
					Sum += Children[Index].Probability;
				}
			}
			if (Sum == 0)
			{
				for (int32 Index = 0; Index < Children.Num(); Index++)
				{
					if (Index != EditedIndex)
					{
						ensure(Children[Index].Probability == 0);
						Children[Index].Probability = (1 - Children[EditedIndex].Probability) / (Children.Num() - 1);
					}
				}
			}
			else
			{
				for (int32 Index = 0; Index < Children.Num(); Index++)
				{
					if (Index != EditedIndex)
					{
						Children[Index].Probability *= (1 - Children[EditedIndex].Probability) / Sum;
					}
				}
			}
		}
	}
}
#endif