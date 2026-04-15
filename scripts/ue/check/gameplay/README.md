# Gameplay Checks

Gameplay-plugin-specific validation helpers live here.

Keep a validator in this subtree when it enforces a stable contract owned by one gameplay plugin rather than a generic Unreal validation rule.

Cross-plugin data cross-reference validation (objectId, lootProfileId, asset refs) lives in `../data/` because it spans multiple plugins.
