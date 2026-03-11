# ProjectVitals TODO

Status (2026-02-13)
- DONE: Vitals integration contracts for definitions/layout/tags.
- DONE: Hysteresis transition coverage.
- DONE: Debuff handle cleanup coverage.
- DONE: ASC-backed debuff lifecycle integration coverage.
- DONE: Vitals ViewModel tag-priority integration coverage.
- DONE: Shared ViewModel contract coverage (HUD + panel).

---

## Optional Hardening (not urgent)

- [ ] Add integration coverage for `UVitalsViewModel::GetActiveStateTexts()` with real `ProjectTagTextSubsystem` mappings.
- [ ] Add ProjectUI factory-level integration coverage proving `vm_creation: Global` resolves one shared `UVitalsViewModel` instance for:
  - `ProjectVitalsUI.VitalsHUD`
  - `ProjectVitalsUI.VitalsPanel`

## Structural Note

- Tests are currently centralized under:
  - `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/ProjectVitalsIntegrationTest.cpp`
- Decide later if plugin-local tests are needed under:
  - `Plugins/Gameplay/ProjectVitals/Source`
  - `Plugins/UI/ProjectVitalsUI/Source`
