# Action Week: Simplified Surfel GI (GIBS-Inspired) for Toy Renderer

**Author:** Aik  
**Date:** 2026-06-15  
**Duration:** 1 Action Week (5 days)  
**Goal:** Prototype a beginner-friendly surfel-based GI system inspired by Frostbite's GIBS technique

---

## 1. Executive Summary

Build a minimal but complete surfel-based global illumination system that demonstrates the same core technique as Frostbite's GIBS (Global Illumination with Beam Surfels). This is a **learning prototype** — no production concerns, no multi-platform, no optimization passes. Pure algorithmic clarity.

### What is GIBS in a Nutshell?

GIBS represents indirect lighting using **surfels** (small discs on surfaces). Each surfel:
1. Gets placed on visible geometry
2. Shoots rays to gather indirect light
3. Stores irradiance (incoming light)
4. Contributes its stored light to nearby pixels

This is essentially a **world-space irradiance cache** with ray-traced updates.

---

## 2. Technique Overview (Simplified vs. Production)

| Concept | Production GIBS | Our Simplified Version |
|---------|----------------|----------------------|
| Surfel storage | 40+ GPU buffers, clipmap hierarchy | Single flat buffer (position + normal + irradiance) |
| Spatial structure | Clipmap probe volume, hierarchical grid | Simple uniform 3D grid (hash map) |
| Ray tracing | Hardware RT with acceleration structures | Software ray marching against depth buffer OR simple scene SDF |
| Lighting | Full PBR, light trees, analytic lights | Single directional light + sky color |
| Filtering | Multi-pass temporal + spatial smoothing | Simple exponential moving average (EMA) |
| Screen apply | Half-res splatting with upscale | Full-res nearest-surfel lookup |
| Lifecycle | Spawn/recycle/age/merge/split | Simple spawn (no recycling, fixed count) |
| Multi-bounce | Surfel-to-surfel lighting cascade | Single bounce only |

---

## 3. Algorithm Overview (5 Compute Passes)

```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Pass 1:     │     │ Pass 2:      │     │ Pass 3:      │     │ Pass 4:      │     │ Pass 5:      │
│ G-Buffer    │────▶│ Surfel Spawn │────▶│ Surfel Light │────▶│ Grid Build   │────▶│ Apply GI     │
│ (Raster)    │     │ (Compute)    │     │ (Compute)    │     │ (Compute)    │     │ (Compute)    │
└─────────────┘     └──────────────┘     └──────────────┘     └──────────────┘     └──────────────┘
     │                    │                     │                    │                    │
 Depth+Normal        Place surfels         Trace rays &        Hash surfels        Look up grid
 + Albedo            on surfaces           accumulate light    into 3D grid        for indirect
```

---

## 4. Data Structures

### 4.1 Surfel (32 bytes)

```hlsl
struct Surfel
{
    float3 position;   // World-space position (12 bytes)
    float  radius;     // Disc radius (4 bytes)
    float3 normal;     // Surface normal (12 bytes)
    uint   packed;     // Packed: age (8 bits) + flags (8 bits) + unused (16 bits)
    float3 irradiance; // Accumulated indirect light (12 bytes)
    float  weight;     // Confidence / sample count (4 bytes)
};
// Total: 44 bytes — pad to 48 for alignment, or pack tighter:
// Alt compact layout (32 bytes):
struct SurfelCompact
{
    float3 position;     // 12 bytes
    uint   normalPacked; // Octahedral-encoded normal (4 bytes)
    float3 irradiance;   // 12 bytes
    float  radius;       // 4 bytes
};
```

### 4.2 Spatial Grid Cell

```hlsl
struct GridCell
{
    uint surfelIndices[MAX_SURFELS_PER_CELL]; // e.g., 4-8 indices
    uint count;
};
```

### 4.3 Constants

