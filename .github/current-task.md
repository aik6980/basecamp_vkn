### Raytracing BLAS/TLAS Spike (Status: Core Infrastructure Complete - Shader/Pipeline Integration Active)

**Goal**
- Build minimal Vulkan raytracing infrastructure (BLAS/TLAS) for foundational ray-triangle tracing.
- Demonstrate screen-space raytracing with hardcoded triangle geometry.
- Establish patterns that will scale to scene-driven raytracing later.

**Scope**
- Add VK_KHR_acceleration_structure and raytracing extensions to device.
- Implement BLAS building from vertex/index buffers.
- Implement TLAS building with instance transforms.
- Create hardcoded single-triangle BLAS for proof-of-concept.
- Write raytracing pipeline (rgen/rchit/rmiss shaders).
- Integrate raytrace dispatch into framegraph before raster pass.
- Output hit distance to UAV texture, sample in raster for visualization.

**Non-Goals (This Phase)**
- Scene-driven BLAS/TLAS (geometry pulled from scene buffers).
- Multiple BLASes or complex geometry.
- BLAS/TLAS updates mid-frame or dynamic geometry.
- Ray tracing material shading (shading comes later).
- Indirect dispatch or compute culling integration.

**Implementation Plan**
1. Done: Added raytracing device extensions (VK_KHR_acceleration_structure, VK_KHR_ray_tracing_pipeline, etc.) to Device::get_device_extensions().
2. Done: Enabled acceleration structure, ray tracing pipeline, and buffer device address features.
3. Done: Added Acceleration_structure, BLAS, and TLAS resource structs.
4. Done: Implemented BLAS building function (Resource_manager::build_blas_from_buffers).
5. Done: Implemented TLAS building function (Resource_manager::build_tlas_from_blas_instances).
6. Done: Created single hardcoded triangle vertex/index buffers in app initialization.
7. Done: Built BLAS from triangle buffers in app initialization.
8. Done: Built TLAS with single BLAS instance in app initialization.
9. Not started: Write raytracing shaders (simple_raytracing.rgen, .rchit, .rmiss).
10. Not started: Create raytracing pipeline in shader_manager.
11. In progress: Add raytrace dispatch pass to framegraph (before raster pass).
12. Not started: Integrate raytrace output texture sampling in raster pass.

**Validation Checklist**
- BLAS/TLAS creation completes without validation errors.
- Raytracing pipeline compiles and dispatches without validation errors.
- Triangle renders with distance-encoded colors (grayscale gradient).
- No new Vulkan validation errors from acceleration structure operations.
- Resize, minimize, and restore remain stable.
- Raytrace output visible in framebuffer (either standalone or blended).

**Definition of Done (BLAS/TLAS Spike)**
- VK_KHR_acceleration_structure and related extensions enabled.
- BLAS building infrastructure in resource_manager (create from vertex/index buffers).
- TLAS building infrastructure in resource_manager (create from BLAS instances + transforms).
- Single hardcoded triangle BLAS built at initialization.
- Single-instance TLAS built at initialization.
- Raytracing pipeline created (rgen/rchit/rmiss compiled and linked).
- Raytrace pass dispatched in framegraph before raster.
- Hit distance from ray-triangle intersection visible in output texture.

**Next Milestones (After BLAS/TLAS Spike)**
- Return to Phase 5 completion: Bind scene storage buffers to mesh_scene_unlit shader.
- Connect scene geometry to BLAS (read vertices from scene buffers instead of hardcoded).
- Add compute culling pass for GPU-driven rendering.
- Switch mesh and raytrace to indirect dispatch patterns.