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
### Phase 2: Descriptor Expansion + Scratch Allocator (Status: Done)
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

### Phase 3: Minimal Frame Graph (Status: In progress)
- Add compute and raster pass descriptions
- Add transient resource model
- Execute passes in dependency order

**Current Task: Frame Graph Transition Ownership**
- Promote frame graph from ordering-only to ordering plus transition management for graph-managed resources.

**Scope**
- Add image transition metadata to frame-graph edge handling and emit image barriers during execute.
- Keep pass execute lambdas focused on pipeline bind and draw/dispatch only.
- Remove manual transitions for t_compute_output from pass code.
- Keep swapchain and final blit transitions outside frame graph in this phase.

**Definition of Done**
- Compute pass writes t_compute_output and raster pass samples it without manual transitions inside those pass lambdas.
- Frame graph emits correct synchronization and layout transitions for graph-managed image resources.
- No new Vulkan validation errors.
- Existing resize/minimize/restore behavior is unchanged.

**Non-Goals (This Step)**
- Full transient resource allocator inside frame graph.
- Multi-queue scheduling.
- Full swapchain present-path ownership by frame graph.

**Implementation Order**
1. Extend frame graph resource-use metadata in framegraph data model.
2. Add image-barrier emission path in execute from declared hazards.
3. Refactor main renderer pass lambdas to remove manual compute-output transitions.
4. Validate runtime behavior and validation-layer cleanliness.

### Phase 4: Demo
- Compute writes pattern or test result into texture
- Raster samples that texture and presents it
- Optional toggle between existing raster demo and compute demo

### Phase 5: Small Render-Scene Seed
- Introduce flat render-scene structs only if needed
- Prefer arrays of instances/materials/textures over a hierarchy
- Design them so they can later upload into storage buffers and support ray tracing instance lookup