```hlsl
cbuffer GIConstants : register(b0)
{
    float4x4 viewProj;
    float4x4 invViewProj;
    float3   cameraPos;
    float    gridCellSize;      // World-space size of each grid cell (e.g., 0.5m)
    int3     gridDimensions;    // e.g., 64x32x64
    float3   gridOrigin;       // World-space corner of the grid
    uint     maxSurfels;        // e.g., 65536
    uint     frameIndex;        // For temporal jitter
    float    surfelRadius;      // Default radius (e.g., 0.1m)
    float    rayMaxDistance;    // Max trace distance
    float    temporalBlend;    // EMA factor (e.g., 0.05)
};
```

---

## 5. Pass Details with HLSL Pseudocode

### Pass 1: G-Buffer (Standard Raster Pass)

Nothing special — standard deferred rendering G-Buffer:
- **Output RT0:** World Normal (RGB) + Roughness (A)
- **Output RT1:** Albedo (RGB) + Metalness (A)  
- **Output Depth:** Hardware depth

*(Use your existing G-Buffer pass — no changes needed)*

---

### Pass 2: Surfel Spawn (Compute)

**Purpose:** Place new surfels on visible geometry by reading back the G-Buffer.

**Dispatch:** `(screenWidth/8, screenHeight/8, 1)` — one surfel candidate per 8x8 tile.

```hlsl
// Pass2_SurfelSpawn.hlsl
RWStructuredBuffer<SurfelCompact> g_Surfels : register(u0);
RWByteAddressBuffer g_SurfelCounter         : register(u1);
Texture2D<float>  g_Depth                   : register(t0);
Texture2D<float4> g_Normal                  : register(t1);

[numthreads(8, 8, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    // Only thread (0,0) of each group spawns a surfel
    if (any(gtid.xy != 0))
        return;

    // Pick a jittered pixel within this 8x8 tile
    uint2 tileOrigin = groupId.xy * 8;
    uint  seed = pcgHash(groupId.x + groupId.y * 9999 + frameIndex * 77777);
    uint2 offset = uint2(seed % 8, (seed >> 4) % 8);
    uint2 pixel = tileOrigin + offset;

    // Read depth and reconstruct world position
    float depth = g_Depth[pixel];
    if (depth >= 1.0) return; // Sky

    float3 worldPos = reconstructWorldPos(pixel, depth, invViewProj);
    float3 normal = g_Normal[pixel].xyz * 2.0 - 1.0;

    // Allocate surfel slot
    uint idx;
    g_SurfelCounter.InterlockedAdd(0, 1, idx);
    if (idx >= maxSurfels) return;

    // Write surfel
    SurfelCompact s;
    s.position = worldPos;
    s.normalPacked = encodeOctahedral(normal);
    s.irradiance = float3(0, 0, 0);
    s.radius = surfelRadius;
    g_Surfels[idx] = s;
}
```

---

### Pass 3: Surfel Lighting (Compute)

**Purpose:** Each surfel traces rays to gather indirect illumination.

**Dispatch:** `(surfelCount / 64, 1, 1)` — one thread per surfel.

```hlsl
// Pass3_SurfelLighting.hlsl
RWStructuredBuffer<SurfelCompact> g_Surfels : register(u0);
Texture2D<float>  g_Depth                   : register(t0);
Texture2D<float4> g_Albedo                  : register(t1);
Texture2D<float4> g_DirectLight             : register(t2); // Previous frame's lit result

cbuffer GIConstants : register(b0) { /* ... */ };

[numthreads(64, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint surfelIdx = dtid.x;
    if (surfelIdx >= surfelCount) return;

    SurfelCompact surfel = g_Surfels[surfelIdx];
    float3 pos = surfel.position;
    float3 N = decodeOctahedral(surfel.normalPacked);

    // Trace N_RAYS rays in cosine-weighted hemisphere
    float3 accum = float3(0, 0, 0);
    const uint N_RAYS = 4; // Keep low for perf (production uses 1-2!)

    for (uint i = 0; i < N_RAYS; i++)
    {
        // Generate random direction in hemisphere
        uint seed = pcgHash(surfelIdx * N_RAYS + i + frameIndex * 12345);
        float3 dir = cosineHemisphereSample(N, seed);

        // Screen-space ray march
        float3 hitPos;
        float2 hitUV;
        if (screenSpaceRayMarch(pos, dir, hitPos, hitUV))
        {
            // Sample the direct lighting at the hit point
            float3 hitRadiance = g_DirectLight.SampleLevel(samplerLinear, hitUV, 0).rgb;
            float3 hitAlbedo = g_Albedo.SampleLevel(samplerLinear, hitUV, 0).rgb;
            accum += hitRadiance * hitAlbedo;
        }
        else
        {
            // Sky fallback
            accum += sampleSkyColor(dir);
        }
    }

    accum /= float(N_RAYS);

    // Temporal accumulation (EMA)
    float3 prev = surfel.irradiance;
    surfel.irradiance = lerp(prev, accum, temporalBlend);
    g_Surfels[surfelIdx] = surfel;
}
```

