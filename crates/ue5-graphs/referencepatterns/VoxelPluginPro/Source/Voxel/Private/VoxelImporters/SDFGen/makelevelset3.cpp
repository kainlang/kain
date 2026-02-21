// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "makelevelset3.h"
#include "VoxelImporters/VoxelMeshImporter.h"
#include "VoxelUtilities/VoxelIntVectorUtilities.h"

// 10% faster to build with FORCEINLINE
FORCEINLINE float PointSegmentDistance(const FVector& Point, const FVector& A, const FVector& B, float& Alpha)
{
	const FVector AB = B - A;
	const float Size = AB.SizeSquared();
	// Find parameter value of closest point on segment
	Alpha = FVector::DotProduct(B - Point, AB) / Size;
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	// And find the distance
	return FVector::Dist(Point, FMath::Lerp(B, A, Alpha));
}

// 10-20% better perf with this FORCEINLINE
FORCEINLINE float PointTriangleDistance(
	const FVector& Point,
	const FVector& A,
	const FVector& B,
	const FVector& C,
	float& AlphaA, float& AlphaB, float& AlphaC)
{
	// First find barycentric coordinates of closest point on infinite plane
	{
		const FVector CA = A - C;
		const FVector CB = B - C;
		const FVector CPoint = Point - C;
		const float SizeCA = CA.SizeSquared();
		const float SizeCB = CB.SizeSquared();
		const float d = FVector::DotProduct(CA, CB);
		const float InvDet = 1.f / FMath::Max(SizeCA * SizeCB - d * d, SMALL_NUMBER);
		const float a = FVector::DotProduct(CA, CPoint);
		const float b = FVector::DotProduct(CB, CPoint);

		// The barycentric coordinates themselves
		AlphaA = InvDet * (SizeCB * a - d * b);
		AlphaB = InvDet * (SizeCA * b - d * a);
		AlphaC = 1 - AlphaA - AlphaB;
	}

	if (AlphaA >= 0 && AlphaB >= 0 && AlphaC >= 0)
	{
		// If we're inside the triangle
		return FVector::Dist(Point, AlphaA * A + AlphaB * B + AlphaC * C);
	}
	else
	{
		// We have to clamp to one of the edges

		if (AlphaA > 0)
		{
			// This rules out edge BC for us
			float AlphaAB;
			const float DistanceAB = PointSegmentDistance(Point, A, B, AlphaAB);
			float AlphaAC;
			const float DistanceAC = PointSegmentDistance(Point, A, C, AlphaAC);

			if (DistanceAB < DistanceAC)
			{
				AlphaA = AlphaAB;
				AlphaB = 1 - AlphaAB;
				AlphaC = 0;
				return DistanceAB;
			}
			else
			{
				AlphaA = AlphaAC;
				AlphaB = 0;
				AlphaC = 1 - AlphaAC;
				return DistanceAC;
			}
		}
		else if (AlphaB > 0)
		{
			// This rules out edge AC
			float AlphaAB;
			const float DistanceAB = PointSegmentDistance(Point, A, B, AlphaAB);
			float AlphaBC;
			const float DistanceBC = PointSegmentDistance(Point, B, C, AlphaBC);

			if (DistanceAB < DistanceBC)
			{
				AlphaA = AlphaAB;
				AlphaB = 1 - AlphaAB;
				AlphaC = 0;
				return DistanceAB;
			}
			else
			{
				AlphaA = 0;
				AlphaB = AlphaBC;
				AlphaC = 1 - AlphaBC;
				return DistanceBC;
			}
		}
		else
		{
			ensureVoxelSlowNoSideEffects(AlphaC > 0);
			// Rules out edge AB

			float AlphaBC;
			const float DistanceBC = PointSegmentDistance(Point, B, C, AlphaBC);
			float AlphaAC;
			const float DistanceAC = PointSegmentDistance(Point, A, C, AlphaAC);

			if (DistanceBC < DistanceAC)
			{
				AlphaA = 0;
				AlphaB = AlphaBC;
				AlphaC = 1 - AlphaBC;
				return DistanceBC;
			}
			else
			{
				AlphaA = AlphaAC;
				AlphaB = 0;
				AlphaC = 1 - AlphaAC;
				return DistanceAC;
			}
		}
	}
}

struct FVector2D_Double
{
	double X;
	double Y;

	FVector2D_Double() = default;
	FVector2D_Double(double X, double Y)
		: X(X)
		, Y(Y)
	{
	}

