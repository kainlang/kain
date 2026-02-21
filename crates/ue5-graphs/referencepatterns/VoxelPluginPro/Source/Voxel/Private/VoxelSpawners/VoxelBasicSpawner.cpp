// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelBasicSpawner.h"
#include "VoxelUtilities/VoxelMathUtilities.h"

FVoxelBasicSpawnerProxy::FVoxelBasicSpawnerProxy(UVoxelBasicSpawner* Spawner, FVoxelSpawnerManager& Manager, EVoxelSpawnerProxyType Type, uint32 Seed)
	: FVoxelSpawnerProxy(Spawner, Manager, Type, Seed)
	, GroundSlopeAngle(Spawner->GroundSlopeAngle)
	, bEnableHeightRestriction(Spawner->bEnableHeightRestriction)
	, HeightRestriction(Spawner->HeightRestriction)
	, HeightRestrictionFalloff(Spawner->HeightRestrictionFalloff)
	, Scaling(Spawner->Scaling)
	, RotationAlignment(Spawner->RotationAlignment)
	, bRandomYaw(Spawner->bRandomYaw)
	, RandomPitchAngle(Spawner->RandomPitchAngle)
	, LocalPositionOffset(Spawner->LocalPositionOffset)
	, LocalRotationOffset(Spawner->LocalRotationOffset)
	, GlobalPositionOffset(Spawner->GlobalPositionOffset)
{
}

bool FVoxelBasicSpawnerProxy::CanSpawn(
	const FRandomStream& RandomStream,
	const FVoxelVector& Position,
	const FVector& Normal,
	const FVector& WorldUp) const
{
	if (bEnableHeightRestriction)
	{
		if (!HeightRestriction.Contains(Position.Z))
		{
			return false;
		}

		const float Center = (HeightRestriction.Min + HeightRestriction.Max) / 2.f;
		const float Radius = HeightRestriction.Size() / 2.f;
		const float Distance = FMath::Abs(Center - Position.Z);
		const float Falloff = FMath::Min(HeightRestrictionFalloff, Radius);

		const float Alpha = FVoxelUtilities::SmoothFalloff(Distance, Radius - Falloff, Falloff);

		if (RandomStream.GetFraction() >= Alpha)
		{
			return false;
		}
	}

	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Normal, WorldUp)));
	if (!GroundSlopeAngle.Contains(Angle))
	{
		return false;
	}

	return true;
}

FMatrix FVoxelBasicSpawnerProxy::GetTransform(
	const FRandomStream& Stream,
	const FVector& Normal,
	const FVector& WorldUp,
	const FVector& Position) const
{
	FMatrix Matrix = FRotationTranslationMatrix(LocalRotationOffset, LocalPositionOffset);
	
	const FVector Scale = Scaling.GetScale(Stream);

	const float Yaw = bRandomYaw ? Stream.FRandRange(0, 360.0f) : 0.0f;
	const float Pitch = Stream.FRandRange(0, RandomPitchAngle);;
	Matrix *= FScaleRotationTranslationMatrix(Scale, FRotator(Pitch, Yaw, 0.0f), FVector::ZeroVector);

	switch (RotationAlignment)
	{
	case EVoxelBasicSpawnerRotation::AlignToSurface:
		Matrix *= FRotationMatrix::MakeFromZ(Normal);
		break;
	case EVoxelBasicSpawnerRotation::AlignToWorldUp:
		Matrix *= FRotationMatrix::MakeFromZ(WorldUp);
		break;
	case EVoxelBasicSpawnerRotation::RandomAlign:
		Matrix *= FRotationMatrix::MakeFromZ(Stream.GetUnitVector());
		break;
	default:
		check(false);
	}

	Matrix *= FTranslationMatrix(Position + GlobalPositionOffset);

	return Matrix;
}
