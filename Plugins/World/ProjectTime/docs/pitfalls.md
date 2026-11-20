# ProjectTime pitfalls

## Invalid zones can silently format as GMT

- Symptom: a mistyped city zone appears to work but shows the wrong hour.
- Cause: native formatting can fall back to GMT; nonempty output is not validity.
  Linking a second static ICU instance also bypasses Core's initialized data.
- Fix: use Core's native formatter and validate its resolved `VV` identity against
  the explicit zone; reject unknown/fallback results before context admission.
- Owner: `ProjectTimeZone.cpp`.
- Regression: `Project.Time.CivilZone.OfflineRules` checks both city zones,
  winter/summer differences and malformed/unknown identifiers.

## A paused preview still needs permission revocation

- Symptom: closing UI at rate zero leaves an unauthorized environmental override.
- Cause: neither a stopped light timer nor a hidden panel triggers a new read.
- Fix: observe permission while Demo is active; revoke to Live and notify below UI.
- Owner: `ProjectTimeSubsystem.cpp`.
- Regression: `Project.Time.Clock.PausedRevocation` advances native timer frames
  without UI refresh or clock reads.

## Startup capture size can be replaced during map loading

- Symptom: a cooked UI probe waits for 1440p and times out at 1080p despite
  correct startup resolution arguments.
- Cause: the real menu/load route applies saved graphics after engine startup.
  Waiting for a size that no owner requests cannot establish a capture envelope.
- Fix: the explicit Development-only UI probe requests its one-shot native size
  after world/pawn admission, then validates the realized viewport. It never
  persists graphics settings. Traversal retains World's performance-envelope
  owner instead of introducing another resolution writer.
- Owner: `ProjectTimeRuntimeProof.cpp`.
- Verification: execute the cooked probe through the real menu/load route with
  differing saved resolution. A successful build is not rendered-envelope proof.
