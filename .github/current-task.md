Current Focus: Single-Draw MainScene3D (GPU-Driven Mesh Task Pipeline)

#Status
MainScene3D renders through framegraph with mesh shader path active.
Per-instance indirect draw is currently validation-clean (drawCount = 1 per call).
Scene buffers are uploaded on GPU (instances, materials, transforms, meshes).
Scene culling compute shader exists but is not yet integrated into framegraph execution.
Depth ownership migration is in progress and must remain stable.

#Goal (This Task)
Render the whole scene in MainScene3D with one graphics draw call per frame.
Move per-instance selection from CPU loop to GPU data-driven path.
Keep Vulkan validation clean during transition.

Minimal execution plan (recommended order):

[x] Register and run scene_culling compute pass.
Create indirect count buffer and reset it each frame.
Convert mesh shader to scene-buffer-driven indexing (remove per-instance constant buffers).
Replace per-instance loop with one indirect draw call in raster pass.
Add framegraph resource/barrier edges compute write -> graphics indirect read + shader read.

#Active Blockers
CPU-side per-instance binding still drives world/mesh/material state.
Mesh shader still depends on per-draw constants instead of scene-buffer indexing.
No per-frame compute pass currently producing final indirect command stream + visible count.
Barrier chain for compute-write to indirect-read/shader-read is not finalized in framegraph.

#Immediate Tasks
Integrate scene culling compute technique into MainScene3D framegraph path.
Add and maintain indirect command buffer plus indirect count buffer per frame.
Refactor scene mesh shader inputs to read instance/mesh/transform/material from structured buffers.
Switch MainScene3D raster submission from instance loop to one indirect dispatch call.
Preserve depth-stencil layout/aspect consistency and existing resize/minimize/restore stability.
Keep verification compute/raytrace modes functional after GPU-driven scene submission changes.

#Implementation Notes
Prefer one material/pipeline path for scene/mesh_scene_unlit to avoid pipeline switches.
Use bindless texture indexing for per-instance material texture selection.
If using drawCount > 1 on indirect draw path, ensure required feature support is explicitly enabled.
If feature support is unavailable, use indirect count extension path or compatible fallback strategy.
Add explicit framegraph resource dependencies for:
compute shader write -> indirect command read
compute shader write -> shader sampled/storage reads (scene data as needed)

#Next Milestones
Validation-clean single-draw MainScene3D on current demo scene.
Scene-driven BLAS input path (replace hardcoded raytrace triangle).
Expand culling quality (frustum first, then optional occlusion/Lod hooks).

#Definition of Done (This Stage)
MainScene3D submits scene with one graphics draw call.
No Vulkan validation errors in normal render loop.
Verification modes still render correctly.
Resize, minimize, and restore remain stable.
Framegraph barriers for compute-to-draw dependencies are explicit and validated.