// © 2025 mrbaconvn. All Rights Reserved.


#include "DamageShape/DamageNotifyData.h"
#include "DamageShape/DamageShape.h"

FString UDamageNotifyData::GetNotifyName() const
{
	return TEXT("Damage Notify");
}

void UDamageNotifyData::DebugNotifyData(const AActor* DebugActor, EAnimNotifyState NotifyState) const
{
	Super::DebugNotifyData(DebugActor, NotifyState);

	FDamageShapeQuery TraceQuery;
	TraceQuery.SourceActor = DebugActor;
	TraceQuery.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TraceQuery.bDrawDebug = true;
	
	for(const TInstancedStruct<FDamageShape>& DamageShape : DamageShapes)
	{
		if(!DamageShape.IsValid())
		{
			UE_LOG (LogTemp, Warning, TEXT("UDamageNotifyData::DebugNotifyData - DamageShape is not valid"));
			return;
		}
		
		TArray<FHitResult> OutHit;
		DamageShape.Get().TraceShape(TraceQuery, OutHit);
	}
}

TArray<FHitResult> UDamageNotifyData::TraceHit(const AActor* Source,
	const TArray<ECollisionChannel>& CollisionChannels) const
{
	TArray<FHitResult> HitResults;

	FDamageShapeQuery TraceQuery;
	TraceQuery.SourceActor = Source; 
	for (ECollisionChannel HitChannel : CollisionChannels)
	{
		TraceQuery.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(HitChannel));
	}
	TraceQuery.bDrawDebug = CVarShowDebugDamageShape.GetValueOnGameThread();

	for(const TInstancedStruct<FDamageShape>& DamageShape : DamageShapes)
	{
		if(!DamageShape.IsValid())
		{
			UE_LOG (LogTemp, Warning, TEXT("UDamageNotifyData::TraceHit - DamageShape is not valid"));
			return HitResults;
		} 
		
		TArray<FHitResult> OutHit;
		DamageShape.Get().TraceShape(TraceQuery, OutHit);
		
		HitResults.Append(OutHit);
	}
	
	return HitResults;
} 