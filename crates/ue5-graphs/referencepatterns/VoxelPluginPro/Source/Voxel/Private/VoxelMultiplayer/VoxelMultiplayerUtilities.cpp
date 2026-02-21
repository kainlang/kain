// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMultiplayer/VoxelMultiplayerUtilities.h"
#include "VoxelData/VoxelSave.h"
#include "VoxelUtilities/VoxelSerializationUtilities.h"
#include "VoxelMinimal.h"
#include "VoxelValue.h"
#include "VoxelMaterial.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"

inline void SerializeDataHeader(FArchive& Archive, bool& bValues, uint32& ItemCount)
{
	Archive << bValues << ItemCount;
}

void FVoxelMultiplayerUtilities::ReadDiffs(const TArray<uint8>& Data, TArray<TVoxelChunkDiff<FVoxelValue>>& OutValueDiffs, TArray<TVoxelChunkDiff<FVoxelMaterial>>& OutMaterialDiffs)
{
	VOXEL_FUNCTION_COUNTER();

	check(Data.Num() > 0);

	TArray64<uint8> UncompressedData;
	if (!ensure(FVoxelSerializationUtilities::DecompressData(Data, UncompressedData)))
	{
		return;
	}
	check(UncompressedData.Num() >= sizeof(bool) + sizeof(uint32));

	FLargeMemoryReader Reader(UncompressedData.GetData(), UncompressedData.Num());

	bool bValues;
	uint32 ItemCount;
	SerializeDataHeader(Reader, bValues, ItemCount);

	if (bValues)
	{
		for (uint32 Index = 0; Index < ItemCount; Index++)
		{
			TVoxelChunkDiff<FVoxelValue> Diff;
			Reader << Diff;
			OutValueDiffs.Add(Diff);
		}
	}
	else
	{
		for (uint32 Index = 0; Index < ItemCount; Index++)
		{
			TVoxelChunkDiff<FVoxelMaterial> Diff;
			Reader << Diff;
			OutMaterialDiffs.Add(Diff);
		}
	}

	ensure(Reader.AtEnd() && !Reader.IsError());
}

template<typename T>
void WriteDiffsImpl(TArray<uint8>& Data, const TArray<TVoxelChunkDiff<T>>& Diffs)
{
	VOXEL_FUNCTION_COUNTER();
	
	FLargeMemoryWriter Writer;

	bool bValue = std::is_same_v<T, FVoxelValue>;
	uint32 SizeToSend = Diffs.Num();
	
	check((bValue || std::is_same_v<T, FVoxelMaterial>));
	
	SerializeDataHeader(Writer, bValue, SizeToSend);

	for (uint32 Index = 0; Index < SizeToSend; Index++)
	{
		Writer << const_cast<TVoxelChunkDiff<T>&>(Diffs[Index]);
	}
	
	TArray<uint8> CompressedData;
	// NOTE: Change the compression level here
	FVoxelSerializationUtilities::CompressData(Writer, CompressedData, EVoxelCompressionLevel::BestSpeed);
	Data.Append(CompressedData);
}

void FVoxelMultiplayerUtilities::WriteDiffs(TArray<uint8>& Data, const TArray<TVoxelChunkDiff<FVoxelValue>>& ValueDiffs, const TArray<TVoxelChunkDiff<FVoxelMaterial>>& MaterialDiffs)
{
	VOXEL_FUNCTION_COUNTER();

	if (ValueDiffs.Num() > 0)
	{
		WriteDiffsImpl(Data, ValueDiffs);
	}
	if (MaterialDiffs.Num() > 0)
	{
		WriteDiffsImpl(Data, MaterialDiffs);
	}
}

void FVoxelMultiplayerUtilities::ReadSave(const TArray<uint8>& Data, FVoxelCompressedWorldSaveImpl& OutSave)
{
	VOXEL_FUNCTION_COUNTER();
	
	FMemoryReader Reader(Data);
	OutSave.Serialize(Reader);
}

void FVoxelMultiplayerUtilities::WriteSave(TArray<uint8>& Data, const FVoxelCompressedWorldSaveImpl& Save)
{
	VOXEL_FUNCTION_COUNTER();
	
	FMemoryWriter Writer(Data);
	const_cast<FVoxelCompressedWorldSaveImpl&>(Save).Serialize(Writer);
}

