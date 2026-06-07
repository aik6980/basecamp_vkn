## Interim task
- Finish compute plumbing before building a larger scene system
- Keep any scene work minimal and only to remove hardcoded demo setup
- Do not start a full scene graph or ray tracing scene manager yet

## Main Goal
Deliver frame graph v1 with one compute pass and one raster pass, while preparing a minimal flattened render-scene data path for later GPU scene and ray tracing work.

## Scope
- Compute technique plumbing
  - Extend Technique class to support compute pipelines
  - Add compute pipeline creation path
- Descriptor/resource plumbing
  - Finish UAV descriptor binding
  - Add storage buffer binding support to Technique_instance and Resource_manager
- Minimal frame graph core
  - Pass node abstraction for compute and raster
  - Resource read/write declarations
  - Execution ordering from dependencies
- Demo scenario
  - Compute pass writes a texture
  - Raster pass samples and presents it
- Minimal render-scene preparation
  - Define a small CPU-side render scene with flat arrays only
  - Keep scope to materials, textures, and instances needed by the demo
  - Avoid full hierarchy/entity/editor concerns for now

## Non-Goals For This Task
- Full scene graph
- Full asset-driven world representation
- Ray tracing acceleration structure management
- BLAS/TLAS lifetime system
- Editor-facing scene authoring

## Definition of Done
- Compute to raster demo visible in runtime
- Zero new validation errors
- Resize/minimize/restore works
- Clean create/destroy for graph resources
- Storage buffers usable for future GPU-scene data
- Brief implementation notes added for follow-up scene and ray tracing work

## Next Steps (Priority Order)

### Phase 1: Compute Plumbing (Status: Done)
- Added compute shader support in Technique and Shader_manager registration flow
- Added compute pipeline creation and command dispatch path
- Validated reflected compute resource binding by name for UAV path
### Phase 2: Descriptor Expansion (Status: In Progress)
- Storage image UAV path is working for compute write then raster sample
- Added dedicated compute output texture with storage usage flags
- Added image layout/access transitions for compute write to fragment read
- Remaining: add storage buffer descriptor support in Technique_instance and Resource_manager

### Phase 3: Minimal Frame Graph
- Add compute and raster pass descriptions
- Add transient resource model
- Execute passes in dependency order

### Phase 4: Demo
- Compute writes pattern or test result into texture
- Raster samples that texture and presents it
- Optional toggle between existing raster demo and compute demo

### Phase 5: Small Render-Scene Seed
- Introduce flat render-scene structs only if needed
- Prefer arrays of instances/materials/textures over a hierarchy
- Design them so they can later upload into storage buffers and support ray tracing instance lookup