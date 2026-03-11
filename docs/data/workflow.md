# Data-Driven Agent Workflow (Zero-Restart Friendly)

This note distills the patterns we expect agents to use when tweaking gameplay/data without touching `.uasset` binaries. It aligns with the ProjectData + ProjectSettings architecture and the upcoming multiplayer/telemetry work.

---

## What to keep outside `.uasset`

- **Tuning knobs** ? stamina drain, damage multipliers, XP curves, match timers.
- **Feature flags** ? enable/disable systems, backend endpoints, EOS/Fab URLs, CDN switches.
- **Quest / dialog / UI copy** ? localized strings, branching dialog text, menu tooltips.
- **Loot tables / spawn sets** ? item identifiers, weights, level ranges, content-pack availability.
- **Procedural params** ? seeds, Perlin/noise values, biome thresholds, day-night curves.
- **Server lists / CDN maps** ? hostnames, regions, lobby weights, Zen stream targets.
- **Localization** ? CSV/PO assets you auto reimport (hooked to the localization dashboard).
- **Gameplay tags** ? keep project-owned tags as native declarations in `ProjectCore` (`ProjectGameplayTags.h/.cpp`); use `Config/DefaultGameplayTags.ini` for engine/plugin/imported tags; expose refresh command.
- **Matchmaking filters** ? JSON descriptors consumed by the multiplayer stack (`ProjectOnline`).
- **Content pack metadata** ? chunk eligibility, auto-download flags, dependency hints (already in `UProjectContentPackManifest`).

> If designers need to tweak it without waiting for a cook or restart, keep it in text + reload hooks.

---

## Three proven patterns (no restart required)

### 1. INI config + `ReloadConfig`

- Store persistent settings under `Config/Alis.ini` (per-module sections via `UDeveloperSettings`).
- Read with `GetDefault<UAlisSettings>()` / `GetMutableDefault`.
- Provide a console command or editor utility that:
  1. Calls `ReloadConfig()` on the settings class.
  2. Broadcasts a settings-changed event (ProjectCore already offers delegate scaffolding).
  3. Applies new values to live systems (stamina manager, UI theme defaults, etc.).
- Agents edit the INI directly; you reload on demand. Works well for:
  - Core tuning (movement, stamina, camera).
  - Multiplayer endpoints (`ProjectOnline` backend selection).
  - Analytics sample rates for the telemetry pipeline.

### 2. JSON / CSV files + live apply

- Keep data under `Project/Data/*.json` for shipped defaults; `Saved/AlisLive/*.json` for hot iteration.
- On subsystem init (ProjectData / ProjectOnline / Procedural manager), parse once, then:
  - Watch directories via `FDirectoryWatcherModule` and reapply on change.
  - Expose a manual ?Reload Data? command if file watching is disabled.
- Packaging:
  - Add `Project/Data` to Project Settings ? Packaging ? ?Additional Non-Asset Directories to Package?.
  - If you need loose copies at runtime (dedicated server or mod support), add to ?Additional Non-Asset Directories to Copy?.
- Ideal for:
  - Loot/spawn tables feeding the content-pack registry.
  - Matchmaking rule sets (filters, weights).
  - Procedural biome configs the world partition loader consumes.

### 3. Console variables (CVAR)

- Define tunables as CVARs: `sg.alis.StaminaDrain`, `mp.alis.MatchmakingTimeout`, etc.
- Set in `.ini` (`Config/Alis/autoexec.cfg`) or at runtime with the console.
- Forward CVAR updates to gameplay systems (e.g., stamina subsystem polls `IConsoleManager` or registers a callback).
- Replicate from server to clients by reading the CVAR and broadcasting via an RPC or subsystem event.

---

## Editor helpers for agents

- **Remote Control API** ? expose specific subsystem properties over HTTP/WebSocket so bots/scripts can tweak without opening the editor.
- **Python + Editor Utility Widget** ? single button to reload JSON/INI; planned inside the `ProjectTools` backlog.
- **Auto Reimport** ? for CSV/JSON feeding DataTables. Keep the text canonical; the `.uasset` DataTable is just a cache.
- **Property change delegates** ? add `OnSettingsChanged` or `OnLootTableReloaded` events so designers don?t touch actors manually.

---

## When it?s truly zero-restart

You can stay fully agentic when:

1. You edit **data**, not C++ types or UPROPERTY layout.
2. Systems consume data via a **runtime registry** (e.g., ProjectData?s manifest manager, ProjectOnline?s session config, or the forthcoming telemetry sink).
3. You provide a **reload channel** (ReloadConfig, JSON reparse, CVAR callback).
4. Actors that depend on the data listen for change events or rerun construction scripts on demand.

If any of these is missing, the edit-before-cook experience degrades quickly.

---

## Minimal folder contract

/Config/Alis.ini                        # Static defaults (agent-editable)
/Config/Alis/autoexec.cfg               # CVAR presets, runtime tuning
/Plugins/<Plugin>/Data/*.json           # Shipping data sets (agent-editable) [See docs/data/structure.md]
/Saved/AlisLive/*.json                  # Hot iteration playground
/Config/DefaultGameplayTags.ini         # Engine/plugin/imported tags (project-owned tags live in ProjectCore native tags)

- Keep directories whitelisted; validate schema before applying (use JSON schema or lightweight checks).
- Sign or checksum JSON if you expect untrusted sources (especially for server/CDN lists).

---

## Project-specific hooks to implement

- **ProjectSettings** ? already reading manifest data assets; add optional JSON overrides for matchmaking availability / download priorities.
- **ProjectOnline subsystem** ? plan a `ProjectOnlineDevSettings` INI and JSON-driven matchmaking rule registry.
- **ProjectLoading** ? provide console command `Project.Loading.ReloadConfig` to reapply stage weights/timeouts.
- **Telemetry/Analytics stack** ? store endpoints + sampling rules in JSON; call `ReloadConfig` on the telemetry subsystem.
- **Editor automation** ? `ProjectTools` module should ship:
  - ?Reload All Data? command (INI + JSON).
  - Validators ensuring JSON/INI meets schema before deployment.
  - Push-button construction script rerun for dependent actors (maps, menus).

By following these patterns, agents can iterate on gameplay, UI, and services instantly, and your runtime stays resilient without recooks or restarts. Keep the pipeline tight, document reload commands in README files, and expose them via console, Python, or Remote Control for maximum leverage.
