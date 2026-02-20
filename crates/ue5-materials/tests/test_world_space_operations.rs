// Tests for Phase 7.4: World-Space Operations

use ue5_materials::material_graph::*;
use ue5_materials::material_serializer::MaterialAssetBuilder;
use ue5_asset_utils::KainEngineTarget;

#[test]
fn test_world_position_node() {
    let mut builder = MaterialAssetBuilder::new("M_WorldPos", KainEngineTarget::default());
    
    // Create world position node
    let world_pos = builder.add_world_position_node();
    
    // Connect to world position offset output
    builder.connect_to_world_position_offset(world_pos);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with world position node");
}

#[test]
fn test_world_normal_node() {
    let mut builder = MaterialAssetBuilder::new("M_WorldNormal", KainEngineTarget::default());
    
    // Create world normal node
    let world_normal = builder.add_world_normal_node();
    
    // Connect to normal output
    builder.connect_to_normal(world_normal);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with world normal node");
}

#[test]
fn test_absolute_world_position_node() {
    let mut builder = MaterialAssetBuilder::new("M_AbsWorldPos", KainEngineTarget::default());
    
    // Create absolute world position node
    let abs_world_pos = builder.add_absolute_world_position_node();
    
    // Use in a calculation
    let scale = builder.add_constant_node(0.01);
    let scaled_pos = builder.add_multiply_node(abs_world_pos, scale);
    
    // Connect to base color
    builder.connect_to_base_color(scaled_pos);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with absolute world position node");
}

#[test]
fn test_camera_position_node() {
    let mut builder = MaterialAssetBuilder::new("M_CameraPos", KainEngineTarget::default());
    
    // Create camera position and world position nodes
    let camera_pos = builder.add_camera_position_node();
    let world_pos = builder.add_world_position_node();
    
    // Calculate distance from camera
    let distance = builder.add_distance_node(world_pos, camera_pos);
    
    // Connect to emissive
    builder.connect_to_emissive(distance);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with camera position node");
}

#[test]
fn test_object_position_node() {
    let mut builder = MaterialAssetBuilder::new("M_ObjectPos", KainEngineTarget::default());
    
    // Create object position node
    let obj_pos = builder.add_object_position_node();
    
    // Connect to base color
    builder.connect_to_base_color(obj_pos);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with object position node");
}

#[test]
fn test_object_orientation_node() {
    let mut builder = MaterialAssetBuilder::new("M_ObjectOrient", KainEngineTarget::default());
    
    // Create object orientation node
    let obj_orient = builder.add_object_orientation_node();
    
    // Connect to base color
    builder.connect_to_base_color(obj_orient);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with object orientation node");
}

#[test]
fn test_triplanar_sampling() {
    let mut builder = MaterialAssetBuilder::new("M_Triplanar", KainEngineTarget::default());
    
    // Create texture parameter
    let texture = builder.add_texture_sample_parameter("BaseTexture", None);
    
    // Create triplanar sample node with default world position and blend sharpness
    let triplanar = builder.add_triplanar_sample_node(texture, None, 4.0);
    
    // Connect to base color
    builder.connect_to_base_color(triplanar);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with triplanar sampling");
}

#[test]
fn test_triplanar_sampling_custom_position() {
    let mut builder = MaterialAssetBuilder::new("M_TriplanarCustom", KainEngineTarget::default());
    
    // Create texture parameter
    let texture = builder.add_texture_sample_parameter("BaseTexture", None);
    
    // Create custom world position (scaled)
    let world_pos = builder.add_world_position_node();
    let scale = builder.add_constant_node(0.5);
    let scaled_pos = builder.add_multiply_node(world_pos, scale);
    
    // Create triplanar sample node with custom position
    let triplanar = builder.add_triplanar_sample_node(texture, Some(scaled_pos), 8.0);
    
    // Connect to base color
    builder.connect_to_base_color(triplanar);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build material with custom triplanar sampling");
}

#[test]
fn test_world_aligned_texture() {
    let mut builder = MaterialAssetBuilder::new("M_WorldAligned", KainEngineTarget::default());
    
    // Create texture parameter
    let texture = builder.add_texture_sample_parameter("Texture", None);
    
    // Get world position and scale it for tiling
    let world_pos = builder.add_world_position_node();
    let tiling = builder.add_constant_node(0.01); // 1 unit = 100cm in UE5
    let tiled_pos = builder.add_multiply_node(world_pos, tiling);
    
    // Use triplanar sampling for world-aligned texture
    let triplanar = builder.add_triplanar_sample_node(texture, Some(tiled_pos), 4.0);
    
    // Connect to base color
    builder.connect_to_base_color(triplanar);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build world-aligned texture material");
}

#[test]
fn test_world_space_procedural_effect() {
    let mut builder = MaterialAssetBuilder::new("M_ProceduralWorld", KainEngineTarget::default());
    
    // Get world position
    let world_pos = builder.add_world_position_node();
    
    // Extract Z component (height)
    let height = builder.add_component_mask_node(world_pos, false, false, true, false);
    
    // Scale height for gradient
    let scale = builder.add_constant_node(0.001);
    let gradient = builder.add_multiply_node(height, scale);
    
    // Clamp to 0-1 range
    let min = builder.add_constant_node(0.0);
    let max = builder.add_constant_node(1.0);
    let clamped = builder.add_clamp_node(gradient, min, max);
    
    // Create color gradient (lerp between two colors)
    let color_a = builder.add_constant3_node(0.0, 0.0, 1.0); // Blue
    let color_b = builder.add_constant3_node(1.0, 1.0, 1.0); // White
    let final_color = builder.add_lerp_node(color_a, color_b, clamped);
    
    // Connect to base color
    builder.connect_to_base_color(final_color);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build procedural world-space effect material");
}

#[test]
fn test_distance_fade_effect() {
    let mut builder = MaterialAssetBuilder::new("M_DistanceFade", KainEngineTarget::default());
    
    // Get camera and world positions
    let camera_pos = builder.add_camera_position_node();
    let world_pos = builder.add_world_position_node();
    
    // Calculate distance
    let distance = builder.add_distance_node(world_pos, camera_pos);
    
    // Scale distance for fade range
    let fade_scale = builder.add_constant_node(0.001);
    let fade_value = builder.add_multiply_node(distance, fade_scale);
    
    // Saturate to 0-1 range
    let fade_clamped = builder.add_saturate_node(fade_value);
    
    // Use as opacity
    builder.connect_to_opacity(fade_clamped);
    
    // Set blend mode to translucent
    builder.set_blend_mode(&BlendMode::Translucent);
    
    // Build should succeed
    let result = builder.build();
    assert!(result.is_ok(), "Failed to build distance fade material");
}
