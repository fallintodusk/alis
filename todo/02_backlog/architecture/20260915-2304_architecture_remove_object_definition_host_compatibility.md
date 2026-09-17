# Remove Object Definition Host Compatibility

Status: uninvestigated

## Problem

Object spawning still defaults definitions without an explicit spawn class to
`AInteractableActor`, and ProjectWorld still stores definition-host metadata on
`AProjectWorldActor`. These are current compatibility paths marked `L001` and
`L004` in code.

## Investigation boundary

- Inventory serialized definitions and authored World actors that use either
  path.
- Identify the current interface/type that should own definition hosting.
- Prove editor placement, spawning, save/load, cook, and packaged runtime
  behavior before removing either path.
- Replace the code markers with the final current contract when migration is
  complete.

Do not remove either runtime path from documentation evidence alone.