---

### Pass 4: Grid Build (Compute)

**Purpose:** Hash all surfels into a uniform 3D grid for fast spatial lookup.

**Dispatch:** `(surfelCount / 64, 1, 1)`

```hlsl
// Pass4_GridBuild.hlsl
StructuredBuffer<SurfelCompact> g_Surfels   : register(t0);
RWStructuredBuffer<GridCell>    g_Grid      : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint surfelIdx = dtid.x;
    if (surfelIdx >= surfelCount) return;

    SurfelCompact surfel = g_Surfels[surfelIdx];

    // Compute grid cell index
    int3 cell = worldToGrid(surfel.position, gridOrigin, gridCellSize);
    if (any(cell < 0) || any(cell >= gridDimensions))
        return;

    uint cellIdx = flattenGridIndex(cell, gridDimensions);

    // Atomic append to cell's surfel list
    uint slot;
    InterlockedAdd(g_Grid[cellIdx].count, 1, slot);
    if (slot < MAX_SURFELS_PER_CELL)
    {
        g_Grid[cellIdx].surfelIndices[slot] = surfelIdx;
    }
}
```

**Note:** Clear the grid to zero before this pass (a simple `ClearUAV` or a small clear compute).

---

### Pass 5: Apply GI (Compute or Full-Screen Pixel Shader)

**Purpose:** For each pixel, look up nearby surfels and accumulate their irradiance contribution.

**Dispatch:** `(screenWidth/8, screenHeight/8, 1)`

```hlsl
// Pass5_ApplyGI.hlsl
StructuredBuffer<SurfelCompact> g_Surfels : register(t0);
StructuredBuffer<GridCell>      g_Grid    : register(t1);
Texture2D<float>  g_Depth                 : register(t2);
Texture2D<float4> g_Normal                : register(t3);
Texture2D<float4> g_Albedo                : register(t4);
RWTexture2D<float4> g_Output              : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint2 pixel = dtid.xy;
    float depth = g_Depth[pixel];
    if (depth >= 1.0)
    {
        g_Output[pixel] = float4(0, 0, 0, 0);
        return;
    }

    float3 worldPos = reconstructWorldPos(pixel, depth, invViewProj);
    float3 normal = g_Normal[pixel].xyz * 2.0 - 1.0;
    float3 albedo = g_Albedo[pixel].rgb;

    // Look up grid cell
    int3 cell = worldToGrid(worldPos, gridOrigin, gridCellSize);
    float3 indirectLight = float3(0, 0, 0);
    float totalWeight = 0.0;

    // Check this cell + 26 neighbors (3x3x3)
    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
    {
        int3 neighbor = cell + int3(dx, dy, dz);
        if (any(neighbor < 0) || any(neighbor >= gridDimensions))
            continue;

        uint cellIdx = flattenGridIndex(neighbor, gridDimensions);
        GridCell gc = g_Grid[cellIdx];

        for (uint i = 0; i < min(gc.count, MAX_SURFELS_PER_CELL); i++)
        {
            SurfelCompact s = g_Surfels[gc.surfelIndices[i]];
            float3 sN = decodeOctahedral(s.normalPacked);

            // Distance-based weight
            float dist = length(s.position - worldPos);
            float distWeight = saturate(1.0 - dist / (gridCellSize * 2.0));

            // Normal compatibility weight
            float normalWeight = saturate(dot(normal, sN));

            float w = distWeight * normalWeight;
            indirectLight += s.irradiance * w;
            totalWeight += w;
        }
    }

    if (totalWeight > 0.001)
        indirectLight /= totalWeight;

    // Apply: indirect contribution modulated by albedo
    float3 gi = indirectLight * albedo;
    g_Output[pixel] = float4(gi, 1.0);
}
```

