
# Project Goal
Build a Vulkan-based rendering sandbox/engine for learning and experimentation.
Support incremental, testable graphics and simulation milestones with a GPU-first architecture.

See current-task.md for active implementation scope.

## Current State
- Vulkan instance/device/surface creation.
- Swapchain creation and presentation loop.
- Per-frame command buffer recording and submission.
- Basic synchronization:
    - per-frame image-available semaphores
    - per-swapchain-image render-finished semaphores
    - per-frame in-flight fences
- Dynamic rendering path.
- Shader compilation/loading pipeline (HLSL with DXC target formats, including SPIR-V path).
- Basic resource management (buffers/images via allocator).
- Bindless texture demo path.
- Compute technique registration and compute pipeline dispatch path.
- Dedicated compute output texture path with storage image usage.
- Compute-to-raster synchronization via explicit image layout/access transitions.
- Framegraph scaffold in place with compute/raster sequencing.
- Raytracing infrastructure mostly integrated:
    - Device extensions/features for acceleration structures and ray tracing pipeline enabled.
    - BLAS/TLAS build path implemented (hardcoded triangle prototype).
    - Raytracing pipeline and SBT setup integrated.
    - Raytrace pass dispatched before raster pass.
    - Final visualization wiring in progress (raster sampling still points to compute output in current path).

## Planned Features (Priority Order)

### Phase 1: Complete Raytracing BLAS/TLAS Spike
- Finish raster visualization wiring to sample raytracing output texture.
- Validate hit-distance output in framebuffer.
- Confirm no new validation errors, including resize/minimize/restore behavior.

### Phase 2: Main Scene Render Path Stabilization
- Separate test scene path from main scene path.
- Make main scene explicitly 3D-focused (camera/view/projection integration and clean scene shader path).
- Keep test shaders available as isolated experiments.

### Phase 3: GPU-Driven Rendering Core
- Expand framegraph pass/resource model.
- Strengthen material and texture pipeline.
- Scene buffer binding for mesh_scene_unlit path.
- Compute culling pass groundwork.
- Move mesh/raytrace workflows toward indirect dispatch patterns.

### Phase 4: Scene-Driven Raytracing
- Connect BLAS input to scene geometry buffers.
- Move from hardcoded triangle to scene-driven geometry.
- Prepare for multiple BLAS instances and scalable TLAS usage.

### Phase 5: GPU Physics Module (Vulkan Compute, GPU-Persistent Data)
- Introduce physics storage buffers (rigid bodies, velocities, constraints).
- Add physics integration compute pass.
- Keep data GPU-resident and render directly from GPU-updated buffers.
- Add synchronization and pass ordering between physics, raytracing, and raster.
- Start with a minimal rigid-body prototype before broad feature expansion.

### Phase 6: Gameplay and Tooling
- Simple gameplay prototype.
- Editor workflow with ImGui integration.

## Nice to Have
- Cross-platform runtime support:
    - Windows
    - Linux

## Out of Scope (for now)
- Full production-grade physics engine parity.
- Heavy multi-platform abstraction layers that slow iteration.
- Complex editor feature set before core runtime paths are stable.

## Definition of Done for New Features
- Feature has a minimal runtime demo path.
- No new Vulkan validation errors.
- Handles resize/minimize/restore safely.
- Resource creation/destruction paths are clean.
- Framegraph synchronization and barriers are explicitly validated.
- Brief maintenance notes captured for future iteration.