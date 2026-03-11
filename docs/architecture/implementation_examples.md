# Implementation Examples

> See: [../architecture/boot_chain.md](../architecture/boot_chain.md) for the boot flow and `../architecture/diagrams/workspace.dsl` for C4 views. Implementation stays in plugins; docs reference class.method only.

Concrete walkthroughs showing how to add new gameplay content using the data-driven architecture. Each section now includes ten real-world scenarios where the pattern shines.

---

## 0. Immutable Foundation - ABI references (no inline code)

Use these references instead of copying code into docs.

- ABI types: `FBootContext` in `Plugins/Boot/Orchestrator/Source/OrchestratorCore/Public/OrchestratorAPI.h`.
- ABI entry (for BootROM compatibility): `IOrchestratorModule::Start` and `IOrchestratorModule::Stop` in the same header.
- Runtime entry (current flow): `FOrchestratorCoreModule::StartupModule` -> `Initialize` -> `LoadFeatureModules` in `Plugins/Boot/Orchestrator/Source/OrchestratorCore/Private/OrchestratorCoreModule.cpp`.
- Feature boot hook: `IProjectFeatureModule::BootInitialize` in `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IProjectFeatureModule.h`.
- Boot events for UI/telemetry: `UOrchestratorBootEvents::Initialize` in `Plugins/Boot/Orchestrator/Source/OrchestratorCore/Public/OrchestratorBootLibrary.h`.
- Launcher responsibilities and manifest/state formats: `docs/tools/launcher/architecture.md`, `docs/architecture/boot_chain.md`, and `Plugins/Boot/Orchestrator/README.md`.

---

## 1. Add a New World Partition Map
- **Manual (editor)**: Build the root World Partition map once (e.g., `/Game/Project/Worlds/AlisWorld`). Keep it minimal; streamable cells contain the heavy content.
- **Data**: Create a `ProjectWorld` manifest under `/Game/Project/Data/Worlds/`. Set `WorldRoot` to the map, then add regions with the Blueprint helper `MakeRegionDescriptor` (e.g., `Kazan_Outskirts`, `MetroHub`).
- **Automation (future)**: When `GenerateManifestFromWorldPartition` is finished (`todo/create_world.md`), run it to auto-fill region descriptors. Until then, the Blueprint helper keeps authoring lightweight.

**Example Scenarios**
1. Main Campaign World - baseline open world streamed by default.
2. Seasonal Snow Variant - alternate lighting/landscape layers swapped in via a new manifest.
3. Tutorial Island - isolated onboarding space with fewer streamed regions.
4. Performance Benchmark World - QA-only map with scripted streaming for perf tests.
5. Night-Only World - identical layout but light profiles swapped for night events.
6. Platform Lite World - reduced foliage on low-power platforms while keeping the same experiences.
7. Event Playground - temporary world for live events without touching the main map.
8. Cinematic World - barebones set used for cutscenes, loaded via the same pipeline.
9. Feature Prototype World - engineers spin up a sandbox region to test new systems.
10. Analytics Test World - region layout tailored to validate telemetry dashboards.

## 2. Define a Gameplay Experience
- **Create the asset**: add a `ProjectExperience` file to `/Game/Project/Data/Experiences/`.
- **Wire gameplay logic**: set `GameModeClass` (plus optional pawn). A single world can now run Story, Free Explore, or Limited Event simply by swapping the experience reference.
- **Declare dependencies**: populate `FeatureDependencies` (`CombatSystem`, `DialogueSystem`, etc.). When the loader sees `ProjectExperience:StoryCampaign`, it activates those systems automatically.
- **Tune behaviour**: use the `CustomSettings` map for per-experience tweaks (difficulty presets, matchmaking rules, UI themes). Code reads these keys instead of hardcoding values.
- **Designer workflow**: duplicate an experience asset, rename it to `ProjectExperience:LimitedEvent`, point `GameModeClass` to an event-specific subclass, add `FeatureDependencies = { "Events", "CombatSystem" }`, and set `CustomSettings` such as `EventDurationMinutes=45`.
- **Engineer workflow**: in boot or menu UI, present a list of `ProjectExperience` assets. When the player picks one, pass its ID into `FProjectLoadRequest`. No enums or switch statements required.
- **Refactor win**: when a system moves to a new plugin, update the manifest dependencies rather than touching game code.
- **End result**: the loading pipeline always consumes `ProjectExperience:*` IDs, keeping the flow declarative and easy to extend.

