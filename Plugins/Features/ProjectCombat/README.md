ProjectCombat

Purpose
- Combat gameplay systems (weapons, abilities, damage, targeting).

Activation Strategy
- **OnDemand** - Lazy-loaded at runtime when combat encounters begin
- Reduces startup time and memory footprint
- Mounted by Orchestrator when game mode or world triggers require combat

Responsibilities
- Combat mechanics (melee, ranged, abilities)
- Damage calculation and application
- Targeting and lock-on systems
- Combat state management
- Registers a combat experience descriptor (CDO) for ProjectLoading (OnDemand content recipe)

Non-responsibilities
- No boot-time initialization required
- No compile-time dependencies on other feature plugins
- Uses engine APIs and ModularFeatures for loose coupling

Activation Triggers
- Game mode startup (combat-enabled modes)
- World trigger (enter combat zone)
- Quest/progression system (combat tutorial)

See Plugins/Boot/Orchestrator/README.md for activation lifecycle details.
