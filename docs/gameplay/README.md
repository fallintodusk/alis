# Gameplay Architecture

Documentation for gameplay mechanics, modes, and features.

> **Implementation Note:** Code lives in `Plugins/Gameplay/`, `Plugins/Features/`, and `Plugins/World/`.

## Gameplay Framework

- **[ProjectGameplay](../../Plugins/Gameplay/ProjectGameplay/README.md)** - Base classes, interfaces, and shared types.
- **[ProjectCharacter](../../Plugins/Gameplay/ProjectCharacter/README.md)** - The primary player character, inputs, and camera.
    - [Design Doc](../../Plugins/Gameplay/ProjectCharacter/docs/design.md)
- **[ProjectGAS](../../Plugins/Gameplay/ProjectGAS/README.md)** - Gameplay Ability System configuration (Attributes, GEs).
- **[ProjectVitals](../../Plugins/Gameplay/ProjectVitals/README.md)** - Survival mechanics (Health, Stamina, Hunger).

## Game Modes

- **[SinglePlay](../../Plugins/Gameplay/ProjectSinglePlay/README.md)** - Orchestrator for single-player session.
- **[OnlinePlay](../../Plugins/Gameplay/ProjectOnlinePlay/README.md)** - Orchestrator for multiplayer session.
- **[MenuPlay](../../Plugins/Gameplay/ProjectMenuPlay/README.md)** - Lightweight mode for the main menu.

## Features

- **[Interaction](../../Plugins/Gameplay/ProjectInteraction/README.md)** - Interaction system.
- **[Inventory](../../Plugins/Features/ProjectInventory/README.md)** - Inventory management.

## World

- **[ProjectWorld](../../Plugins/World/ProjectWorld/README.md)** - World Partition, Tiles, and Environment.

## Usage Guides (Moved to Features)

- **[Dialogue Manual](../../Plugins/Features/ProjectDialogue/docs/manual.md)** - Asset authoring & setup.
- **[World Manual](../../Plugins/World/ProjectWorld/docs/manual.md)** - World Partition & Maps guide.
- **[Experience Manual](../../Plugins/Gameplay/ProjectGameplay/docs/manual_experiences.md)** - Game Modes & Experience setup.
