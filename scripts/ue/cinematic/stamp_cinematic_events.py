"""Stamp a JSON event plan onto a LevelSequence's AAlisCinematicProxy spawnable.

FULL AUTOMATION for cinematic UI / interaction events. After this script
runs against a LevelSequence + event-plan JSON:

  - The LevelSequence has a Spawnable binding for AAlisCinematicProxy
    (created if absent).
  - The spawnable's ScheduledEvents UPROPERTY contains the JSON's events.
  - At sequence playback, the proxy's Tick fires each event when proxy-
    elapsed time crosses its OffsetSeconds.

No Sequencer Event Track / Director Blueprint / endpoint binding needed -
the events run from the spawnable's own Tick, not from Sequencer's
event-track machinery (which has a fragile Python API).

Clock semantics: OffsetSeconds is measured from the proxy's BeginPlay
(World->GetTimeSeconds() - StartTimeSeconds). This is NOT the LevelSequence
playback timeline; for typical takes (no time dilation, modest warmup) the
two are close enough that "seconds since the shot started" works. For
frame-accurate sync the proxy supports a Sequencer Event Track too.

Idempotent: re-running with the same JSON re-writes the ScheduledEvents
array. Re-running with a different JSON replaces the events.

Usage:

    py <project-root>/scripts/ue/cinematic/stamp_cinematic_events.py \
        <project-root>/Saved/cinematic_events/kitchen.json

JSON shape (see Saved/cinematic_events/example_kitchen.json):

    {
      "sequence": "/Game/Cinematics/Takes/2026-05-18/Scene_1_02",
      "events": [
        { "offset_seconds": 2.0, "method": "OpenInventory"  },
        { "offset_seconds": 3.0, "method": "InteractActor",
          "target_actor": "/Game/Maps/City17_Persistent_WP.City17_Persistent_WP:PersistentLevel.Door_42" },
        { "offset_seconds": 5.0, "method": "CloseInventory" }
      ]
    }

Allowed methods (case-insensitive):
    OpenInventory, CloseInventory,
    OpenVitals, CloseVitals,
    OpenMindJournal, CloseMindJournal,
    InteractFocused, InteractActor

`target_actor` is optional and only meaningful for `InteractActor` -- the
proxy resolves the soft path at render time and bypasses the focus trace
so the recorded actor is the one interacted with (not whatever the
focus trace lands on a frame off). The same JSON is what the editor-side
cinematic-take stamper auto-writes to Saved/cinematic_events/<TakeName>.json
when Take Recorder finishes a take, so this script's input format and the
stamper's output format are intentionally identical.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

PROXY_CLASS_PATH = "/Script/ProjectSinglePlayClient.AlisCinematicProxy"
ALLOWED_METHODS = {
    "OpenInventory", "CloseInventory",
    "OpenVitals", "CloseVitals",
    "OpenMindJournal", "CloseMindJournal",
    "InteractFocused",
    "InteractActor",
}

OUT = Path(unreal.Paths.project_saved_dir()) / "cinematic_stamp_result.json"


def _add_spawnable_from_class(seq, proxy_class, result: dict):
    """Add a spawnable binding for `proxy_class` on `seq`.

    UE 5.7 deprecated `MovieSceneSequence.add_spawnable_from_class` in favor of
    the equivalent on `LevelSequenceEditorSubsystem`. We prefer the new path
    when present so this script doesn't drift onto a deprecated API, and fall
    back to the old path on older engines. The exact method name on the new
    subsystem can vary across UE Python builds (snake_case vs camelCase), so
    we probe both attribute names before falling back.
    """
    try:
        ed_subsys = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
    except Exception:
        ed_subsys = None

    if ed_subsys is not None:
        for attr in ("add_spawnable_from_class", "AddSpawnableFromClass"):
            fn = getattr(ed_subsys, attr, None)
            if callable(fn):
                result.setdefault("ops", []).append(
                    {"add_spawnable": f"LevelSequenceEditorSubsystem.{attr}"}
                )
                return fn(seq, proxy_class)

    # Older engine path. Still works in UE 5.7 but logs a deprecation.
    result.setdefault("ops", []).append(
        {"add_spawnable": "MovieSceneSequence.add_spawnable_from_class (deprecated fallback)"}
    )
    return seq.add_spawnable_from_class(proxy_class)


def fail(result: dict, message: str) -> None:
    result["errors"].append(message)
    OUT.write_text(json.dumps(result, indent=2))
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: stamp_cinematic_events.py <path/to/event_plan.json>")
        raise SystemExit(1)

    plan_path = Path(sys.argv[1])
    result: dict = {"plan_path": str(plan_path), "ops": [], "errors": []}

    # --------------------------------------------------------------- read plan
    if not plan_path.is_file():
        fail(result, f"Plan file not found: {plan_path}")

    try:
        plan = json.loads(plan_path.read_text())
    except Exception as exc:
        fail(result, f"Could not parse JSON: {exc!r}")

    seq_path = plan.get("sequence")
    raw_events = plan.get("events", [])
    if not seq_path:
        fail(result, "Plan missing 'sequence' field.")
    if not isinstance(raw_events, list):
        fail(result, "Plan 'events' must be a list.")

    # --------------------------------------------------------- validate events
    # Accepts the canonical key `offset_seconds`. The legacy key `time_seconds`
    # (used by the first iteration of this script) is rejected with a clear
    # error so old plans surface immediately instead of silently dropping fields.
    valid_lower = {m.lower(): m for m in ALLOWED_METHODS}
    parsed_events = []
    for i, e in enumerate(raw_events):
        if "time_seconds" in e and "offset_seconds" not in e:
            fail(result,
                 f"Event {i}: legacy key 'time_seconds' is no longer accepted. "
                 f"Rename to 'offset_seconds' (the field is BeginPlay-relative, "
                 f"not sequence-timeline time - see AlisCinematicProxy.h).")
        t = e.get("offset_seconds")
        m = e.get("method")
        eid = e.get("id")  # optional; auto-generated below if missing
        target = e.get("target_actor")  # optional; only meaningful for InteractActor
        if not isinstance(t, (int, float)) or t < 0:
            fail(result, f"Event {i}: 'offset_seconds' must be a non-negative number, got {t!r}.")
        if not isinstance(m, str):
            fail(result, f"Event {i}: 'method' must be a string, got {m!r}.")
        if m.lower() not in valid_lower:
            fail(result, f"Event {i}: method '{m}' not in allowed set {sorted(ALLOWED_METHODS)}.")
        if eid is not None and not isinstance(eid, str):
            fail(result, f"Event {i}: 'id' must be a string if provided, got {eid!r}.")
        if target is not None and not isinstance(target, str):
            fail(result, f"Event {i}: 'target_actor' must be a string if provided, got {target!r}.")
        canonical_method = valid_lower[m.lower()]
        if canonical_method == "InteractActor" and not target:
            fail(result, f"Event {i}: InteractActor requires non-empty 'target_actor'.")
        if canonical_method != "InteractActor" and target is not None:
            fail(result, f"Event {i}: 'target_actor' is only allowed for InteractActor.")
        parsed_events.append({
            "id": eid,
            "offset_seconds": float(t),
            "method": canonical_method,
            "target_actor": target,
        })

    parsed_events.sort(key=lambda e: e["offset_seconds"])
    result["sequence"] = seq_path
    result["event_count"] = len(parsed_events)

    # ------------------------------------------------------------------ load asset
    seq = unreal.load_asset(seq_path)
    if seq is None or not isinstance(seq, unreal.LevelSequence):
        fail(result, f"LevelSequence not loaded at {seq_path}")

    proxy_class = unreal.load_class(None, PROXY_CLASS_PATH)
    if proxy_class is None:
        fail(result, f"Class not loaded: {PROXY_CLASS_PATH}. Rebuild editor first.")

    # ---------------------------------------------- find or create proxy binding
    binding = None
    try:
        bindings = seq.get_bindings()
    except Exception as exc:
        fail(result, f"get_bindings: {exc!r}")
        bindings = []

    for b in bindings:
        try:
            template = b.get_object_template()
        except Exception:
            template = None
        if template is not None and template.get_class() == proxy_class:
            binding = b
            result["ops"].append({"binding": "existing", "name": binding.get_name()})
            break

    if binding is None:
        try:
            binding = _add_spawnable_from_class(seq, proxy_class, result)
            result["ops"].append({"binding": "created_spawnable", "name": binding.get_name()})
        except Exception as exc:
            fail(result, f"add_spawnable_from_class: {exc!r}")

    # ------------------------------------------------------ playback range check
    # offset_seconds is BeginPlay-relative, the playback range is sequence-time.
    # They are NOT the same clock; this check is best-effort to catch obviously
    # wrong values (e.g. typing 200 in a 30-second take) and only assumes the
    # two clocks are roughly comparable at proxy-spawn-time.
    try:
        movie_scene = seq.get_movie_scene()
        pb_range = movie_scene.get_playback_range()
        display_rate = movie_scene.get_display_rate()
        rate_num = display_rate.numerator
        rate_den = display_rate.denominator
        if rate_num > 0:
            end_seconds = pb_range.get_end_frame() * rate_den / rate_num
            for i, e in enumerate(parsed_events):
                if e["offset_seconds"] > end_seconds + 0.001:
                    result["errors"].append(
                        f"Event {i} offset {e['offset_seconds']:.3f}s exceeds sequence playback end {end_seconds:.3f}s"
                    )
    except Exception as exc:
        result["errors"].append(f"playback range check failed (non-fatal): {exc!r}")

    # --------------------------------------------- write ScheduledEvents on template
    template = binding.get_object_template()
    if not isinstance(template, unreal.AlisCinematicProxy):
        fail(result, f"binding template is not an AAlisCinematicProxy ({template.get_class().get_name()})")

    # Build the array of FAlisCinematicEvent structs. We always provide an Id
    # so the proxy's FiredEventIds dedup is reliable (no need to rely on
    # BeginPlay-time auto-generation).
    struct_array = []
    for i, e in enumerate(parsed_events):
        s = unreal.AlisCinematicEvent()
        # Caller may set "id" in the JSON; otherwise we generate a stable one
        # based on (index, method, offset).
        eid = e.get("id") or f"evt_{i}_{e['method']}_{e['offset_seconds']:.3f}"
        s.set_editor_property("id", unreal.Name(eid))
        s.set_editor_property("offset_seconds", e["offset_seconds"])
        s.set_editor_property("method", unreal.Name(e["method"]))
        # InteractActor events carry a soft path to the recorded target actor;
        # the proxy resolves it at render and bypasses the focus trace.
        target = e.get("target_actor")
        if target:
            s.set_editor_property("target_actor", unreal.SoftObjectPath(target))
        struct_array.append(s)

    try:
        template.set_editor_property("scheduled_events", struct_array)
        result["ops"].append({"scheduled_events": len(struct_array)})
    except Exception as exc:
        fail(result, f"set scheduled_events: {exc!r}")

    # ------------------------------------------------------------------ save
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
        result["ops"].append({"save": True})
    except Exception as exc:
        result["errors"].append(f"save: {exc!r}")

    # Echo back what we wrote so the result JSON is self-describing.
    result["events_written"] = parsed_events

    OUT.write_text(json.dumps(result, indent=2))
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
