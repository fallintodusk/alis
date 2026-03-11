# Data Asset Knowledge Base & Methodology

This document serves as the **Standard Operating Procedure (SOP)** for converting Blueprint objects into JSON Data Assets.

---

## Golden Rules (The "Law")

1.  **ID = Filename**
    *   The `id` field inside the JSON **MUST** match the filename exactly (case-sensitive, without `.json`).

2.  **Absolute Paths ONLY**
    *   Always use full package paths for assets: `/ProjectObject/HumanMade/...`.
    *   *Why?* Relative paths break icon generation.

3.  **Hierarchy Logic (The Sibling Rule)**
    *   **Independent Moving Parts** (Doors, Drawers, Windows) = **Sibling Roots**. Do NOT parent them to the Frame.
    *   **Attached Parts** (Glass, Handles) = **Child**. Parent them to the specific moving part they belong to.

4.  **Capabilities**
    *   `scope` targets the Mesh ID.
    *   `Hinged` = Rotation.
    *   `Sliding` = Translation (Position).

5.  **Scale MUST be 1.0**
    *   Always set `"scale": [1.0, 1.0, 1.0]` in the JSON.
    *   If the visual size is wrong, fix it in the Mesh (Build Settings) or the Level Actor, but NEVER in the Data Asset.

---

## Workflow: Converting Blueprint to Data Asset (SOP)

### 1. Preparation (The "Bake" Step)
*   **Goal**: Ensure all meshes have `Scale: [1, 1, 1]` and correct Pivot points *before* creating the JSON.
*   **Problem**: If you scale a BP (e.g., to 0.8), the meshes inside are still 1.0. If you bake the parent but not the children, the children will "pop" back to 1.0 (become huge).
*   **Procedure**:
    1.  **Detach Children**: Temporarily unparent doors/drawers (keep World Transform).
    2.  **Bake Parent**: Apply "Bake Transform" (Scale) to the main Body.
    3.  **Bake Children**: Apply "Bake Transform" (Scale) to each Door/Drawer independently.
    4.  **Re-assemble**: Parent them back or just copy their new World Coordinates.

### 2. Gathering Coordinates
*   Once meshes are baked (Scale 1.0):
    1.  Place them loosely in the BP or Level.
    2.  Align them perfectly.
    3.  Copy the **Relative Location** (if parented) or **World Location** (if unparented but placed at World 0,0,0) into the JSON.

---

## Naming Conventions (Standardization)
### Kitchen Sets
**Format**: `KitchenCabinet_[Type]_[Config]_[SetID]`

*   **Prefix**: Always `KitchenCabinet` (indicates usage).
*   **Type**:
    *   `Base` (Floor/Lower cabinets)
    *   `Wall` (Hanging/Upper cabinets)
    *   `Tall` (Full-height pantries/fridges)
*   **Config** (Content/Doors):
    *   `Sink` (Base for sink)
    *   `Corner` (Corner unit)
    *   `OneDoor`, `TwoDoor` (Number of doors)
    *   `OneDoor_Drawer` (Combo)
    *   `ThreeDrawer` (Drawers only)
*   **SetID**:
    *   `Set_1` (Belongs to Kitchen Set 1)
    *   `Set_2` (Belongs to Kitchen Set 2), etc.

**Examples**:
*   `KitchenCabinet_Base_Sink_Set_1`
*   `KitchenCabinet_Wall_TwoDoor_Set_1`
*   `KitchenCabinet_Base_Corner_Set_1`

---

## Troubleshooting & Pitfalls (Read Before Panic)

### 1. "My Object Falls to the Floor when interacting!"
*   **Context**: You used `Sliding` capability on an elevated object (e.g., a drawer at `Z=67`).
*   **Cause**: `OpenPosition` is absolute. If you set it to `(0,20,0)`, the system drives the object to Z=0.
*   **Fix**: **Preserve Z-Height**. Set `OpenPosition` to `(0, 20, 67)`.
*   **Lesson**: ALWAYS define `"ClosedPosition": "(0,0,67)"` to match the starting location.

### 2. "I have Duplicate/Ghost Actors in the level!"
*   **Cause**: You changed the `id` in the JSON after generating it once. The Generator spawns a NEW actor for the new ID.
*   **Fix**: Manually delete the old Actor in the Level.

### 3. "My Icon is Blank/Generic!"
*   **Cause**: You used a relative path (`../Asset`) or the asset path is wrong.
*   **Fix**: Verify the path corresponds to a valid static mesh in the Content Browser.

### 4. "My Door moves the whole Frame!"
*   **Cause**: You parented `door` -> `frame`.
*   **Fix**: Unparent `door`. Make it a proper sibling in the `meshes` array.

### 5. "My Doors clip through the shelves inside!"
*   **Cause**: You set the `OpenAngle` to open *inwards* (e.g., 90) but the furniture has internal shelves.
*   **Fix**: Check the geometry. If there are shelves, doors MUST open **OUTWARDS**. Try flipping the sign (e.g., `-90` instead of `90`).

---

## Recipes & Methodology

### Recipe 1: Simple Hinged (Fridges, Cabinet Doors)
*Use for objects with rotating doors.*

```json
{
    "id": "Refrigerator_2",
    "meshes": [
        { "id": "body", "asset": "..." }, // Static Body
        { "id": "door", "asset": "..." }  // Moving Door (Sibling)
    ],
    "capabilities": [
        { "type": "Hinged", "scope": ["door"], "properties": { "OpenAngle": "90" } }
    ]
}
```

### Recipe 2: Sliding at Height (Drawers, Wardrobes)
*Use for objects that slide. **CRITICALLY IMPORTANT**: Handle Z-height correctly.*

```json
{
    "id": "ToiletTable",
    "meshes": [
        { "id": "body", "asset": "..." },
        {
            "id": "drawer",
            "asset": "...",
            "transform": { "location": [0, 0, 67] } // Starts at Height 67
        }
    ],
    "capabilities": [
        {
            "type": "Sliding",
            "scope": ["drawer"],
            "properties": {
                "ClosedPosition": "(0,0,67)", // Explicitly matches Start
                "OpenPosition": "(0,20,67)",  // Preserves Height while sliding
                "Stiffness": "80"
            }
        }
    ]
}
```

### Recipe 3: Complex Composite (Balconies, Windows)
*Use for objects with multiple independent moving parts and glass.*

**Hierarchy Strategy**:
*   `frame` (Root)
*   `door` (Root) -> `glass` (Child of Door)
*   `window` (Root) -> `glass` (Child of Window)

```json
{
    "id": "BalconyFrame_Complex",
    "meshes": [
        { "id": "frame", "asset": "..." },
        { "id": "door", "asset": "..." },
        { "id": "glass", "parent": "door", "asset": "..." } // Moves with door
    ],
    "capabilities": [
        { "type": "Hinged", "scope": ["door"], "properties": { "OpenAngle": "90" } }
    ]
}
```

---

## Pre-Flight Checklist

1.  [ ] **Schema**: Is the path correct? (check `../../` count).
2.  [ ] **ID**: Does it match the filename?
3.  [ ] **Hierarchy**: Are moving parts Siblings (Roots)?
4.  [ ] **Coords**: If Sliding, did I set `ClosedPosition` to match Z-height?
5.  [ ] **Icon**: Did I restart/regenerate to check the icon?
