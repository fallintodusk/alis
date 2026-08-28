# Packaged gameplay acceptance

This folder owns end-to-end packaged gameplay release gates. These runners are
acceptance tools, not normal development iteration commands.

## Kazan survival proof

Run from the repository root:

```powershell
.\scripts\ue\gameplay\test\run_kazan_survival_proof.ps1
```

The runner builds and packages through the configured launcher engine, enters
Kazan through the real menu and ProjectLoading route, drives real input and UI,
and proves both success and failure/restart. Development owns instrumented
performance; Shipping owns cook and product-route correctness.

Authenticated operation evidence is written below
`Saved/Validation/Gameplay/KazanSurvival/<operation-id>/`. An accepted package
is published to `Saved/PackageRelease/KazanSurvival/Candidate`; an existing
candidate is rotated to `PreviousCandidate` before replacement.

Each accepted Candidate also contains `Launch_Kazan_PreviewFlight.cmd`. This
operator helper enters Kazan through the same menu and ProjectLoading route with
the explicit `PreviewFlight` traversal policy. It is not an in-game fly/walk
toggle and does not change the normal grounded Kazan route. In PreviewFlight,
use mouse look and WASD; hold Space to rise and Left Ctrl to descend.

The runner starts processes hidden and disables UE messaging discovery. Its
disposable files are isolated under `tmp/gameplay/kazan_survival/` and removed
by the runner. Accepted package and validation evidence are retained.
Before either cook, the runner proves that an existing Candidate can be rotated.
This catches a running game or File Explorer window browsing inside Candidate
before an expensive package transaction is started.