---

## 6. Ray Tracing Strategy Options

For the simplified prototype, choose ONE:

### Option A: Screen-Space Ray March (Recommended for Day 1-2)

```hlsl
bool screenSpaceRayMarch(float3 origin, float3 dir, out float3 hitPos, out float2 hitUV)
{
    const int MAX_STEPS = 64;
    const float STEP_SIZE = 0.05; // World-space step

    float3 pos = origin + dir * 0.01; // Small offset to avoid self-hit

    for (int i = 0; i < MAX_STEPS; i++)
    {
        pos += dir * STEP_SIZE;

        // Project to screen
        float4 clip = mul(viewProj, float4(pos, 1.0));
        float2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        uv.y = 1.0 - uv.y;

        // Out of screen?
        if (any(uv < 0.0) || any(uv > 1.0))
            return false;

        // Compare depths
        float sceneDepth = linearizeDepth(g_Depth.SampleLevel(samplerPoint, uv, 0));
        float rayDepth = clip.w; // Linear depth of ray position

        if (rayDepth > sceneDepth && rayDepth < sceneDepth + STEP_SIZE * 2.0)
        {
            hitPos = pos;
            hitUV = uv;
            return true;
        }
    }
    return false;
}
```

**Pros:** No extra data structures. Works with any renderer.  
**Cons:** Misses offscreen geometry. Limited range.

### Option B: SDF Ray March (Best quality, more setup)

```hlsl
// Requires a distance field volume texture
bool sdfRayMarch(float3 origin, float3 dir, out float3 hitPos)
{
    float t = 0.01;
    for (int i = 0; i < 64; i++)
    {
        float3 p = origin + dir * t;
        float d = sampleSDF(p); // Sample 3D distance field texture
        if (d < 0.001)
        {
            hitPos = p;
            return true;
        }
        t += max(d, 0.01);
        if (t > rayMaxDistance) break;
    }
    return false;
}
```

### Option C: Voxel Grid Ray March (Middle ground)

Voxelize the scene into a 3D occupancy grid, then ray march through it. Good balance of quality and simplicity.

---

## 7. Day-by-Day Schedule

### Day 1: Foundation

- [ ] Set up compute shader pipeline (dispatch, UAV binding)
- [ ] Implement G-Buffer pass (if not already in toy renderer)
- [ ] Create surfel buffer (RWStructuredBuffer) and grid buffer
- [ ] Implement Pass 2: Surfel Spawn
- [ ] **Milestone:** Visualize surfel positions as debug points

### Day 2: Ray Tracing

- [ ] Implement screen-space ray march helper
- [ ] Implement Pass 3: Surfel Lighting (single ray per surfel first)
- [ ] Add temporal accumulation (EMA)
- [ ] **Milestone:** Surfels showing color from traced rays (debug viz)

### Day 3: Spatial Grid + Apply

- [ ] Implement Pass 4: Grid Build (clear + hash insert)
- [ ] Implement Pass 5: Apply GI (grid lookup + weighted blend)
- [ ] Integrate: add GI output to final composite
- [ ] **Milestone:** First visible indirect lighting on screen!

### Day 4: Quality & Tuning

