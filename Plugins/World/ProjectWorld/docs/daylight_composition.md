# Cooked daylight composition

World owns projection and composition of environmental time and lighting;
[ProjectTime](../../ProjectTime/README.md) owns the clock and optional controls,
and [ProjectLight](../../ProjectLight/README.md) owns lighting policy/application.

## Generated context

The optional `environment_time` object in the
[runtime-profile schema](../Data/Schemas/project_world_runtime_profile.schema.json)
opts a generated map into environmental time and names its explicit native city
zone. Concrete profiles own that selection. Omitted or disabled time leaves a
static scene; a partial presentation call without a runtime profile preserves an
existing context rather than implicitly disabling it.

WorldEditor serializes one Time context and one Light component on the existing
generated, always-loaded GeoReferencing actor. It creates no second geographic
authority. The optional night fill is a component on the existing generated Sun
actor, whose rendering remains visible; the geographic AInfo is hidden in game.
Native actor/volume creation, stable identity, material bindings,
save/rollback and package manifests remain inside the World transaction.
ProjectLight receives existing native actors and validated settings; it needs
neither canonical Terrain nor UnrealEd.

Context identity/integrity validation checks the source-profile identity, explicit
zone, same-world references and generated lighting roles. Hash equality is not
cryptographic source authenticity. Source admission and package verification
remain the existing generation authority's responsibility.

Identical projection requires matching data and registered Time/Light components
and performs no writes. A valid replay re-registers existing components in place;
serialized equality alone cannot establish runtime readiness. A source-backed
replay repairs a corrupted context or binding through the same owner. Duplicate
contexts/roles refuse; an invalid input does not partially create context. Explicit opt-out
removes the owned context, controller and night-fill components. The accompanying presentation application
restores declared static light settings; authored actors remain untouched.

## Runtime owner

`ProjectWorldDaylightSubsystem` performs concrete composition at world BeginPlay.
It admits only standalone Game/PIE worlds, resolves the one serialized context
once, validates native generated roles, and binds that world's Time provider to
Light through Core's read-only time-view contract. Position and ENU basis come
from the cooked GeoReferencing actor. No Editor module, global registry,
cross-world fallback or second binding mechanism participates.

Default clock reads are available before demo permission is granted. The
single-player policy separately requires the loading service's confirmed
experience/arrived-world pair to belong to this exact world and the configured experience to permit
controls. World end-play and deinitialization stop the controller; Time clears
its own context/observers. A new world starts Live.

## Evidence

`Project.World.Realization.Presentation.DaylightLifecycle` uses a tiny scene with
no Terrain cells. It checks no-op, corruption repair, invalid-input refusal,
actual map save/unload/reload, serialized references, damaged component
registration, runtime provider binding, a native world tick after repair and
Game-world startup through the concrete composition subsystem. Editor persistence alone is not
runtime composition or cooked acceptance. Rendered player/UI, day/night motion
and event-aligned performance are separate acceptance checks.

`Project.World.Realization.Presentation.DaylightPlayer` captures the actual Ready
player viewport. Its `-ProjectTimeProofStreet` option selects blocking road
support near the admitted preview spawn and requires a grounded Hero for every
capture. It exercises the real panel's scrub, speed and Reset handlers; pointer
gesture routing remains the separate `Project.Time.DemoUI.GestureRouting` test.
`-ProjectTimeProofVisualOnly` captures four paused times as scene views and one
separate UI view, then verifies Reset reaches Light without a cadence comparison.
Visual captures wait at least 75 seconds after each seek; resource readiness alone
does not establish settled lighting/adaptation history. This is a bounded wait
qualified against a reproduced time-jump artifact, not an automatic convergence
or night-visibility verdict. Review normal transitions separately. The cadence
and performance path retains its existing observation interval.
Allow 600 seconds in the outer runner for a full four/five-view visual proof;
the single scene/UI mode fits the narrower 300-second allowance.
UI capture targets the live panel's own Slate window; a global screenshot request
can select an Editor viewport after a scene capture. A control-pixel sanity check
rejects the known empty target but does not establish semantic UI acceptance.
`-ProjectTimeProofUiOnly` narrows this to one scene/UI pair for capture repair.
With `NightCompare` and `UiOnly`, `-ProjectTimeProofNightFloor=<EV>` selects one
bounded intermediate night-floor comparison while retaining the saved policy
outside the transient proof. Original controls may be reused only with matching
source identities, midnight time, player pose and lighting inputs; label retained
images separately from fresh ones and do not imply pixel-identical cloud animation.
The optional `-ProjectTimeProofNightCompare` diagnostic uses matched midnight
baseline, fill-off and lower-exposure-floor views, a cloud-free view toward the
artistic night source, and solar-relative twilight. It restores transient settings
and reads the final player-view exposure configuration; this is not a GPU exposure
measurement or a new preset. These modes prove neither performance nor other
streets. Review original PNGs, retain their exact
pose/time and distinguish fresh PIE views from reused cooked evidence; a contact
sheet or successful test is not human visual acceptance.

`Project.World.Realization.Presentation.DaylightHistory` is an opt-in early-window
diagnostic using the normal admitted Manhattan demo, actual clock and Hero. Pass
`-ProjectTimeHistoryArm=primary` for baseline/radiance-cache-off comparisons and
a short solar-relative 60x sunset; `scene` and `temporal` isolate Scene Lighting
Update Speed and temporal clearing only when the primary comparison warrants it.
The `runtime` arm keeps the existing binding for candidate seeks and tests the
actual ProjectLight recovery and its expiry. Controlled baselines rebind after
seeking to isolate native knobs from automatic runtime recovery. Final player-view
settings and camera positions are recorded; the runtime arm requires the faster
Scene budget at the early checkpoint and restored volume state at the late one.
It records normal 5/15/75-second views, separate native diffuse-indirect views,
actual UTC/rate, image dimensions and per-frame timings under
`-ProjectTimeProofRoot`. The controls are applied before each noon/night jump and
restored before teardown. It performs no generation or publication and requires
no untracked fixture path. Allow 660 seconds for the primary diagnostic.
An externally mounted retained snapshot remains diagnostic, even if its runtime
context binds successfully; this does not authenticate current generated outputs.
Image brightness/spatial variation and Editor timings are diagnostic measurements,
not night-visibility, temporal-noise, full-cycle or release-performance acceptance.