**Example Scenarios**
1. Story Campaign - full feature set enabled, cinematic settings applied.
2. Free Explore - same world, combat disabled, dialogue optional for casual wandering.
3. Hardcore Survival - increased enemy spawn rate and permadeath toggle via `CustomSettings`.
4. PvP Arena - swaps game mode to a team-based controller and enables ranking systems.
5. Benchmark Mode - disables UI, turns on telemetry, runs scripted sequences.
6. Tutorial Experience - activates guidance widgets, disables advanced features.
7. Coop Raid - requires PartySystem + CombatSystem, adds raid-specific rules.
8. Limited-Time Event - short-duration modifiers (double loot, unique objectives).
9. Photo Mode Experience - spawns a photo-specific pawn, disables enemies, enables camera features.
10. Narrative Replay - reuses world assets with branching dialogue features toggled on.

## 3. Ship Optional Content Packs
- Create a `ProjectContentPack` data asset for each downloadable pack.
- Fill in mount points or plugin names (`MountPoints` array) and mark whether it is optional.
- Provide `FeatureUnlocks` if the pack enables new systems. The pipeline mounts it before travel when required.

**Example Scenarios**
1. Metro Expansion - new underground regions gated behind a content pack.
2. Weapon Pack - mounts an IOStore with additional gear and unlocks CombatSystem modifiers.
3. Cosmetic Bundle - adds outfits through a lightweight pack enabled on demand.
4. Language Voice Pack - streams localized VO when the player opts in.
5. Soundtrack DLC - mounts new music banks for fans who purchase the upgrade.
6. Seasonal Event Pack - contains temporary meshes/UI for holiday events.
7. Creator Tools Pack - exposes debug widgets for modding or dev builds only.
8. Story Chapter Pack - unlocks new quests by activating related features.
9. Accessibility Pack - mounts alternative UI assets (high-contrast themes, fonts).
10. Benchmark Assets Pack - QA-only high-detail assets for stress testing.

## 4. Surface Worlds in Boot UI Without Blueprint Wiring
- Configure the `ProjectBoot` widget to enumerate `ProjectWorld` assets (asset registry query or helper function).
- Because boot settings live in config, new worlds appear automatically once their manifests exist; no Blueprint rework needed.
- Selecting a world triggers a `FProjectLoadRequest` with `ProjectWorld` plus `ProjectExperience` IDs.

**Example Scenarios**
1. World Picker Grid - list every `ProjectWorld` asset with thumbnails.
2. Mode-Specific Menu - filter worlds by experience tags (Story, Survival, PvP).
3. Platform-Sensitive Options - automatically hide worlds not available on current platform.
4. QA Debug Menu - engineers load sandbox worlds without entering console commands.
5. Seasonal Event Banner - boot UI surfaces the latest event world first.
6. Content Ownership Check - worlds tied to DLC appear only when the content pack is owned.
7. Tutorial Prompt - first-time players see only the tutorial world until completion.
8. Benchmark Shortcut - QA builds load directly into the benchmark world/experience pair.
9. Custom Server Selection - dedicated servers expose world manifests representing shards.
10. Accessibility Profile - players with accessibility pack see curated world suggestions.

## 5. Track Session Milestones with Data Awareness
- Call `UProjectSessionSubsystem::RecordMilestone("World_Completed")` (or similar) from gameplay code when a milestone is reached.
- The subsystem stores milestones, and placeholders (`SyncMilestoneToProjectSave`, `QueueAnalyticsEvent`) will push data to save/analytics once those services arrive.
- Since worlds and experiences live in data assets, milestone IDs can follow the same naming (`World:Kazan_Outskirts::Completed`).

**Example Scenarios**
1. Boot Complete - fire when the boot UI finishes loading resources.
2. Tutorial Finished - milestone ties to tutorial world/experience for analytics.
3. First Combat Encounter - triggered when CombatSystem activates for a player.
4. Region Entered - record `World:MetroHub::Enter` for heatmap analytics.
5. Raid Victory - milestone for completing the coop experience.
6. DLC Installed - track when a content pack becomes active in session.
7. Photo Mode Session - mark start/end for optional features usage stats.
8. Benchmark Run Completed - inform QA dashboards when scripted tests finish.
9. Session Timeout - note when players idle out to improve retention tracking.
10. Narrative Branch Chosen - log which experience setting the player selected.
