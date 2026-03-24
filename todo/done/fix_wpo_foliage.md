# [RESOLVED] UE 5.7 Nanite Foliage Migration & Troubleshooting

**Status**: FIXED.
**Confirmed**: The critical error "WPO Foliage not working nanite" is resolved. Nanite Foliage with World Position Offset (WPO) is now **fully functional** and rendering correctly without errors.

The previous issues were caused by incorrect settings on small foliage assets.

## Top 3 Fixes for 5.7 Migration

### 1. "Preserve Area" Glitches (New in 5.7)
**Note**: The critical error "WPO Foliage not working nanite" disappeared **before** applying this fix. This setting only addresses visual artifacts (spikes/holes).
If your foliage has spikes, dark triangles, or disappears at a distance:

**Cause**: The "Preserve Area" Nanite attribute (often enabled by default on 5.7 foliage) is bugged or overly aggressive on thin geometry like leaves.

**Fix / Trade-off**:
*   **Check "Preserve Area" behavior**:
    *   **Enabled (Default)**: Helps visibility directly, but can distort/trim the "crown" shape of complex trees.
    *   **Disabled**: Preserves the original silhouette better close up, but trees become **thin** and sparse at a distance.
*   **Recommendation**: If "Disabled" makes trees too thin, try keeping it **Enabled** but increasing **Max Edge Length Factor** (1.5 - 2.0) to compensate for the shape distortion.

### 2. Lumen/Ray Tracing Ignores WPO
If your trees move but Lighting/Reflections are static (or crashes occur):

**Cause**: A known regression where Lumen ignores WPO on Foliage Types if specific settings are on.

**Fix**:
* **Option B**: Run console command: `r.RayTracing.Geometry.InstancedStaticMeshes.EvaluateWPO 0`
* **Note**: This trades accurate shadow movement for stability/functioning lighting.

### 3. Settings Reset during Migration
Check these specific settings that often reset to "0" or "Off" during upgrade:

* **WPO Disable Distance**: Check your Foliage Type settings. If World Position Offset Disable Distance is set to a low value (or 0 with a hard cutoff behavior), WPO effectively turns off.
* **Max World Position Offset Displacement**: In the Static Mesh Settings, if this is 0, Nanite might cull the "moved" vertices because it thinks they are out of bounds. Set this to 50-100 (or match your max wind displacement).

## Other 5.7 Specifics (Non-PCG)

### "New" Nanite Foliage System
Even without PCG, UE 5.7 tries to use a Voxelized approach for far distances.
If you see "blocky" trees at a distance, this is the new system.

* **Legacy Mode**: If the new system is breaking your art style, check if you can revert the Foliage Actor to use standard Instanced Static Meshes instead of the new "Foliage Assembly" if that got auto-applied.

### Shadows Disappearing (Regression from 5.6)
If shadows are missing entirely:

* **Cause**: This is the UE-314241 bug.
* **Workaround**: You might have to disable **Evaluate WPO** entirely on the Mesh instance to get shadows back, until a hotfix patches this.

## Verification Results (Passed)
User confirmed WPO is working using:
1.  **Nanite Visualizer**: Shows WPO activity.
2.  **Wind Intensity**: Increasing `MLI_Wind` intensity results in visible movement, confirming the pipeline is valid.

3.  **Lit Mode Check**: Simply look at the edges of the leaves in Lit mode to verify movement.

### Check Wind Intensity (MLI_Wind)
Even if WPO is technically working, low wind intensity might make it invisible at a distance.
* **Symptom**: Trees work close up but look static far away.
* **Check**: Increase **MLI_Wind** intensity. Low intensity values are often imperceptible at large distances, making the foliage appear static even when WPO is active.

## Summary Checklist
- [ ] Uncheck **Preserve Area** on the Nanite Mesh.
- [ ] Set `r.RayTracing.Geometry.InstancedStaticMeshes.EvaluateWPO = 0`.
- [ ] Verify **Max World Position Offset Displacement** > 0 on the Mesh.

## Performance Optimization
For fixing **FPS drops** and **VRAM errors** related to Ray Tracing on foliage:
**Read Guide:** [docs/optimization/foliage_raytracing.md](file:///<user-home>/Documents/GitHub/Alis/docs/optimization/foliage_raytracing.md)