	FORCEINLINE FVector2D_Double operator*(const FVector2D_Double& Other) const
	{
		return { X * Other.X, Y * Other.Y };
	}
	FORCEINLINE FVector2D_Double operator+(const FVector2D_Double& Other) const
	{
		return { X + Other.X, Y + Other.Y };
	}
	FORCEINLINE FVector2D_Double operator-(const FVector2D_Double& Other) const
	{
		return { X - Other.X, Y - Other.Y };
	}
	FORCEINLINE FVector2D_Double& operator-=(const FVector2D_Double& Other)
	{
		return *this = *this - Other;
	}
};

// Calculate twice signed area of triangle (0,0)-(A.X,A.Y)-(B.X,B.Y)
// Return an SOS-determined sign (-1, +1, or 0 only if it's a truly degenerate triangle)
inline int32 Orientation(
	const FVector2D_Double& A, 
	const FVector2D_Double& B, 
	double& TwiceSignedArea)
{
	TwiceSignedArea = A.Y * B.X - A.X * B.Y;
	if (TwiceSignedArea > 0) return 1;
	else if (TwiceSignedArea < 0) return -1;
	else if (B.Y > A.Y) return 1;
	else if (B.Y < A.Y) return -1;
	else if (A.X > B.X) return 1;
	else if (A.X < B.X) return -1;
	else return 0; // Only true when A.X == B.X and A.Y == B.Y
}

// Robust test of (x0,y0) in the triangle (x1,y1)-(x2,y2)-(x3,y3)
// If true is returned, the barycentric coordinates are set in a,b,c.
inline bool PointInTriangle2D(
	const FVector2D_Double& Point,
	FVector2D_Double A,
	FVector2D_Double B,
	FVector2D_Double C,
	double& AlphaA, double& AlphaB, double& AlphaC)
{
	A -= Point;
	B -= Point;
	C -= Point;

	const int32 SignA = Orientation(B, C, AlphaA);
	if (SignA == 0) return false;

	const int32 SignB = Orientation(C, A, AlphaB);
	if (SignB != SignA) return false;

	const int32 SignC = Orientation(A, B, AlphaC);
	if (SignC != SignA) return false;

	const double Sum = AlphaA + AlphaB + AlphaC;
	checkSlow(Sum != 0); // if the SOS signs match and are non-zero, there's no way all of a, b, and c are zero.
	AlphaA /= Sum;
	AlphaB /= Sum;
	AlphaC /= Sum;

	return true;
}

