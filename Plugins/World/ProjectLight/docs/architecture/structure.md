# ProjectLight structure

## Purpose and boundaries

ProjectLight owns scene-lighting parameters and geographic Sun updates, including
sky, cloud/fog appearance and gameplay exposure policy. It operates on explicitly
supplied native actors. It does not spawn actors, create volume brushes, select
geography, compile materials, change renderer scalability or own gameplay lamps.
It has no UnrealEd or concrete ProjectTime dependency.

World owns native actor creation, generated identity, transactions and cooked
binding. Time and optional UI remain independent owners. Future weather may
supply conditions to rendering owners; ProjectLight does not own weather state.
Relationships are drawn in [the main view](diagrams/main.md).

## Composition and contracts

`FProjectLightSettings` and `FProjectLightBindings` define the supplied policy
and native object binding. Static application validates before writes, retaining
the declared fixed/Histogram exposure, movable Sun, real-time Sky Light and
existing cloud/fog setup. Diagnostic capture exposure remains explicit.

`UProjectLightComponent` is an opt-in runtime consumer of Core's read-only
`IProjectTimeView`. Its concrete composition owner supplies the exact world's
provider and native geographic ENU basis. Game/PIE standalone admission,
same-world references and finite settings are required before activation.
No private clock or implicit global lookup is used on failure.

Solar position uses native SunPosition with UTC, zero civil offset and no DST
offset. The output is transformed through the supplied geographic basis.
The controller updates Sun direction and fades direct illumination across the
horizon. A separately bound, non-shadowing night fill points opposite the Sun;
it is an artistic ambient source, not an astronomical Moon simulation. Histogram
exposure transitions to the declared night floor. The configuration contract
owns these values. This policy still requires rendered night-visibility and
highlight acceptance; valid geometry and finite settings alone do not prove it.

The night floor bounds Histogram adaptation; it is not a target image brightness
or another light source. Assess navigation, sky brightness and shaded surfaces
separately. Exposure cannot provide missing local illumination. A bounded floor
comparison must preserve the fill and other lighting inputs rather than claiming
that globally brighter images establish finished nighttime presentation.

Forward shading, translucency, single-layer Water and volumetric fog consume one
directional light. The runtime controller gives unique priority to the brighter
active colored source, with a deterministic Sun tie-break. It writes priorities
only when their native values change. A permanently preferred Sun is insufficient
for the night transition; normal surface lighting still uses the supplied sources.

The second atmospheric light can have a native sky disk without a new actor, but
does not provide lunar orbit/phase data or stars. An artistic night fill is not an
astronomical Moon. `TimeOfDay_Default` is an Engine example level, not a drop-in
star material. Additional sky assets require explicit World ownership/cooked
references and appearance proof; they do not justify another environment clock.
See Epic's [sky atmosphere expressions](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-properties-in-unreal-engine)
and [Sky Light capture contract](https://dev.epicgames.com/documentation/unreal-engine/sky-lights-in-unreal-engine).

## Update lifecycle and performance

Clock notifications coalesce into one pending component tick. Timed updates read
the latest UTC rather than replaying missed steps. Paused Demo stops periodic
light updates; seek, rate and Reset request immediate fresh application. No
unchanged UTC instant is written twice. End-play and Stop remove delegates,
timers and pending ticks.

Large notified UTC discontinuities temporarily accelerate the bound PostProcess
volume's native Lumen Scene lighting updates. The
[runtime controller](../../Source/ProjectLight/Private/ProjectLightComponent.cpp)
owns the threshold, recovery speed and short world-timer window. It saves the
original native override/value once, preserves higher authored budgets and
restores the saved state on expiry, Stop, rebind or end-play. Repeated large
seeks extend the window without replacing its saved baseline. Same-time
notifications, small clock steps and ordinary timer reads do not request recovery.
Pausing the calendar does not pause this world timer. No global CVar, cache
reset, permanent quality increase or separate time authority participates.

Cadence belongs to serialized Light settings. Defaults are hypotheses until
visual motion and dynamic rendering cost qualify them. Native trace bookmarks
identify actual applies and engine frames. Compare frozen, Live and accelerated
Sun at matched viewpoints and quality. Retain preceding/apply/recovery frames,
worst observed event cost and overall percentiles: sparse VSM/sky work can be
invisible in pooled p95/p99. Existing load-aware performance rules still apply.

Static acceptance does not certify moving-Sun performance. A headless Sun
direction test does not certify night visibility, shadow stability or GPU cost.
