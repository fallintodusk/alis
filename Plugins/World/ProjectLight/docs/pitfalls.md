# ProjectLight pitfalls

## Large time seeks leave daylight on night walls

- Symptom: a stationary wall has bright diffuse-indirect patches after a large
  noon/night jump, despite correct Sun intensity and ready textures.
- Cause: cached indirect lighting can converge more slowly than the clock or
  assets. Scene Lighting and Final Gather update speeds control different stages;
  increasing only Final Gather does not establish Scene recovery.
- Fix: the runtime controller briefly accelerates the bound volume's native
  Scene update budget after a large notified seek, then restores its original
  override/value. Keep steady clocks, renderer defaults and exposure intact.
- Owner: `ProjectLightComponent.cpp`.
- Regression: `Project.Time.Integration.LightForwardPriority` covers large seeks,
  preserved authored budgets, Stop and unchanged/small/timer-driven updates.
  `Project.World.Realization.Presentation.DaylightHistory` in `runtime` mode checks
  the early rendered window, final player-view control and timed restoration.
  A longer visual-capture wait alone does not fix the runtime transient.

## Sun and night fill compete for the forward-light slot

- Symptom: twilight displays an equal-priority directional-light warning;
  Water, translucency and volumetric fog rely on an implicit brightness fallback.
- Cause: both atmospheric lights retain the same native forward priority.
- Fix: the existing runtime controller selects the brighter colored source and
  assigns distinct native priorities, changing them only on a selection change.
  Do not permanently prefer a zero-intensity Sun or add another light controller.
- Owner: `ProjectLightComponent.cpp`.
- Regression: `Project.Time.Integration.LightForwardPriority` checks day, night,
  both sources during twilight, dominance reversal, paused replay and no fill.

## Native solar elevation includes an engine orientation offset

- Symptom: noon illuminates from below or sunrise faces the wrong direction.
- Cause: SunPosition's returned elevation includes a 180-degree orientation
  offset; treating it as geographic altitude inverts the result.
- Fix: remove the native offset, construct the geographic direction through ENU,
  and point the directional light opposite the direction to the Sun.
- Owner: `ProjectLightSettings.cpp`.
- Regression: `Project.Time.Integration.LightSolarDirection` checks equatorial
  noon, midnight, east-facing sunrise and refusal of duplicate basis axes.

## A geographic context actor hides attached lighting in game

- Symptom: a registered and correctly updated night light contributes no light.
- Cause: the geographic actor derives from native AInfo, whose constructor hides
  the actor in game. A render component inherits its owner's hidden state.
- Fix: the composition owner puts the fill component on the existing visible
  Sun actor; the controller and context remain on the geographic actor.
- Regression: `Project.World.Realization.Presentation.DaylightLifecycle` checks
  visible ownership, saved references, opt-out and re-enablement.