void MakeLevelSet3(
	const FVoxelMeshImporterSettingsBase& Settings,
	const FMakeLevelSet3InData& InData,
	FMakeLevelSet3OutData& OutData)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();

	check(!InData.bExportUVs || InData.UVs.Num() == InData.Vertices.Num());
	check(InData.ColorTextures.Num() == 0 || InData.UVs.Num() == InData.Vertices.Num());

	const FIntVector Size = InData.Size;
	
	OutData.Phi.Resize(Size);
	OutData.Phi.Assign(MAX_flt);

	if (InData.bExportUVs)
	{
		OutData.UVs.Resize(Size);
		OutData.UVs.Memzero();
	}
	
	if (InData.bExportPositions)
	{
		OutData.Positions.Resize(Size);
		OutData.Positions.Assign(FVector3f(MAX_flt));
	}
	
	OutData.Colors.SetNum(InData.ColorTextures.Num());
	for (int32 Index = 0; Index < InData.ColorTextures.Num(); Index++)
	{
		OutData.Colors[Index].Resize(Size);
		OutData.Colors[Index].Memzero();
	}
	
	TVoxelArray3<bool> Computed(Size);
	Computed.Memzero();
	
	TVoxelArray3<int32> ClosestTriangleIndices(Size);
	ClosestTriangleIndices.Assign(-1);

	TVoxelArray3<int32> IntersectionCount(Size); // IntersectionCount(i,j,k) is # of tri intersections in (i-1,i]x{j}x{k}
	IntersectionCount.Assign(0);

	// Find the axis mappings based on the sweep direction
	int32 IndexI;
	int32 IndexJ;
	int32 IndexK;
	switch (Settings.SweepDirection)
	{
	default: ensure(false);
	case EVoxelAxis::X: IndexI = 1; IndexJ = 2; IndexK = 0; break;
	case EVoxelAxis::Y: IndexI = 2; IndexJ = 0; IndexK = 1; break;
	case EVoxelAxis::Z: IndexI = 0; IndexJ = 1; IndexK = 2; break;
	}
	
	// We begin by initializing distances near the mesh, and figuring out intersection counts
	{
		VOXEL_ASYNC_SCOPE_COUNTER("Intersections");
		for (int32 TriangleIndex = 0; TriangleIndex < InData.Triangles.Num(); TriangleIndex++)
		{
			VOXEL_SLOW_SCOPE_COUNTER("Process triangle");

			const FIntVector& Triangle = InData.Triangles[TriangleIndex];
			const int32 IndexA = Triangle.X;
			const int32 IndexB = Triangle.Y;
			const int32 IndexC = Triangle.Z;

			const FVector& VertexA = InData.Vertices[IndexA];
			const FVector& VertexB = InData.Vertices[IndexB];
			const FVector& VertexC = InData.Vertices[IndexC];

			const auto ToVoxelSpace = [&](const FVector& Value)
			{
				return (Value - InData.Origin) / Settings.VoxelSize;
			};
			const auto FromVoxelSpace = [&](const FVector& Value)
			{
				return Value * Settings.VoxelSize + InData.Origin;
			};

			const FVector VoxelVertexA = ToVoxelSpace(VertexA);
			const FVector VoxelVertexB = ToVoxelSpace(VertexB);
			const FVector VoxelVertexC = ToVoxelSpace(VertexC);

			const FVector MinVoxelVertex = FVoxelUtilities::ComponentMin3(VoxelVertexA, VoxelVertexB, VoxelVertexC);
			const FVector MaxVoxelVertex = FVoxelUtilities::ComponentMax3(VoxelVertexA, VoxelVertexB, VoxelVertexC);

			{
				VOXEL_SLOW_SCOPE_COUNTER("Compute distance");

				const FIntVector Start = FVoxelUtilities::Clamp(FVoxelUtilities::FloorToInt(MinVoxelVertex) - Settings.ExactBand, FIntVector(0), Size - 1);
				const FIntVector End = FVoxelUtilities::Clamp(FVoxelUtilities::CeilToInt(MaxVoxelVertex) + Settings.ExactBand, FIntVector(0), Size - 1);

				// Do distances nearby
				for (int32 Z = Start.Z; Z <= End.Z; Z++)
				{
					for (int32 Y = Start.Y; Y <= End.Y; Y++)
					{
						for (int32 X = Start.X; X <= End.X; X++)
						{
							const FVector Position = FromVoxelSpace(FVector(X, Y, Z));
							float AlphaA, AlphaB, AlphaC;
							const float Distance = PointTriangleDistance(Position, VertexA, VertexB, VertexC, AlphaA, AlphaB, AlphaC);
							if (Distance < OutData.Phi(X, Y, Z))
							{
								Computed(X, Y, Z) = true;
								OutData.Phi(X, Y, Z) = Distance;
								ClosestTriangleIndices(X, Y, Z) = TriangleIndex;
								
								if (InData.bExportUVs || InData.ColorTextures.Num() > 0)
								{
									const FVector2D UV = AlphaA * InData.UVs[IndexA] + AlphaB * InData.UVs[IndexB] + AlphaC * InData.UVs[IndexC];
									if (InData.bExportUVs)
									{
										OutData.UVs(X, Y, Z) = UV;
									}
									for (int32 Index = 0; Index < InData.ColorTextures.Num(); Index++)
									{
										const auto& ColorTexture = InData.ColorTextures[Index];
										OutData.Colors[Index](X, Y, Z) = ColorTexture.Sample<FLinearColor>(
											float(UV.X * ColorTexture.GetSizeX()),
											float(UV.Y * ColorTexture.GetSizeY()),
											EVoxelSamplerMode::Tile).ToFColor(true);
									}
								}
								
								if (InData.bExportPositions)
								{
									OutData.Positions(X, Y, Z) = FVector3f(AlphaA * VoxelVertexA + AlphaB * VoxelVertexB + AlphaC * VoxelVertexC);
								}
							}
						}
					}
				}
			}

			{
				VOXEL_SLOW_SCOPE_COUNTER("Compute intersections");
				
				const FIntVector Start = FVoxelUtilities::Clamp(FVoxelUtilities::CeilToInt(MinVoxelVertex), FIntVector(0), Size - 1);
				const FIntVector End = FVoxelUtilities::Clamp(FVoxelUtilities::FloorToInt(MaxVoxelVertex), FIntVector(0), Size - 1);

				// Do intersection counts. Make sure to follow SweepDirection!
				FIntVector Position;
				for (int32 I = Start[IndexI]; I <= End[IndexI]; I++)
				{
					Position[IndexI] = I;
					for (int32 J = Start[IndexJ]; J <= End[IndexJ]; J++)
					{
						Position[IndexJ] = J;
						
						const auto Get2D = [&](const FVector& V) { return FVector2D_Double(V[IndexI], V[IndexJ]); };
						
						double AlphaA, AlphaB, AlphaC;
						if (PointInTriangle2D(FVector2D_Double(I, J), Get2D(VoxelVertexA), Get2D(VoxelVertexB), Get2D(VoxelVertexC), AlphaA, AlphaB, AlphaC))
						{
							const float K = AlphaA * VoxelVertexA[IndexK] + AlphaB * VoxelVertexB[IndexK] + AlphaC * VoxelVertexC[IndexK]; // Intersection K coordinate
#if 1
							Position[IndexK] = FMath::Clamp(Settings.bReverseSweep ? FMath::FloorToInt(K) : FMath::CeilToInt(K), 0, Size[IndexK] - 1);
							IntersectionCount(Position)++;
#else
							const int32 IntervalX = FMath::CeilToInt(X); // Intersection is in (IntervalX - 1, IntervalX]
							if (IntervalX < 0)
							{
								IntersectionCount(FMath::Clamp(FMath::CeilToInt(X), 0, Size.X - 1), Y, Z)++; // We enlarge the first interval to include everything to the -X direction
							}
							else if (IntervalX < Size.X)
							{
								IntersectionCount(IntervalX, Y, Z)++; // We ignore intersections that are beyond the +X side of the grid
							}
#endif
						}
					}
				}
			}
		}
	}

	{
		VOXEL_ASYNC_SCOPE_COUNTER("Compute Signs");
		
		// Then figure out signs (inside/outside) from intersection counts
		FIntVector Position;
		for (int32 I = 0; I < Size[IndexI]; I++)
		{
			Position[IndexI] = I;
			for (int32 J = 0; J < Size[IndexJ]; J++)
			{
				Position[IndexJ] = J;

				// Compute the number of intersections, and skip it if it's a leak
				if (Settings.bHideLeaks)
				{
					int32 Count = 0;
					for (int32 K = 0; K < Size[IndexK]; K++)
					{
						Position[IndexK] = K;
						Count += IntersectionCount(Position);
					}

					if (Settings.bWatertight)
					{
						if (Count % 2 == 1)
						{
							// For watertight meshes, we're expecting to come in and out of the mesh
							OutData.NumLeaks++;
							continue;
						}
					}
					else
					{
						if (Count == 0)
						{
							// For other meshes, only skip when there was no hit
							OutData.NumLeaks++;
							continue;
						}
					}
				}
				// If we are not watertight, start inside (unless we're reverse)
				int32 Count = (!Settings.bWatertight && !Settings.bReverseSweep) ? 1 : 0;
				
				FVector2D LastUV = FVector2D(ForceInit);
				
				TArray<FColor, TInlineAllocator<8>> LastColors;
				LastColors.SetNum(InData.ColorTextures.Num());

				for (int32 K = 0; K < Size[IndexK]; K++)
				{
					Position[IndexK] = Settings.bReverseSweep ? Size[IndexK] - 1 - K : K;
					
					Count += IntersectionCount(Position);
					if (Count % 2 == 1)
					{
						// If parity of intersections so far is odd, we are inside the mesh
						OutData.Phi(Position) *= -1;
					}

					// Only propagate values that are actually valid
					const bool bComputed = Computed(Position);
					if (InData.bExportUVs)
					{
						if (!bComputed)
						{
							OutData.UVs(Position) = LastUV;
						}
						else
						{
							LastUV = OutData.UVs(Position);
						}
					}
					for (int32 Index = 0; Index < InData.ColorTextures.Num(); Index++)
					{
						if (!bComputed)
						{
							OutData.Colors[Index](Position) = LastColors[Index];
						}
						else
						{
							LastColors[Index] = OutData.Colors[Index](Position);
						}
					}
				}
			}
		}
	}

	{
		VOXEL_ASYNC_SCOPE_COUNTER("Divide distances");
		for (float& Distance : OutData.Phi.Data)
		{
			Distance /= Settings.VoxelSize;
			Distance /= Settings.DistanceDivisor;
		}
	}
}
