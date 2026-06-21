### Small Render-Scene Seed (Status: Step 5 In Progress - Scene path active with multi-instance CPU loop, Active)

**Goal**
- Introduce a minimal CPU-side render-scene with flat arrays, integrated into current draw flow.
- Keep scene data compatible with both mesh-shader raster and future ray tracing pipelines.

**Scope**
- Keep flat render-scene data model for textures, materials, transforms, and instances.
- Route mesh pass material and texture binding through scene arrays.
- Route instance transform and material selection through scene indices.
- Keep all scene data CPU-side in this phase.
- Keep test shaders isolated from scene shaders.

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
5. In progress: Multi-instance scene traversal in mesh pass with per-instance selection.
6. Next: Harden index-range validation and add clear failure diagnostics.
7. Next: Add per-frame scene counters and lightweight debug reporting.
8. Next: Freeze a shared scene schema contract that is backend-agnostic for raster and ray tracing.
9. Next: Prepare upload-ready layout notes for storage-buffer migration in next phase.

**Validation Checklist (Current Phase)**
- Visual output remains equivalent to prior demo intent.
- No new Vulkan validation errors.
- Resize, minimize, and restore remain stable.
- Scene traversal remains linear through flat arrays.
- Scene shader path is separate from test shader path.
- Data layout remains upload-ready.

**Definition of Done (Phase 5)**
- Mesh shader scene path uses scene arrays for technique, material, texture, and instance selection.
- Multi-instance render path runs from scene instances without hardcoded draw selection.
- Index validation is enforced for all scene references.
- Per-frame counters for textures, materials, transforms, and instances are available.
- No new validation errors and runtime stability preserved.

**Next After Phase 5**
- Upload scene arrays into storage buffers.
- Bind scene buffers through technique instance storage-buffer path.
- Add compute culling pass that writes visible instance list and indirect args.
- Switch mesh draw from CPU loop to indirect mesh dispatch.
- Reuse the same scene instance and material records for ray tracing instance lookup and shading.