
# Project goal
Build a Vulkan-based rendering sandbox/engine for learning and experimentation.
Support incremental graphics features with clear, testable milestones.

See current-task.md for active implementation scope

## Current State
- Vulkan instance/device/surface creation.
- Swapchain creation and presentation loop.
- Per-frame command buffer recording and submission.
- Basic synchronization:
    - per-frame image-available semaphores
    - per-swapchain-image render-finished semaphores
    - per-frame in-flight fences
- Dynamic rendering path.
- Shader compilation/loading pipeline (HLSL to target shader formats).
- Basic resource management (buffers/images via allocator).
- Support a simple case of bindless texture
- Compute technique registration and compute pipeline dispatch path.
- Dedicated compute output texture path with storage image usage.
- Compute-to-raster synchronization via explicit image layout/access transitions.
- Compute output sampling integrated into raster demo path.

## Planned Features
- Camera system improvements.
- GPU Driven rendering pipeline
    - Frame graph implementation
    - Material and texture pipeline.
    - Compute and raster pass orchestration foundation:
        - One compute pass writing UAV texture
        - One raster pass sampling compute output
        - Next: formalize into frame graph pass/resource nodes
- GPU Driven rendering pipeline use mesh shader
- instancing
- mesh pipeline experiments
- ray tracing experiments
- a simple physic module, so I can use it to make a game
- a simple game
- Full game-engine editor workflow.

## Nice to have
- Cross-platform
    - Linux support

## Out of Scope (for now)

## Definition of Done for New Features
- Feature has a minimal demo path in runtime.
- No new validation errors.
- Handles resize/minimize/restore safely.
- Resource creation and destruction paths are clean.
- Brief notes added for future maintenance.

## Editor using ImGUI