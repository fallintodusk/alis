# ProjectTime structure

## Purpose and boundaries

ProjectTime supplies an offline environmental calendar for one standalone Game
or PIE world. It does not retime gameplay, physics, animation, vitals or logs,
and does not implement network time or weather.

The public read-only boundary is
[ProjectTimeView](../../../../Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/ProjectTimeView.h).
The provider owns UTC; consumers receive an explicit world identity and change
notifications. A notification requests a fresh read rather than carrying a stale
timestamp. There is no global clock registry. Relationships are drawn in
[the main view](diagrams/main.md).

## Composition

| Part | Responsibility |
|---|---|
| `ProjectTimeClock` | Deterministic Live/Demo model with supplied UTC and monotonic samples |
| `ProjectTimeZone` | Native PC UTC sampling, city-zone validation and civil formatting |
| `ProjectTimeWorldContextComponent` | Serialized zone and generating profile identity on an existing owner |
| `ProjectTimeSubsystem` | World admission, context integrity, commands, permission revocation and notifications |
| `ProjectTimeDemoUI` | Optional local-player launcher, native JSON panel and ViewModel |

## Calendar and permission invariants

Live reads the PC's current UTC, independently of the PC's configured timezone.
City civil time is presentation, using UE Core's initialized native timezone
formatter. Solar consumers must not apply the city offset a second time.
Unsupported zone identifiers fail closed; no online service or custom timezone
database substitutes for missing native support.

Scrubbing enters paused Demo. Rate changes reanchor the current instant and use
monotonic elapsed time; Reset returns to fresh PC UTC. These overrides are
session-only and do not migrate across worlds. Invalid inputs refuse before
changing the clock. Display refresh never issues a command.

Commands require a valid same-world serialized context and the authoritative
GameMode's `IProjectTimeControlPolicy`. Permission defaults to denial below UI.
The single-player implementation consumes configured experience permission and
the loading service's confirmed named-experience/arrived-world pair. A URL flag, matching map name
or stale experience name cannot grant it. A permission observer runs only during
Demo, including paused Demo; revocation restores Live and notifies consumers.

The subsystem admits Game/PIE standalone worlds only. Editor, preview and
networked worlds do not acquire an active clock. Unbind, world end-play and
deinitialization clear observers, context and mutable session state.

## UI lifecycle

The optional client module stages its own JSON through UFS. ProjectUI remains
the owner of modal layering and input restoration. The admitted local player
gets an F6 hint and launcher; the modal panel provides time offset, rate, Reset
and Close. No world pause or global time dilation is applied. Hiding the panel
does not reset an intentionally running preview. Revocation/travel closes it,
and teardown removes only this module's input bindings and ticker.

## Verification boundaries

`Project.Time.Clock.*`, `Project.Time.CivilZone.OfflineRules`,
`Project.Time.Controls.ModePolicy` and `Project.Time.DemoUI.Controls.Session`
cover the model, native zones, isolation, permission and widget command path.
World owns serialized context projection and real runtime composition tests.
Cooked UI input/focus, visual day/night and rendering cost require native player
evidence; headless clock tests cannot certify them.
