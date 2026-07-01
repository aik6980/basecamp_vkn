Current Focus: Build 3D Scene

#Status
Raytracing BLAS/TLAS pipeline is integrated.
Verification compute and raytrace fullscreen visualization are working.
MainScene3D path is wired and running through framegraph.
Depth buffer ownership has moved to resource manager (in progress stabilization).

#Active Blocker
Vulkan depth/stencil barrier and layout consistency for D24S8.
Use matching depth-stencil layout and aspect mask across framegraph barrier and rendering attachment.
Keep configuration aligned with whether separateDepthStencilLayouts is enabled.
Immediate Tasks
Make MainScene3D validation-clean.
Finalize depth barrier/layout setup.
Confirm depth attachment is always bound in rendering info.
Keep verification paths stable after depth migration.

#Next Milestones
Bind scene storage buffers to mesh_scene_unlit.
Connect scene geometry to BLAS instead of hardcoded triangle.
Add compute culling groundwork.
Move mesh and raytrace flows toward indirect dispatch.

#Definition of Done (This Stage)
MainScene3D renders correctly every frame.
No Vulkan validation errors on depth/barriers.
Verification modes still work after scene/depth changes.
Resize, minimize, and restore remain stable.