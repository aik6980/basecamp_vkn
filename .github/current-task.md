## Interim task
- Finish compute plumbing before building a larger scene system
- Keep any scene work minimal and only to remove hardcoded demo setup
- Do not start a full scene graph or ray tracing scene manager yet

## Main Goal
Deliver frame graph v1 with one compute pass and one raster pass, while preparing a minimal flattened render-scene data path for later GPU scene and ray tracing work, and introducing a scratch allocator path for transient uploads and per-frame descriptor data.

## Scope
- Compute technique plumbing
  - Extend Technique class to support compute pipelines
  - Add compute pipeline creation path
- Descriptor/resource plumbing
  - Finish UAV descriptor binding
  - Add storage buffer binding support to Technique_instance and Resource_manager
- Scratch allocator foundation
  - Add transient upload scratch path for staging copies in Resource_manager
  - Add per-frame scratch path for constant and storage buffer descriptor payloads
  - Define reset points tied to single-submit completion and per-frame fence-safe begin frame
  - Enforce offset/range alignment for descriptor buffer writes
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
- Temporary staging uploads use transient scratch allocation instead of accumulating persistent staging buffers
- Per-frame transient constant buffer data uses frame scratch allocation with safe frame reset
- Storage buffer descriptor path works with correct offset/range and alignment
- Brief implementation notes added for follow-up scene and ray tracing work

## Scratch Allocator Plan
1. Upload scratch allocator
  - Add a mapped, host-visible upload scratch buffer allocator for single-submit resource upload flow
  - Suballocate staging ranges for buffer/image upload instead of creating one staging buffer per resource
  - Reset allocator at end of single-submit submission after queue completion

2. Frame scratch allocator
  - Add per-frame scratch allocator instance reset during frame begin after fence wait
  - Use it for transient descriptor payloads (constant first, storage buffer next)
  - Keep descriptor writes contiguous and stable for the full apply/bind window

3. Resource_manager integration
  - Replace m_staging_buffers growth path with upload scratch suballocations
  - Keep persistent GPU resources unchanged (textures/samplers/device-local buffers)

4. Technique_instance integration
  - Move bind_constant_by_name to frame scratch suballocation instead of per-call buffer allocation
  - Add storage buffer bind path using named/persistent buffers and optional transient scratch-backed payload

5. Validation and instrumentation
  - Validate descriptor offsets/ranges and required alignment for uniform/storage usage
  - Track peak scratch usage per frame and per upload pass for sizing follow-up

## Next Steps (Priority Order)

### Phase 1: Compute Plumbing (Status: Done)
- Added compute shader support in Technique and Shader_manager registration flow
- Added compute pipeline creation and command dispatch path
- Validated reflected compute resource binding by name for UAV path
### Phase 2: Descriptor Expansion + Scratch Allocator (Status: In Progress)
**Upload Scratch Allocator (Complete)**
- Added Scratch_allocator class with multi-page linear allocation and reuse pattern
- Integrated allocator into Resource_manager (16MB default page size)
- Migrated create_buffer and create_texture upload paths to use allocator suballocations
- Wired submit-batch lifecycle: reset at begin_single_command_submission and after queue.waitIdle() in end_single_command_submission
- Cleaned up old m_staging_buffers pattern—allocator now owns all transient upload pages

**Frame Scratch Allocator + Constant Migration (Complete)**
- Added per-frame Scratch_allocator in Frame_resource for transient descriptor payload data
- Reset frame scratch and descriptor pool in frame_resource::begin_frame() after fence-safe frame begin
- Migrated bind_constant_by_name to frame scratch suballocation path
- Removed per-frame temporary constant-buffer ownership list from Frame_resource

**Remaining Work**
- Add storage buffer descriptor support in Technique_instance and Resource_manager
- Replace hardcoded uniform-buffer alignment with device limit driven alignment
- Add explicit alignment validation/asserts for uniform/storage descriptor offsets and ranges
- Add scratch usage instrumentation (peak bytes per frame and per upload pass)

### Immediate Follow-up
- **Priority 1: Storage Buffer Support**
  - Add storage buffer bind path to Technique_instance (bind_storage_buffer_by_name)
  - Extend Resource_manager to support named persistent storage buffers
  - Validate descriptor offset/range alignment for storage buffer writes

- **Priority 2: Alignment Hardening**
  - Query physical-device limits for uniform/storage alignment requirements
  - Replace hardcoded alignment constants in frame-scratch constant binding
  - Add alignment assertions for scratch offset/range writes

- **Priority 3: Validation & Instrumentation**
  - Add basic peak scratch usage stats (bytes per upload batch, bytes per frame)
  - Add startup reflection assertions for compute binding names and descriptor types

- Remove hardcoded compute dispatch texture size and query dimensions from the target texture
- Add runtime toggle between compute output and static sampled texture for debugging

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