- [ ] Increase ray count (1 → 4 per surfel)
- [ ] Add cosine hemisphere importance sampling
- [ ] Tune parameters (grid cell size, temporal blend, surfel radius)
- [ ] Fix light leaking (normal compatibility weight, distance falloff)
- [ ] Add surfel debug visualization mode (show irradiance as colored spheres)
- [ ] **Milestone:** Visually plausible color bleeding (red wall → white object)

### Day 5: Polish & Demo

- [ ] Set up Cornell box demo scene
- [ ] Add imgui/debug UI for parameter tweaking
- [ ] Capture before/after screenshots (direct only vs. direct + GI)
- [ ] Write brief technical notes on what you learned
- [ ] **Stretch:** Try multi-bounce (feed GI output back as input next frame)
- [ ] **Milestone:** Demo-ready result with clear visual improvement

---

## 8. Helper Functions

### PCG Hash (Fast GPU random)

```hlsl
uint pcgHash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float randomFloat(uint seed)
{
    return float(pcgHash(seed)) / 4294967295.0;
}
```

### Cosine-Weighted Hemisphere Sampling

```hlsl
float3 cosineHemisphereSample(float3 normal, uint seed)
{
    float u1 = randomFloat(seed);
    float u2 = randomFloat(seed + 1);

    float r = sqrt(u1);
    float theta = 2.0 * 3.14159265 * u2;

    // Sample in tangent space
    float3 localDir = float3(r * cos(theta), r * sin(theta), sqrt(1.0 - u1));

    // Build TBN from normal
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    return normalize(tangent * localDir.x + bitangent * localDir.y + normal * localDir.z);
}
```

### World Position Reconstruction

```hlsl
float3 reconstructWorldPos(uint2 pixel, float depth, float4x4 invViewProj)
{
    float2 uv = (float2(pixel) + 0.5) / float2(screenWidth, screenHeight);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 clipPos = float4(ndc, depth, 1.0);
    float4 worldPos = mul(invViewProj, clipPos);
    return worldPos.xyz / worldPos.w;
}
```

### Octahedral Normal Encoding

```hlsl
uint encodeOctahedral(float3 n)
{
    float2 p = n.xy * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));
    if (n.z <= 0.0)
        p = (1.0 - abs(p.yx)) * sign(p);
    // Pack to 16-bit per component
    uint2 packed = uint2((p * 0.5 + 0.5) * 65535.0);
    return packed.x | (packed.y << 16);
}

float3 decodeOctahedral(uint packed)
{
    float2 p = float2(packed & 0xFFFF, packed >> 16) / 65535.0;
    p = p * 2.0 - 1.0;
    float3 n = float3(p.xy, 1.0 - abs(p.x) - abs(p.y));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}
```

### Grid Helpers

```hlsl
int3 worldToGrid(float3 worldPos, float3 gridOrigin, float gridCellSize)
{
    return int3(floor((worldPos - gridOrigin) / gridCellSize));
}

uint flattenGridIndex(int3 cell, int3 dims)
{
    return uint(cell.x + cell.y * dims.x + cell.z * dims.x * dims.y);
}
```

---

## 9. Parameter Tuning Guide

| Parameter | Suggested Start | Effect of Increase | Effect of Decrease |
|-----------|----------------|-------------------|-------------------|
| `maxSurfels` | 65536 | More coverage, slower | Gaps in lighting |
| `gridCellSize` | 0.5m | Smoother but blurrier | Sharper but noisy |
| `N_RAYS` | 4 | Less noise, slower | More noise, faster |
| `temporalBlend` | 0.05 | Faster convergence, flicker | Smoother, slower update |
| `surfelRadius` | 0.1m | More overlap coverage | Gaps between surfels |
| `rayMaxDistance` | 10m | Catches distant bounces | Misses far surfaces |
| `MAX_STEPS` (ray march) | 64 | More hits, slower | Misses geometry |

---

## 10. Common Pitfalls & Solutions

