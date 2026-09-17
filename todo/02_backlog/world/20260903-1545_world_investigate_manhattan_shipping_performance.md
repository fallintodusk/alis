# Investigate Manhattan Shipping Performance

Status: watch; activate only if the symptom reproduces

## Current gap

An operator observed poor Manhattan performance in Shipping, while comparable
Development measurements remained within the fixed frame budget. The cause is
unverified and the symptom is not a release blocker without current
reproduction.

## Investigation boundary

- Reproduce in a package built from the same source state as the Development
  control.
- Use the same route, resolution, warmup, camera, and frame sample envelope.
- Capture CPU/GPU frame percentiles, streaming state, executable identity, and
  package identity.
- Compare configuration/cook differences only after the product symptom is
  measured.

Do not relax the fixed performance budget or tune Manhattan from subjective
evidence alone.
