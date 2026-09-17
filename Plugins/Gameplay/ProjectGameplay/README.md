# ProjectGameplay

Enabled runtime module shell retained as a declared composition dependency of
ProjectSinglePlay and ProjectOnlinePlay.

The current module contains startup/shutdown only. Feature registration
contracts live in [ProjectFeature](../../Features/ProjectFeature/README.md),
and gameplay tags live in ProjectCore. This plugin does not currently provide
base GameModes or gameplay utilities.

Removing or repurposing the shell requires a descriptor, Build.cs, asset, and
cook consumer audit; its enabled state alone is not implementation evidence.
