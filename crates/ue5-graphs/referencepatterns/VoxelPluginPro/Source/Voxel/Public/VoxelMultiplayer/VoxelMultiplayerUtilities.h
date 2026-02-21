// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMultiplayer/VoxelMultiplayerInterface.h"

struct FVoxelCompressedWorldSaveImpl;

namespace FVoxelMultiplayerUtilities
{
	constexpr uint8 SizeBytes = 4;
	constexpr uint8 NextLoadTypeByes = 1;
	constexpr uint8 MagicBytes = 8;
	constexpr uint8 HeaderBytes = SizeBytes + NextLoadTypeByes + MagicBytes;

	FORCEINLINE void CreateHeader(TArray<uint8>& Data, uint32 SizeToSend, EVoxelMultiplayerNextLoadType NextLoadType)
	{
		check(NextLoadType != EVoxelMultiplayerNextLoadType::Unknown);

		Data.SetNum(HeaderBytes);

		Data[0] = SizeToSend % 256;
		SizeToSend /= 256;
		Data[1] = SizeToSend % 256;
		SizeToSend /= 256;
		Data[2] = SizeToSend % 256;
		SizeToSend /= 256;
		Data[3] = SizeToSend % 256;

		Data[4] = uint8(NextLoadType);

		Data[5] = 0xD;
		Data[6] = 0xE;
		Data[7] = 0xA;
		Data[8] = 0xD;
		Data[9] = 0xB;
		Data[10] = 0xE;
		Data[11] = 0xE;
		Data[12] = 0xF;

		static_assert(HeaderBytes == 13, "");
	}

	FORCEINLINE bool LoadHeader(const TArray<uint8>& Data, uint32& ExpectedSize, EVoxelMultiplayerNextLoadType& NextLoadType)
	{
		check(Data.Num() == HeaderBytes);

		ExpectedSize = Data[0] + 256 * (Data[1] + 256 * (Data[2] + 256 * Data[3]));

		NextLoadType = EVoxelMultiplayerNextLoadType(Data[4]);

		bool bValid = true;
		bValid &= ensureAlways(Data[5] == 0xD);
		bValid &= ensureAlways(Data[6] == 0xE);
		bValid &= ensureAlways(Data[7] == 0xA);
		bValid &= ensureAlways(Data[8] == 0xD);
		bValid &= ensureAlways(Data[9] == 0xB);
		bValid &= ensureAlways(Data[10] == 0xE);
		bValid &= ensureAlways(Data[11] == 0xE);
		bValid &= ensureAlways(Data[12] == 0xF);
		static_assert(HeaderBytes == 13, "");

		return bValid;
	}

	VOXEL_API void ReadDiffs(const TArray<uint8>& Data, TArray<TVoxelChunkDiff<FVoxelValue>>& OutValueDiffs, TArray<TVoxelChunkDiff<FVoxelMaterial>>& OutMaterialDiffs);
	// Will append to Data
	VOXEL_API void WriteDiffs(TArray<uint8>& Data, const TArray<TVoxelChunkDiff<FVoxelValue>>& ValueDiffs, const TArray<TVoxelChunkDiff<FVoxelMaterial>>& MaterialDiffs);

	VOXEL_API void ReadSave(const TArray<uint8>& Data, FVoxelCompressedWorldSaveImpl& OutSave);
	// Will append to Data
	VOXEL_API void WriteSave(TArray<uint8>& Data, const FVoxelCompressedWorldSaveImpl& Save);
}
