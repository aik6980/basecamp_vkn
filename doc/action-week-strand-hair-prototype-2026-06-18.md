# Action-Week Plan: Strand Hair Prototype (Toy Renderer, Frostbite-like)

## Week Goal
Build a real-time strand hair vertical slice with:
- Groom import
- Basic simulation
- Strand rendering
- Anisotropic shading
- Self-shadow approximation
- Stable camera motion

Target end-of-week demo: one animated head with believable hair motion at interactive framerate.

## Scope Guardrails
- In scope: one hairstyle, one character head, one directional key light, one camera path.
- Out of scope: full authoring tools, perfect physics accuracy, production-quality AA, console optimization.
- Rule: prefer working end-to-end over best-in-class depth in one subsystem.

## Architecture Slice (Frostbite-like Mindset)
1. Data
- Guide strands + generated render strands
- Root attachment to scalp mesh
- Per-strand attributes: width, color, roughness, bend stiffness

2. Simulation
- Guide-only simulation on CPU first
- Constraints: length + bend + root pinning
- 2-4 substeps max, predictable timestep

3. Rendering
- Generate camera-facing ribbon segments from strands
- Frustum cull + simple LOD by strand decimation
- Weighted blended transparency for fast OIT

4. Shading
- Start with Kajiya-Kay style anisotropic highlights
- Add secondary lobe only if time remains
- Single directional light + ambient term

5. Shadows
- Approximate self-shadow via deep opacity map or simplified shadow attenuation term
- Keep it cheap and stable first

## Daily Plan (5 Days)

### Day 1: Data + Scaffold
- Implement groom/strand data container and loader.
- Attach roots to scalp space.
- Render debug lines for guides and generated strands.
- Deliverable: static hairstyle visible and transform-correct on animated head.

### Day 2: Simulation MVP
- Add guide simulation loop with root pinning and length constraints.
- Add damping and gravity/wind controls.
- Expose 4-6 tweakable params in UI.
- Deliverable: guides move plausibly without exploding.

### Day 3: Render Strands + Visibility
- Interpolate render strands from guides.
- Convert to ribbon segments and render with transparency.
- Add basic culling and strand decimation LOD.
- Deliverable: full strand volume at target camera distance, interactive performance.

### Day 4: Shading + Shadow Approx
- Implement anisotropic strand lighting (primary lobe).
- Add simple self-shadow approximation.
- Add roughness and melanin-like color controls.
- Deliverable: hair reads as hair under camera orbit and light rotation.

### Day 5: Stabilize + Demo
- Fix flicker/pop, tune transparency and width scaling.
- Add camera path and side-by-side debug toggles:
  - guides
  - render strands
  - shading only
  - shadow on/off
- Record short demo clip and summary notes.
- Deliverable: reproducible showcase build + brief readout.

## Definition of Done (Friday)
- Hair remains stable for a 10-15 second camera orbit.
- No catastrophic simulation failures.
- Clear anisotropic highlight response with light direction.
- Acceptable visual density at near and mid distance.
- One-click demo scene launch.

## Risk List + Mitigation
1. Simulation instability
- Mitigation: clamp velocities, fixed timestep, fewer constraints before adding complexity.

2. Transparency artifacts
- Mitigation: weighted blended OIT first; postpone perfect sorting.

3. Performance drops
- Mitigation: strand decimation, segment cap per strand, aggressive frustum culling.

4. Scope creep
- Mitigation: no new features after Day 3 noon unless blocking.

## Nice-to-Have If Ahead
- Secondary specular lobe
- Better self-shadow quality
- Simple collision capsules for head/shoulders
- Temporal accumulation for smoother strands
