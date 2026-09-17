# Fix Shipping Playtest Defects

Status: uninvestigated

## Current defects

- P0: stairwell collision can let the player fall through modular landings.
- P1: wall/door seams become visible at close camera contact.
- P1: some open doors still block the locomotion capsule.
- P1: basic combat can kill the player before a meaningful response.
- P1: Escape did not open the pause menu in the reported Shipping route.
- P2: bathroom-door affordance, loot balance, and first-person motion trails
  need separate product decisions.

## Acceptance

Reproduce each item independently in a current Shipping package. Compare a
known-good modular entrance for collision geometry, keep balance separate from
mechanics, and add focused regressions for every confirmed code/config defect.
Close items that do not reproduce only with captured current evidence.
