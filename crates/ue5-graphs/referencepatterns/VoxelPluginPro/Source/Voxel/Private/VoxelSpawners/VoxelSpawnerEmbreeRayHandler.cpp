// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSpawners/VoxelSpawnerEmbreeRayHandler.h"
#include "VoxelRender/VoxelChunkMesh.h"
#include "VoxelUtilities/VoxelIntVectorUtilities.h"

#if USE_EMBREE_VOXEL

#define CHECK_EMBREE_ERRORS() \
if (EmbreeDevice) \
{ \
	RTCError ReturnError = rtcGetDeviceError(EmbreeDevice); \
	if (!ensureAlwaysMsgf(ReturnError == RTC_ERROR_NONE, TEXT("Embree error!"))) \
	{ \
		rtcReleaseScene(EmbreeScene); \
		rtcReleaseDevice(EmbreeDevice); \
		EmbreeScene = nullptr; \
		EmbreeDevice = nullptr; \
		return; \
	} \
}

FVoxelSpawnerEmbreeRayHandler::FVoxelSpawnerEmbreeRayHandler(
	bool bStoreDebugRays,
	TArray<uint32>&& InIndices,
	TArray<FVector>&& InVertices)
	: FVoxelSpawnerRayHandler(bStoreDebugRays)
	, Indices(MoveTemp(InIndices))
	, Vertices(MoveTemp(InVertices))
{
	VOXEL_ASYNC_FUNCTION_COUNTER();
	
	// Embree needs 16 bytes of padding, just to be safe we allocate 32
	Indices.Reserve(Indices.Num() + FVoxelUtilities::DivideCeil(32, sizeof(uint32)));
	Vertices.Reserve(Vertices.Num() + FVoxelUtilities::DivideCeil(32, sizeof(FVector3f)));

	EmbreeDevice = rtcNewDevice(nullptr);
	CHECK_EMBREE_ERRORS();

	EmbreeScene = rtcNewScene(EmbreeDevice);
	CHECK_EMBREE_ERRORS();
    rtcSetSceneBuildQuality(EmbreeScene, RTC_BUILD_QUALITY_HIGH);
	CHECK_EMBREE_ERRORS();

	Geometry = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	CHECK_EMBREE_ERRORS();
    rtcSetGeometryBuildQuality(Geometry, RTC_BUILD_QUALITY_HIGH);
	CHECK_EMBREE_ERRORS();
	rtcSetSharedGeometryBuffer(Geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, Vertices.GetData(), 0, sizeof(FVector3f), Vertices.Num());
	CHECK_EMBREE_ERRORS();
    rtcSetSharedGeometryBuffer(Geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, Indices.GetData(), 0, 3 * sizeof(uint32), Indices.Num() / 3);
	CHECK_EMBREE_ERRORS();
    rtcCommitGeometry(Geometry);
	CHECK_EMBREE_ERRORS();
	rtcAttachGeometryByID(EmbreeScene, Geometry, 0);
	CHECK_EMBREE_ERRORS();

	rtcCommitScene(EmbreeScene);
	CHECK_EMBREE_ERRORS();
}

FVoxelSpawnerEmbreeRayHandler::~FVoxelSpawnerEmbreeRayHandler()
{
	VOXEL_ASYNC_FUNCTION_COUNTER();
	
	rtcReleaseGeometry(Geometry);
	Geometry = nullptr;
	CHECK_EMBREE_ERRORS();
	rtcReleaseScene(EmbreeScene);
	EmbreeScene = nullptr;
	CHECK_EMBREE_ERRORS();
	rtcReleaseDevice(EmbreeDevice);
	EmbreeDevice = nullptr;
	CHECK_EMBREE_ERRORS();
}

bool FVoxelSpawnerEmbreeRayHandler::TraceRayInternal(const FVector& Start, const FVector& Direction, FVector& HitNormal, FVector& HitPosition) const
{
	RTCRayHit RayHit;
	RTCRay& Ray = RayHit.ray;
	Ray.org_x = Start.X;
	Ray.org_y = Start.Y;
	Ray.org_z = Start.Z;
	Ray.dir_x = Direction.X;
	Ray.dir_y = Direction.Y;
	Ray.dir_z = Direction.Z;
	Ray.tnear = 0;
	Ray.tfar = 1e9;
	Ray.flags = 0;
	Ray.mask = -1;

	RTCHit& Hit = RayHit.hit;
	Hit.geomID = RTC_INVALID_GEOMETRY_ID;
	Hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

	RTCIntersectContext Context;
	rtcInitIntersectContext(&Context);

	rtcIntersect1(EmbreeScene, &Context, &RayHit);
	
#if 0
	RTCError ReturnError = rtcGetDeviceError(EmbreeDevice);
	ensureAlwaysMsgf(ReturnError == RTC_ERROR_NONE, TEXT("Embree error!"));
#endif

	if (Hit.geomID != RTC_INVALID_GEOMETRY_ID)
	{
		HitNormal = -FVector(Hit.Ng_x, Hit.Ng_y, Hit.Ng_z).GetSafeNormal();
		HitPosition = Start + Direction * Ray.tfar;
		return true;
	}

	return false;
}

#endif // USE_EMBREE_VOXEL
