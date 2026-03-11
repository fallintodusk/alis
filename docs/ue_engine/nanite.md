# Nanite Triangle Collapse & Distance Settings

Nanite does not use traditional "Distance" based LODs. Instead, it uses **Screen Space Error** (pixels) to decide when to collapse triangles. However, you can tweak the aggressiveness of this collapse globally or per-mesh.

## 1. Controlling "Collapse Distance" (Global)

Since Nanite is based on screen pixels, you control strictly "how small a triangle can look on screen before it collapses".

### `r.Nanite.MaxPixelsPerEdge` (Default: 1.0)
This is the main quality knob. It defines the target size of a triangle in pixels.
*   **Lower (e.g., 0.5)**: Triangles are allowed to be smaller. Nanite will render **more triangles** at further distances. "High Quality".
*   **Higher (e.g., 2.0 - 4.0)**: Triangles must be larger. Nanite will **collapse/simplify sooner** as you move away. "High Performance".

### `r.Nanite.ViewMeshLODBias.Offset` (Default: 0)
Biases the selection of the cluster level.
*   **Negative (e.g., -1, -2)**: Forces Nanite to use higher-detail clusters than it normally would at that distance.
*   **Positive (e.g., 1, 2)**: Forces Nanite to use lower-detail clusters (more collapsed) at that distance.

---

## 2. Controlling "Number of Segments" (Per Mesh)

To control the actual density of the Nanite mesh itself (how many segments/triangles it *has* to work with), you adjust the **Nanite Settings** inside the **Static Mesh Editor**.

### Trim Relative Error (Default: 0.04)
This controls how much "mistake" is allowed when compressing the mesh.
*   **Lower (e.g., 0.01 - 0.02)**: Keeps more segments/triangles. Harder to collapse. Mesh size on disk increases.
*   **Higher (e.g., 0.1 - 0.2)**: Collapses segments aggressively. Good for distant background objects.

### Keep Triangle Percent (Fallbacks)
This only affects the **Fallback Mesh** (what is used for collisions or when Nanite is disabled/unsupported), not the Nanite rendering itself.

---

## 3. Statistics & Profiling

To see detailed realtime statistics about what Nanite is actually rendering:

### `r.NaniteStats`
Prints detailed statistics to the output log (and sometimes screen depending on args) including:
*   **Total Triangles**: How many triangles are actually being drawn.
*   **Clusters**: Number of active Nanite clusters.
*   **Instances**: Number of Nanite object instances visible.
*   **Views**: Main view vs shadow views.

Use this to verify if your "Distance" tweaks (like `MaxPixelsPerEdge`) are actually reducing the triangle count as expected.

---

## 4. Summary of Workflow

1.  **To visualize collapse**:
    *   `r.Nanite.Visualize Triangles`
    *   `r.Nanite.Visualize Clusters`
2.  **To check stats**:
    *   `r.NaniteStats` (Check number of triangles/clusters)
3.  **To tweak "Distance" globally**:
    *   Use `r.Nanite.MaxPixelsPerEdge` to change how fast things simplify as they get small on screen.
4.  **To tweak specific objects**:
    *   Currently, Nanite does not support per-actor "Distance Multipliers" easily without using `Displacement` or material tricks, but you can adjust `Trim Relative Error` on the Asset to force it to be "heavier" or "lighter" overall.
