# ProjectCombat

Optional combat primitives.

The plugin currently provides `UProjectCombatComponent` health, damage, heal,
and Blueprint event hooks plus an empty experience descriptor. Startup
registers the Combat feature name, but its initializer does not attach or
configure the component.

It does not currently implement weapons, targeting, abilities, encounter
activation, or a complete character integration. ProjectFeature owns feature
registration mechanics; ProjectCombat owns only combat-domain behavior.
