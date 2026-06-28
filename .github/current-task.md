### Raytracing BLAS/TLAS Spike (Status: Raytrace Pipeline + Pass Integrated, Final Visualization Wiring In Progress)

**Goal**
- Build minimal Vulkan raytracing infrastructure (BLAS/TLAS) for foundational ray-triangle tracing.
- Demonstrate screen-space raytracing with hardcoded triangle geometry.
- Establish patterns that will scale to scene-driven raytracing later.

**Next Milestones**
- Build 3D Scene
- Return to Phase 5 completion: Bind scene storage buffers to mesh_scene_unlit shader.
- Connect scene geometry to BLAS (read vertices from scene buffers instead of hardcoded).
- Add compute culling pass for GPU-driven rendering.
- Switch mesh and raytrace to indirect dispatch patterns.

---

### GLM Migration Workstream (Planned: Start After BLAS/TLAS Spike Completion)

**Why**
- Move away from DirectX-dependent math types and APIs.
- Standardize CPU-side math on a platform-agnostic library for Windows/Linux.
- Keep compatibility with HLSL + DXC to SPIR-V workflow while preparing for GPU-physics integration.

**Migration Goal**
- Replace SimpleMath/DirectXMath usage in runtime code with GLM-based math wrappers.
- Preserve behavior first, then clean up conversion and legacy helpers.

**Scope (Initial Phase)**
- Add GLM to build and include paths.
- Introduce shared math typedef layer (`Vector2/3/4`, `Matrix`, `Quaternion`) backed by GLM.
- Migrate high-touch runtime files first:
	- `src/vulkan_test/renderscene.cpp`
	- `src/vulkan_test/renderscene.h`
	- `src/vulkan_test/app.cpp` (remove `XMCOLOR` usage path)
- Remove unnecessary DirectXMath conversion calls (`XMLoad*`, `XMStore*`) from scene upload path.

**Out of Scope (Initial GLM Phase)**
- Full rewrite of all legacy utility modules.
- Physics solver implementation details.
- Shader-side math changes (HLSL remains unchanged).

**Validation Checklist (GLM Migration)**
- Project compiles on Windows with no new warnings in migrated files.
- Scene rendering output matches pre-migration behavior.
- Raytracing spike path remains functional.
- No new validation-layer errors introduced by migration.
- Linux build path remains viable (no new DirectX-only dependency added).

**Definition of Done (Initial GLM Migration)**
- GLM dependency integrated and used by shared runtime math aliases.
- Migrated files no longer rely on SimpleMath/DirectXMath calls.
- Existing render + raytrace demo path still runs with expected output.
- Follow-up migration backlog captured in feature-list phases.