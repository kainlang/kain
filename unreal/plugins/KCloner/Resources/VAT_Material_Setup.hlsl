// K-Cloner VAT (Vertex Animation Texture) Material Function
// This is a placeholder HLSL file to document the required material setup.
// Create the actual material in Unreal Editor using the Material Editor.

/*
=============================================================================
K-CLONER VAT MATERIAL SETUP GUIDE
=============================================================================

1. CREATE BASE VAT MATERIAL
   - Right-click in Content Browser → Material
   - Name it: M_KCloner_VAT_Base

2. REQUIRED TEXTURE PARAMETERS
   - VATPositionTexture (Texture2D): Position offset per vertex per frame
   - VATRotationTexture (Texture2D): Rotation (optional, for complex animations)

3. REQUIRED SCALAR/VECTOR PARAMETERS
   - VATFrameCount (Scalar): Total frames in animation
   - VATMaxBounds (Vector3): Bounding box extent for position decode

4. WORLD POSITION OFFSET NODE SETUP
   
   // Sample position texture using vertex UV and time
   float2 UV;
   UV.x = VertexUV.x;                    // Mesh vertex index (packed in UV)
   UV.y = frac(Time / VATFrameCount);    // Animation frame (0-1)
   
   float3 PositionOffset = VATPositionTexture.Sample(UV).rgb;
   PositionOffset = PositionOffset * 2.0 - 1.0;  // Decode from 0-1 to -1 to 1
   PositionOffset *= VATMaxBounds;                // Scale to world units
   
   WorldPositionOffset = PositionOffset;

5. CUSTOM DATA ACCESS (Per-Instance)
   - CustomData0 = Instance Time (use for animation frame)
   - CustomData1 = Instance Index
   - CustomData2 = Total Clone Count
   - CustomData3 = Reserved

6. HOW TO USE IN K-CLONER
   - Bake your skeletal animation to VAT using Bake → VAT
   - Assign the generated textures to the K-Cloner's VATPositionTexture/VATRotationTexture
   - Assign M_KCloner_VAT_Base to VATBaseMaterial
   - Set SkeletalMode to VATBaked or Auto

=============================================================================
EXAMPLE MATERIAL GRAPH (Pseudo-code)
=============================================================================

[PerInstanceCustomData0]  // Time
        |
        v
[Divide] <---- [VATFrameCount Parameter]
        |
        v
[Frac] // Wrap animation
        |
        v
[AppendVector] <---- [TexCoord.x] (Vertex ID)
        |
        v
[TextureSampleParameter2D: VATPositionTexture]
        |
        v
[Subtract 0.5] --> [Multiply 2.0] // Decode from 0-1 to -1 to 1
        |
        v
[Multiply] <---- [VATMaxBounds Parameter]
        |
        v
[World Position Offset Output]

=============================================================================
*/
