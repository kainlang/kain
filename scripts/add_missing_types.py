#!/usr/bin/env python3
"""
Add missing UObject-derived types to engine_knowledge.json
Eliminates the need for hardcoded type lists in codegen_ue5.rs
"""

import json
from pathlib import Path

# Types that were hardcoded in codegen_ue5.rs
MISSING_TYPES = [
    # Materials
    {"name": "UMaterialInterface", "parent": "UObject", "header": "Materials/MaterialInterface.h", "module": "Engine", "prefix": "U"},
    {"name": "UMaterial", "parent": "UMaterialInterface", "header": "Materials/Material.h", "module": "Engine", "prefix": "U"},
    {"name": "UMaterialInstance", "parent": "UMaterialInterface", "header": "Materials/MaterialInstance.h", "module": "Engine", "prefix": "U"},
    {"name": "UMaterialInstanceDynamic", "parent": "UMaterialInstance", "header": "Materials/MaterialInstanceDynamic.h", "module": "Engine", "prefix": "U"},
    
    # Textures
    {"name": "UTexture", "parent": "UObject", "header": "Engine/Texture.h", "module": "Engine", "prefix": "U"},
    {"name": "UTexture2D", "parent": "UTexture", "header": "Engine/Texture2D.h", "module": "Engine", "prefix": "U"},
    {"name": "UTextureRenderTarget2D", "parent": "UTexture", "header": "Engine/TextureRenderTarget2D.h", "module": "Engine", "prefix": "U"},
    
    # Meshes
    {"name": "UStaticMesh", "parent": "UObject", "header": "Engine/StaticMesh.h", "module": "Engine", "prefix": "U"},
    {"name": "USkeletalMesh", "parent": "UObject", "header": "Engine/SkeletalMesh.h", "module": "Engine", "prefix": "U"},
    
    # Animation
    {"name": "UAnimInstance", "parent": "UObject", "header": "Animation/AnimInstance.h", "module": "Engine", "prefix": "U"},
    {"name": "UAnimSequence", "parent": "UObject", "header": "Animation/AnimSequence.h", "module": "Engine", "prefix": "U"},
    
    # Audio
    {"name": "USoundBase", "parent": "UObject", "header": "Sound/SoundBase.h", "module": "Engine", "prefix": "U"},
    {"name": "USoundWave", "parent": "USoundBase", "header": "Sound/SoundWave.h", "module": "Engine", "prefix": "U"},
    
    # Particles/VFX
    {"name": "UParticleSystem", "parent": "UObject", "header": "Particles/ParticleSystem.h", "module": "Engine", "prefix": "U"},
    {"name": "UNiagaraSystem", "parent": "UObject", "header": "NiagaraSystem.h", "module": "Niagara", "prefix": "U"},
    
    # Data
    {"name": "UDataTable", "parent": "UObject", "header": "Engine/DataTable.h", "module": "Engine", "prefix": "U"},
    {"name": "UCurveFloat", "parent": "UObject", "header": "Curves/CurveFloat.h", "module": "Engine", "prefix": "U"},
    {"name": "UCurveLinearColor", "parent": "UObject", "header": "Curves/CurveLinearColor.h", "module": "Engine", "prefix": "U"},
    
    # World
    {"name": "UWorld", "parent": "UObject", "header": "Engine/World.h", "module": "Engine", "prefix": "U"},
    {"name": "UGameInstance", "parent": "UObject", "header": "Engine/GameInstance.h", "module": "Engine", "prefix": "U"},
]

# Type aliases that were hardcoded in types.rs
TYPE_ALIASES = [
    {"kain_name": "Transform", "ue5_name": "FTransform", "header": "Math/Transform.h"},
    {"kain_name": "AnimMontage", "ue5_name": "UAnimMontage", "header": "Animation/AnimMontage.h"},
    {"kain_name": "StaticMesh", "ue5_name": "UStaticMesh", "header": "Engine/StaticMesh.h"},
    {"kain_name": "SkeletalMesh", "ue5_name": "USkeletalMesh", "header": "Engine/SkeletalMesh.h"},
    {"kain_name": "Texture2D", "ue5_name": "UTexture2D", "header": "Engine/Texture2D.h"},
    {"kain_name": "Material", "ue5_name": "UMaterial", "header": "Materials/Material.h"},
    {"kain_name": "MaterialInstance", "ue5_name": "UMaterialInstance", "header": "Materials/MaterialInstance.h"},
    {"kain_name": "MaterialInstanceDynamic", "ue5_name": "UMaterialInstanceDynamic", "header": "Materials/MaterialInstanceDynamic.h"},
    {"kain_name": "StaticMeshComponent", "ue5_name": "UStaticMeshComponent", "header": "Components/StaticMeshComponent.h"},
    {"kain_name": "SplineComponent", "ue5_name": "USplineComponent", "header": "Components/SplineComponent.h"},
    {"kain_name": "InstancedStaticMeshComponent", "ue5_name": "UInstancedStaticMeshComponent", "header": "Components/InstancedStaticMeshComponent.h"},
    {"kain_name": "SkeletalMeshComponent", "ue5_name": "USkeletalMeshComponent", "header": "Components/SkeletalMeshComponent.h"},
    {"kain_name": "AnimSequence", "ue5_name": "UAnimSequence", "header": "Animation/AnimSequence.h"},
    {"kain_name": "SoundBase", "ue5_name": "USoundBase", "header": "Sound/SoundBase.h"},
    {"kain_name": "ParticleSystem", "ue5_name": "UParticleSystem", "header": "Particles/ParticleSystem.h"},
    {"kain_name": "NiagaraSystem", "ue5_name": "UNiagaraSystem", "header": "NiagaraSystem.h"},
]

def main():
    json_path = Path("unreal/metadata/engine_knowledge.json")
    
    if not json_path.exists():
        print(f"❌ File not found: {json_path}")
        return
    
    # Load existing data
    with open(json_path, 'r') as f:
        data = json.load(f)
    
    # Get existing class names to avoid duplicates
    existing_classes = {cls['name'] for cls in data.get('classes', [])}
    existing_aliases = {alias['kain_name'] for alias in data.get('type_aliases', [])}
    
    # Add missing classes
    added_classes = 0
    for cls in MISSING_TYPES:
        if cls['name'] not in existing_classes:
            data['classes'].append({
                "name": cls['name'],
                "parent": cls['parent'],
                "header": cls['header'],
                "module": cls['module'],
                "prefix": cls['prefix'],
                "is_abstract": False,
                "functions": [],
                "properties": []
            })
            added_classes += 1
            print(f"✅ Added class: {cls['name']}")
    
    # Add missing type aliases
    added_aliases = 0
    for alias in TYPE_ALIASES:
        if alias['kain_name'] not in existing_aliases:
            data['type_aliases'].append(alias)
            added_aliases += 1
            print(f"✅ Added alias: {alias['kain_name']} -> {alias['ue5_name']}")
    
    # Write back
    with open(json_path, 'w') as f:
        json.dump(data, f, indent=4)
    
    print(f"\n🎉 Done! Added {added_classes} classes and {added_aliases} type aliases")
    print(f"📊 Total classes: {len(data['classes'])}")
    print(f"📊 Total type aliases: {len(data['type_aliases'])}")

if __name__ == '__main__':
    main()
