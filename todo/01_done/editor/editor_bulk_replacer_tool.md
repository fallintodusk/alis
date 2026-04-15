# Bulk Replacer Tool Architecture

**Tool Name**: Project Bulk Replacer
**Plugin Location**: `Plugins/Editor/ProjectPlacementEditor`

## Overview
A tool to automate the mass replacement of actors (e.g., deprecated Blueprints) with new Project Assets (DataAssets), preserving world positions and scaling/rotating correctly according to the new definitions.

## Justification for Location
We place this in **`ProjectPlacementEditor`** because:
1.  **Dependencies**: It relies on the same factories (`ProjectObjectActorFactory`, `ProjectPickupItemActorFactory`) as the manual Placement tool.
2.  **Logic**: It shares the "Spawn at Transform" logic with the "Copy/Paste" tool.
3.  **UI**: It can be a new **Tab** in the existing "Project Placement" panel, offering a unified "Level Editing" workspace.
    *   Tab 1: Place New (Manual)
    *   Tab 2: Bulk Replace (Automated)

    *   Tab 1: Place New (Manual)
    *   Tab 2: Bulk Replace (Automated)

## Foundations & Related Documentation
This tool uses the native Unreal Engine spawning mechanisms, but automates the selection and matching process.

*   **Manual Replacement**: Handled by native UE "Replace Selected Actors with..." (Right-click in Viewport).
*   **Automated Replacement**: Handled by this **Bulk Replacer** tool.
*   **Shared Logic**: This tool will implement its own `SpawnActorFromFactory` function (similar to the deprecated [Copy Tool architecture](editor_project_copy_selected_transform.md)) to handle Data Asset spawning programmatically.

## Agentic Architecture (Modular)

We will structure this tool as a pipeline of 3 distinct modules. This ensures that an AI Agent (or human) can intervene at any stage.

### Module 1: The Scanner (Observation)
**Responsibility**: "See the world and its properties."
*   **Input**: Selection / Level / Folder path.
*   **Output**: `FWorldActorInfo` - Deep Scan:
    *   **Basic**: Class, Location, Rotation, Scale, Tags, Name.
    *   **Mechanical Hints**: List of Components (e.g. `UBoxComponent`, `UDestuctibleMesh`), Applied Materials.
    *   **Interfaces**: Implemented Interfaces (e.g. `BPI_Interactable`).
    *   **Physics**: Mobility state (Static/Movable), Collision presets.
*   **Agentic Role**: The Agent receives this "Rich Data" to understand not just *where* it is, but *what* it might be physically.

### Module 2: The Matcher (Decision & Semantics)
**Responsibility**: "Understand mechanisms and decide."
*   **Input**: `FWorldActorInfo` (Rich Data) + Data Asset Repository (Capabilities).
*   **Advanced Logic**:
    *   **Capability Check (JSON Source of Truth)**: 
        *   The Matcher inspects the **JSON Definition** of available Data Assets.
        *   It looks for specific keys/components in the JSON (e.g., `SpringRotatorComponent`, `SlidingComponent`) to confirm the asset supports the required mechanism.
    *   **Mechanical Matching (The "Theory" Check)**: 
        *   **Goal**: Identify ALL objects that *should* move (theoretically) and give them the real Capability.
        *   "Source is named 'Drawer'?" (Theory: It slides) -> "Target MUST have `SlidingComponent`."
        *   "Source is named 'CabinetDoor'?" (Theory: It rotates) -> "Target MUST have `SpringRotatorComponent`."
        *   "Source has `HingeComponent`?" -> "Target MUST have `HingedOpenable` Capability."
    *   **Visual Matching**: "Source uses `M_Glass`?" -> "Prefer Target `Door_Glass_DataAsset`."
    *   **Gameplay Logic Injection**:
        *   "Source looks flimsy?" (Material/Name check) + "Context is 'Entrance'?" -> 
        *   **Action**: Select DataAsset with `Breakable` or `Lockpickable` capability.
        *   **Reasoning**: "Players expect to force entry here (crowbar/key)."
    *   **State Rules (Randomization)**:
        *   Can attach rules like "Randomize 'OpenState' between 0.0 and 1.0".
        *   "If context is 'AbandonedHouse', OpenState = 0.8 (Ajar)".
        *   "If context is 'SecureLab', OpenState = 0.0 (Closed)".
*   **Output**: `FReplacementPlan` (List of { ActorID, AssetID, **PropertyOverrides** }).
*   **Agentic Role**: The Agent acts as an expert level designer, ensuring that the *behavior* of the new object matches the *implied behavior* of the old one. It guarantees the new Data Asset has the required **Capability** (e.g. `SpringRotator`) and sets **Variations**.

### Module 3: The Executor (Action)
**Responsibility**: "Make it happen."
*   **Input**: `FReplacementPlan`.
*   **Logic**:
    *   Iterates the plan.
    *   Calls `SpawnActorFromFactory(AssetID, Location)`.
    *   **Apply Overrides**: Sets properties on the new actor (e.g. `Door->SetOpenState(0.5)`).
    *   Destroys the old actor.
*   **Output**: Updated Level.

#### Technical Implementation of Spawning (Executor)
*   **Mechanism**: Custom helper function `SpawnActorFromFactory`.
*   **Why**: Native "Replace Selected" cannot be called via API, so we replicate its logic.
*   **Logic**:
    1.  Load the Target Asset (`UObjectDefinition`, `UItemDefinition`, etc).
    2.  Iterate `GEditor->ActorFactories`.
    3.  Find best factory: `Factory->CanCreateActorFrom(Asset)`.
    4.  Spawn: `Factory->CreateActor(Asset, Location, Rotation)`.
    5.  **SOLID Compliance**: This ensures we support ANY asset type that has a registered factory (e.g., `ProjectObjectActorFactory`), without hardcoding types.

## Integration
*   The **Copy/Paste Transform Tool** (already designed) effectively covers **Module 3 (Executor)** for single-item manual cases.
*   The **Bulk Replacer** adds Module 1 and 2 to automate feeding Module 3.
