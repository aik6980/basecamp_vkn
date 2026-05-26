
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

## Planned Features
- Camera system improvements.
- GPU Driven rendering pipeline
    - Frame graph implementation
    - Material and texture pipeline.
- instancing
- mesh pipeline experiments
- ray tracing experiments
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