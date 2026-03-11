# Optimization: Nanite Foliage & Ray Tracing

**Problem**: Using **Ray Tracing** combined with **Nanite** on small, dense foliage (Grass, Flowers, Small Bushes) causes severe performance degradation and high VRAM usage.

**Symptoms**:
*   FPS drops significantly when looking at fields of grass.

## The Cause
Ray Tracing requires building an **Acceleration Structure (BVH)** in the GPU memory.
*   **Nanite** generates millions of tiny instances (grass blades).
*   If **"Support Ray Tracing"** is ON, the engine allocates VRAM for *each* of these tiny instances to calculate accurate ray-traced shadows and reflections.
*   This leads to massive **Overdraw** and **VRAM saturation**, even though the visual difference on a blade of grass is negligible.

## The Fix

For **Small Foliage** (Grass, Flowers, Bushes, Debris):

1.  Open the **Static Mesh** in the Editor.
2.  Search for **Ray Tracing**.
3.  **Uncheck** `Support Ray Tracing` (or `Visible in Ray Tracing`).
4.  **Save** and **Apply**.

**Result**:
*   The object is removed from the heavy Ray Tracing structure.
*   It is still rendered by Nanite.
*   Shadows fallback to **Virtual Shadow Maps** (which are cheaper and look great for foliage).
*   **VRAM Usage** drops significantly.

## Quick Diagnostic Commands (UE 5.7)
Use these console commands to check if disabling Ray Tracing for foliage improves performance globally before applying changes to individual meshes.

*   **Test**: `r.RayTracing.Geometry.InstancedStaticMeshes.LowScaleCullRadius 5000`
    *   *Effect*: Removes small instanced objects (like grass) from Ray Tracing beyond 50 meters. If FPS increases, apply the fix above.
*   **Full Disable Test**: `r.RayTracing.Geometry.InstancedStaticMeshes.Enabled 0`
    *   *Effect*: Completely disables Ray Tracing for all instanced foliage.

## Best Practices: Ray Tracing Whitelist

Use this rule of thumb to decide which assets need Ray Tracing:

### Enable Ray Tracing For:
*   **Architecture**: Walls, Floors, Ceilings (Critical for Lumen GI).
*   **Large Hero Assets**: Statues, Vehicles, Main Character.
*   **Reflective Shiny Objects**: Mirrors, Wet Surfaces, Metal, Glass.
*   **Large Trees**: Thick trunks and main branches (for accurate casting of large shadows).

### Disable Ray Tracing For:
*   **Dense Ground Cover**: Grass, Weeds, Clovers.
*   **Scatter Assets**: Pebbles, Debris, Papers, Trash.
*   **Distant Foliage**: Background trees on mountains.
*   **Rough/Matte Small Props**: Things that don't have distinct reflections.
