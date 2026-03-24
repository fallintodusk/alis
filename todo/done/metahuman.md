# MetaHuman 5.7: Assembly & Architecture

**Goal:** Generate optimized, game-ready characters (<100MB) without manual cleanup.

## The "UE Optimized" Pipeline (Assembly Tool)
This is the native UE 5.7 workflow. It bakes heavy source assets into lightweight game assets.

### 1. Preparation
*   **Web:** Open MetaHuman Creator.
*   **Action:** Click **"Generate"** for your character (Wait for "Complete").

### 2. Assembly (The "Golden Ratio" Configuration)
To achieve AAA quality at mobile sizes (~50MB), we use the **High** preset but manually override textures.

1.  **Open:** `Window` -> `MetaHuman` -> **Assembly Tool**.
2.  **Asset Type:** Select **"UE Optimized"**.
3.  **Preset:** Select **"High"** (Generic presets like "Low" strip Physics/RigLogic - do not use them).
4.  **Texture Overrides (Crucial Step):**
    *   **Face:** Set Albedo/Normal to **1024** (or lower).
        *   *Reason:* We use a **Modular Head Pipeline**. Heads are baked separately, optimized, and attached to the Neck bone at runtime. We support various archetypes (Muscular Hero, Old Man, Soldiers, etc.).
    *   **Body:** Set Albedo/Normal to **2048** (High Quality).
        *   *Reason:* **True First Person View.** The camera mimics real eyes; the hero sees their own legs, arms, shoulders, and torso. Body and clothing must be high fidelity.
5.  **Execute:**
    *   [x] Check **"Physics Asset"** (Enabled).
    *   Click **Assemble**.

### 3. Result
*   **Location:** `Content/MetaHumans/[Name]`
*   **Tech Specs:**
    *   Textures baked & capped at **2K**.
    *   AnimBP replaced by light **RigLogic** (~0.4ms).
    *   Skeleton stripped to **~260 bones** (Low).
    *   Hair converted to **Cards** (Low).


## 4. Master Character Architecture (RTX 3060 Target)
This section defines the **Universal Entity Pattern** used for the entire project.

### A. Universal Skeleton Tiers
We use a **Modular Head Pipeline**. The Body is the master skeleton; the Head is a separate mesh attached to the `neck_01` bone at runtime.
*   **Tier A (Hero/Cinematic):** Full Body Skeleton + Separate High-Res Head (baked & optimized).
    *   *Note:* Supports various archetypes (Muscular, Old, Female, Soldier) via swappable head/body meshes.
*   **Tier B (Gameplay NPC):** Merged Mesh or Reduced Skeleton.
*   **Tier C (Crowd):** Proxy Skeleton. Basic Biped only.

### B. Animation Architecture
*   **Template AnimBP:** Use `ABP_Humanoid_Template`. Logic (Idle->Run) is abstract and shared.
*   **Linked Anim Layers:** Combat Logic is **not** in the main graph. We use `LinkAnimClassLayers` to dynamically inject weapon animations (e.g., `ABP_Rifle_Layer`) only when equipped. Hard references to animations are forbidden.

### C. Physics tiers (Chaos Cloth & Flesh)
*   **Tier A (Hero):** Full Chaos Cloth (Self-Collision) + Chaos Flesh (Muscle Sim).
*   **Tier B (Near NPC):** Kinetic Colliders only (No Self-Collision). ML Deformer instead of Flesh.
*   **Tier C (Far NPC):** Static Mesh or simple Rigid Body. No Cloth simulation.

### D. Rendering Optimization (RTX 3060 Profile)
*   **Nanite Constraint:** **DISABLE World Position Offset (WPO)** on standard clothing. WPO breaks Nanite efficiency. Use *Chaos Cloth* for movement instead.
*   **Shadows (VSM):** MetaHuman micro-movements invalidate shadow cache.
    *   *Fix:* Set `r.Shadow.Virtual.Cache.InvalidationThreshold 2.0`. This ignores micro-jittering, saving massive GPU performance.
*   **Lumen Strategy:** Use **Software Ray Tracing (Hybrid)**.
    *   *Global Illumination:* SWRT (Mesh Distance Fields).
    *   *Reflections:* HWRT (Hardware) only for very glossy surfaces, otherwise SWRT.

## 5. Fast Path Strategy (Solo/Small Team)
**Context:** The only way to achieve AAA quality with limited time is a **Code-Driven Pipeline**.

### A. C++ over Blueprints
*   **Strategy:** Use a `UniversalCharacter` C++ base class. Write "Equip" logic **once** in C++.

### B. Python Automation
*   **Strategy:** Write `OptimizeMetaHuman.py` to auto-import and configure assets in seconds.

### C. Data Assets over Hard References
*   **Strategy:** Use `PrimaryDataAsset`. The game only loads what is worn.

### D. Resource Reuse (The Flyweight Pattern - "Golden Ratio")
*   **MetaHuman Wardrobe Prototype:** Usage of separate `procedural_wardrobe_architecture.md`.
*   **Optimization Strategy:**
    *   **Occlusion Culling:** We do **NOT** draw triangles under clothing. The wardrobe system masks/hides the body geometry beneath layers to save performance.
    *   **Skeleton Adaptation:** Clothes automatically conform to the skeleton's movement and shape.
    *   **1 Asset Source:** One standard mesh (e.g., `T_Shirt_Med`) serves all 18 body types via runtime resizing (Dataflow).

## 6. Next Steps: Wardrobe Implementation
Now that the Base Character and Architecture are defined, see **[Procedural Wardrobe Architecture](procedural_wardrobe_architecture.md)** for:
*   **MetaHuman Wardrobe Prototype** details.
*   Logic of equipping items (Component Model, Data Assets).
*   Procedural resizing/fitting mechanics.
