# Time verification

Clock, zone, isolation, paused revocation and UI command regressions live in the
plugin's Editor test module. Actual Slate gestures are covered by
`Project.Time.DemoUI.GestureRouting`; the gameplay mode contract is covered by
`Project.Time.Controls.ModePolicy`. Use the repository's exact-test runner.

World owns the tiny serialized composition fixture and real-city Editor probe;
see [its composition contract](../../ProjectWorld/docs/daylight_composition.md).
These fixtures do not establish cooked UI discovery or moving-Sun performance.

The optional client module contains a Development-only cooked acceptance probe.
It is inactive without explicit arguments and absent from Shipping behavior:

```text
<Development-game> -ProjectMenuPlayAutoExperience=<configured-demo> -ProjectMenuPlayAutoMode=SinglePlayer -ProjectTimeRuntimeProofRoot=<repo>/tmp/world/time/proof -ProjectTimeRuntimeProofNoonUtc=<ISO-UTC-noon> -ResX=2560 -ResY=1440 -Windowed -ForceRes -novsync -RenderOffScreen -unattended
```

Use a fresh diagnostic root. The loaded map must already have a qualified cooked
Time context, and the real gameplay policy must permit controls. The probe does
not create context or grant permission. It checks PC UTC, actual staged UI and
widget commands, civil labels, four day phases, paused/normal/accelerated samples,
Reset and configured-policy revocation. It quits with a bounded result and saves
screenshots and native frame timings under the supplied scratch root.

Screenshot writing can stall these frames. Treat this capture timing as
diagnostic; use the separate World traversal for frame-budget acceptance.

For the existing World product-route traversal, the same probe can supply only
the permitted clock fixture using `-ProjectTimeRuntimeTourRate=<rate>`. The World
performance owner establishes and validates its own quality/resolution envelope;
the clock fixture must not reject the temporary startup viewport before that
owner runs. UI-only capture must establish its one-shot native size after
save-load graphics settings have been applied, then validate the realized size within its bounded
timeout. Startup `-ResX`/`-ResY` alone are insufficient: ProjectSave can later
restore another resolution. The probe must not change or persist player saves.
Sample host load immediately after native process exit,
before source fingerprinting, so validation work does not shift that window.

Inspect screenshots independently. For Light apply/recovery attribution, enable
`LogProjectLight Verbose` or native CPU/frame/GPU/bookmark tracing. Source/package
identity, host-load windows, renderer settings and the existing performance
acceptance policy remain the surrounding execution owner's responsibility.
Probe success alone does not certify visual quality, performance, ordinary-mode
experience selection, full-world acceptance or an installed release.
