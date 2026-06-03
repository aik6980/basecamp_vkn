## Main Goal
Deliver frame graph v1 with one compute pass and one raster pass.

## Scope
- Compute technique plumbing (extend technique system for compute pipelines)
- UAV descriptor binding support
- Frame graph pass/resource declaration and execution ordering
- Demo: compute writes texture, raster presents it
- Optional: ImGui toggle to switch demo shaders

## Definition of Done
- Demo visible in runtime
- Zero new validation errors
- Resize/minimize/restore works
- Clean create/destroy for graph resources
- Brief implementation notes added

## Next Steps (Priority Order)

### Phase 1: Compute Plumbing
- Add compute shader support to Technique class
  - Extend Technique_createinfo to support compute shaders
  - Add separate compute pipeline creation path (no vertex/fragment stages)
- Implement UAV binding in Technique_instance
  - Implement bind_uav_by_name() method (currently stubbed)
  - Add storage image/buffer support to descriptor pool
- Create a simple compute test shader in src/shaders/test/

### Phase 2: Minimal Frame Graph Core
- Define pass node abstraction
  - Pass type (raster or compute)
  - Read/write resource declarations
  - Execute callback
- Implement transient resource model
- Build simple dependency ordering from declarations
- Execute passes in correct order

### Phase 3: Demo Scenario
- Graph: Compute pass writes pattern to UAV texture → Raster pass samples and draws to swapchain
- Keep static resolution first
- Handle resize via offscreen recreation

### Phase 4: Hardening
- Validate resize/minimize/restore
- Resource destruction lifecycle
- Add maintenance notes
- Optional: ImGui shader selector