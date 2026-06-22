### Small Render-Scene Seed (Status: Phase 5 Complete - GPU buffer upload path in progress, Active)

**Goal**
- Introduce a minimal CPU-side render-scene with flat arrays, integrated into current draw flow.
- Keep scene data compatible with both mesh-shader raster and future ray tracing pipelines.

**Scope**
- Keep flat render-scene data model for textures, materials, transforms, and instances.
- Route mesh pass material and texture binding through scene arrays.
- Route instance transform and material selection through scene indices.
- Keep test shaders isolated from scene shaders.
- Upload scene arrays into GPU storage buffers using shared Scene_* layout.

**Non-Goals (This Phase)**
- Scene hierarchy or entity graph.
- Editor authoring flow.
- BLAS and TLAS build path.
- GPU culling and indirect dispatch execution.
- Full ray tracing shading integration.

**Implementation Plan**
1. Done: Minimal POD-style scene structs added.
2. Done: Minimal mesh shader test backend added.
3. Done: Demo scene bootstrap populates arrays.
4. Done: Scene technique and material texture binding path added.
5. Done: Multi-instance scene traversal in mesh pass with per-instance selection.
6. Done: Index-range validation with verbose diagnostics added.
7. Done: Per-frame scene counters and debug reporting added.
8. Done: Shared scene schema frozen - Scene_mesh_desc, Scene_material_desc, Scene_transform_desc, Scene_instance_desc added to hlsl_shared_struct.h. CPU structs renamed to snake_case convention.
9. In progress: Implement upload_to_gpu() in Render_scene_state to pack CPU arrays into GPU storage buffers using Scene_* layout.

**Validation Checklist**
- Visual output remains equivalent to prior demo intent.
- No new Vulkan validation errors.
- Resize, minimize, and restore remain stable.
- Scene traversal remains linear through flat arrays.
- Scene shader path is separate from test shader path.
- CPU and GPU scene struct layouts are kept in sync.

**Definition of Done (Phase 5 - complete)**
- Mesh shader scene path uses scene arrays for technique, material, texture, and instance selection.
- Multi-instance render path runs from scene instances without hardcoded draw selection.
- Index validation is enforced with verbose diagnostics.
- Per-frame counters for textures, materials, transforms, and instances are available.
- Shared GPU-compatible scene structs frozen in hlsl_shared_struct.h.
- upload_to_gpu packs and submits all scene arrays as named GPU storage buffers.

**Next After GPU Upload**
- Bind scene storage buffers to scene/mesh_scene_unlit shader via bind_storage_buffer_by_name.
- Verify buffer contents match CPU scene by checking instance count in debug output.
- Add compute culling pass that writes visible instance list and indirect args.
- Switch mesh draw from CPU loop to indirect mesh dispatch.
- Reuse the same scene instance and material records for ray tracing instance lookup and shading.