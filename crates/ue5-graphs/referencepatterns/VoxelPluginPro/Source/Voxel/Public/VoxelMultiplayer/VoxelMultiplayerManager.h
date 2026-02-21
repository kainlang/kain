// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelTickable.h"
#include "UObject/WeakObjectPtr.h"

class AVoxelWorld;
class FVoxelData;
class IVoxelLODManager;
class FVoxelDebugManager;
class IVoxelMultiplayerClient;
class IVoxelMultiplayerServer;

DECLARE_MULTICAST_DELEGATE(FVoxelMultiplayerManagerOnClientConnection);

struct FVoxelMultiplayerSettings
{
	const TVoxelSharedRef<FVoxelData> Data;
	const TVoxelSharedRef<FVoxelDebugManager> DebugManager;
	const TVoxelSharedRef<IVoxelLODManager> LODManager;
	const TWeakObjectPtr<const AVoxelWorld> VoxelWorld;
	const float MultiplayerSyncRate;
	
	FVoxelMultiplayerSettings(
		const AVoxelWorld* World,
		const TVoxelSharedRef<FVoxelData>& Data,
		const TVoxelSharedRef<FVoxelDebugManager>& DebugManager,
		const TVoxelSharedRef<IVoxelLODManager>& LODManager);
};

class VOXEL_API FVoxelMultiplayerManager : public FVoxelTickable, public TVoxelSharedFromThis<FVoxelMultiplayerManager>
{
public:
	const FVoxelMultiplayerSettings Settings;

	FVoxelMultiplayerManagerOnClientConnection OnClientConnection;

	static TVoxelSharedRef<FVoxelMultiplayerManager> Create(const FVoxelMultiplayerSettings& Settings);
	void Destroy();
	
	//~ Begin FVoxelTickable Interface
	virtual void Tick(float DeltaTime) override;
	//~ End FVoxelTickable Interface

private:
	explicit FVoxelMultiplayerManager(
		const FVoxelMultiplayerSettings& Settings,
		TVoxelSharedPtr<IVoxelMultiplayerServer> Server,
		TVoxelSharedPtr<IVoxelMultiplayerClient> Client);

	double LastSyncTime = 0;

	const TVoxelSharedPtr<IVoxelMultiplayerServer> Server;
	const TVoxelSharedPtr<IVoxelMultiplayerClient> Client;
	
	void ReceiveData() const;
	void SendData() const;
	void OnConnection();
};
