### Small Render-Scene Seed (Status: Step 2 Complete - Mesh shader working, Active)

**Goal**
- Introduce a minimal CPU-side render-scene with flat arrays only, integrated into current draw flow.

**Scope**
- Add flat render-scene data model for textures, materials, transforms, and instances
- Replace hardcoded draw binding inputs with scene-array lookups while preserving current output
- Keep all scene data CPU-side in this phase
- Use Mesh shader as the first geometry backend for this phase
- Skip vertex and index buffer ownership for now

**Non-Goals (This Phase)**
- Scene hierarchy/entity graph
- Editor authoring flow
- BLAS/TLAS and ray tracing instance system
- GPU-driven culling/indirect dispatch

**Implementation Plan**
1. ✅ Add minimal POD-style structs (DONE)
2. ✅ Add a minimal mesh shader test path for the new backend (DONE)
3. 🔄 Add demo scene bootstrap function to populate flat arrays (NEXT)
4. Route raster or mesh pass material/texture binding through scene arrays
5. Route instance transform/material selection through scene indices
6. Add index-range validation checks for scene references
7. Add per-frame scene counters (textures/materials/instances) for sanity checks

**Definition of Done (Phase 5)**
- Runtime visual output is equivalent to current demo
- No new Vulkan validation errors
- Resize/minimize/restore remains stable
- Scene traversal is linear through flat arrays (no hierarchy)
- Mesh shader test path compiles and runs in the demo pipeline
- Data layout is upload-ready for later storage-buffer integration

**Next After Phase 5**
- Upload render-scene arrays into storage buffers
- Bind scene buffers via Technique_instance storage-buffer path
- Preserve data model compatibility for future ray tracing instance lookup