# Rendering CVar Profiles (editor-side A/B helpers)

Per-session console-variable batches used during the 2026-05-15 indoor
Lumen tuning session. Tracked in git so the tuning history is reviewable.

**!! READ THE TODO BEFORE USING THESE !!**

Companion todo: [todo/00_current/fix_lumen_indoor_noise.md](../../../../todo/00_current/fix_lumen_indoor_noise.md)

These profiles encode the early experimental CVar campaign. The todo's
final research-grounded plan **superseded** this approach. Specifically:

- `cvar_clean.txt` and `cvar_full.txt` raise
  `r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated` to 32 and
  `r.Lumen.Reflections.Temporal.MaxFramesAccumulated` to 24. These were
  experimentally **confirmed to cause dynamic-shadow / GI ghosting and
  smear** on moving casters (door open, NPC walk). They reduce static
  noise but at the cost of the "shleif" artifact. **Do not ship these
  values.**
- The safe subset that does NOT cause smear is documented in the todo's
  Track A7 (in `DefaultEngine.ini`):
  - `r.Lumen.ScreenProbeGather.MaxRayIntensity=4`
  - `r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing=1`
  - `r.Lumen.Reflections.RoughnessFadeLength=0.6`
- The main fix path is **content-side** (Tracks A1-A6: mesh split,
  SkyLight RTC off, Rect Lights per window, etc.), not CVar.
- The dynamic-shadow smear ("shleif") is a **known UE 5.7.0 VSM
  regression** targeted for fix in 5.7.3 (Track B1).

This directory is a candidate for **deletion** once the todo's plan
lands. Kept temporarily so the experimental trail is reviewable.

Scope: editor-only tuning aid. Once final values are agreed, they roll into
[`Config/DefaultEngine.ini`](../../../../Config/DefaultEngine.ini) and
[`Config/DefaultScalability.ini`](../../../../Config/DefaultScalability.ini)
and this directory gets deleted. Do NOT depend on these `.txt` files from
production code or shipped builds.

## Files

| File | Intent | Expected cost vs base |
| --- | --- | --- |
| [`cvar_base.txt`](cvar_base.txt) | Resets all Phase 1 knobs to engine defaults | 0 (baseline) |
| [`cvar_clean.txt`](cvar_clean.txt) | Cheap stability only: longer temporal + firefly clamp + HWRT ShortRangeAO | ~0 ms |
| [`cvar_full.txt`](cvar_full.txt) | Full Phase 1: above + more rays + Quality 3 + ref samples 16 | ~0.5-1.5 ms (RTX 3060) |

## Usage

UE 5.7 renamed `exec` to `execfile`. `FFileHelper::LoadFileToString` uses
the raw filename so absolute paths are the safe bet (CWD is engine
binaries, not project).

In PIE console:

```
execfile <project-root>/scripts/ue/editor/render/cvar_base.txt
execfile <project-root>/scripts/ue/editor/render/cvar_clean.txt
execfile <project-root>/scripts/ue/editor/render/cvar_full.txt
```

Look for `Execing file: ...` in the log to confirm a load succeeded.
`Can't find file '...'` means the path did not resolve - paste the full
absolute path exactly.

## Optional: key bindings (per-session)

Use Ctrl+Alt+digit chords - none of these are bound by default in UE 5.7
editor or in PIE, unlike `F7` (Build) / `F8` (Eject) / `F9` (HighResShot)
which conflict.

```
setbind One   "execfile <project-root>/scripts/ue/editor/render/cvar_base.txt"  Control Alt
setbind Two   "execfile <project-root>/scripts/ue/editor/render/cvar_clean.txt" Control Alt
setbind Three "execfile <project-root>/scripts/ue/editor/render/cvar_full.txt"  Control Alt
```

`setbind` is session-only; lost on PIE stop / editor restart. Re-type if
you start a new session, or have the AI agent dispatch via
`mcp__ue-mcp__control_editor.console_command` instead - same effect,
zero key conflicts.

## Workflow

1. Start PIE in `City17_Persistent_WP`, navigate to a representative pose
   (entrance door, window-radiator, hallway).
2. `stat unit` visible in the corner.
3. Ctrl+Alt+1 (base) -> hold the camera still for 5 seconds.
4. Ctrl+Alt+2 (clean) -> same pose, same 5 seconds.
5. Ctrl+Alt+3 (full) -> same pose, same 5 seconds.
6. Pick the winner. Note the `stat unit` GPU delta.

## Related

- Scripts router: [../../../README.md](../../../README.md)
- Scripts architecture (SOLID/SoC): [../../../docs/architecture.md](../../../docs/architecture.md)
- Sibling helpers:
  [../blueprint/README.md](../blueprint/README.md),
  [../content/](../content/),
  [../level/README.md](../level/README.md)