| Problem | Cause | Fix |
|---------|-------|-----|
| Black scene | Surfels not spawning | Debug viz surfel positions; check depth read |
| Light leaking through walls | Grid too coarse, no occlusion check | Decrease `gridCellSize`; add normal compatibility weight |
| Flickering | `temporalBlend` too high | Reduce to 0.02-0.05; increase ray count |
| Banding artifacts | Low surfel density | Increase `maxSurfels`; spatial blur pass |
| Self-illumination | Ray starts inside surface | Offset ray origin along normal |
| Performance tanked | Too many rays | Use 1-2 rays + temporal accumulation |

---

## 11. Extensions (Stretch Goals)

Once the basic system works, these additions are each ~2-4 hours:

1. **Multi-bounce:** Feed previous frame's GI back into surfel lighting → instant second bounce
2. **Emissive surfaces:** Detect emissive materials in G-Buffer, spawn "emissive surfels" with pre-set irradiance
3. **Dynamic objects:** Re-spawn surfels each frame (no persistence) for moving geometry
4. **Probe fallback:** For areas with no surfels, fall back to ambient cube or SH probe
5. **Half-res tracing:** Trace at half resolution, bilateral upscale for 4x perf gain

---

## 12. How This Maps to Production GIBS

| Our Pass | GIBS Equivalent | What production adds |
|----------|----------------|---------------------|
| Pass 2: Spawn | `SurfelGiSpawnSurfels` | Multi-resolution clipmap, recycling/aging, coverage analysis |
| Pass 3: Light | `SurfelGiTraceSurfels` | Hardware RT, light tree sampling, reservoir sampling |
| Pass 4: Grid | `SurfelGiUpdateClipmapProbes` | Hierarchical clipmap, SH storage, irradiance probe interpolation |
| Pass 5: Apply | `SurfelGiApplyToScreen` | Half-res splatting, bilateral filter, multi-pass denoise, BRDF integration |
| — | `SurfelGiIntegrate` | Multi-bounce cascading, validation passes, quality metrics |
| — | `SurfelGiDebug*` | 15+ debug visualization modes |

---

## 13. Minimum Viable Demo Scene

**Cornell Box:**
- White floor, ceiling, back wall
- Red left wall, green right wall  
- 1-2 white boxes on the floor
- Single white directional light from above

**Expected result:** Red/green color bleeding onto white surfaces near colored walls. This is the classic proof that GI is working.

---

## 14. Success Criteria

- [ ] Indirect light visibly colors surfaces (color bleeding)
- [ ] GI responds to scene changes within a few frames (temporal convergence)
- [ ] Performance: > 30 FPS at 1080p on discrete GPU
- [ ] No catastrophic light leaking through solid walls
- [ ] Debug visualization shows surfel placement and irradiance values
- [ ] Clear before/after comparison (direct only vs. direct + GI)

---

## 15. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Screen-space ray march too limited | High | Medium | Day 4: switch to SDF if needed |
| Surfel density too low for scene | Medium | Medium | Increase maxSurfels; adaptive spawn |
| Temporal accumulation too slow | Low | Low | Reduce blend factor; more rays |
| Grid memory too large | Low | Medium | Use hash map instead of dense grid |
| Unfamiliar with compute shaders | Medium | High | Day 1 focused on just dispatch + buffer binding |

---

## 16. References

- **Frostbite GIBS source:** `Code/Engine/World/Render/SurfelGi/` (FrameGraph passes, GPU compute pipeline)
- **Key files to study:** `SurfelGiRender.cpp`, `SurfelGi.build` (module structure)
- **Academic:** "Real-Time Global Illumination using Precomputed Light Field Probes" (McGuire et al.)
- **GDC:** "Global Illumination Based on Surfels" (SIGGRAPH 2021, Lumen approach shares principles)
- **Tutorial:** "Ray Tracing in One Weekend" (basic ray-surface intersection)
- **Technique:** "Stochastic Screen-Space Reflections" (SSR march technique reusable for GI rays)
