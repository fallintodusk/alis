# Migrate Placement Editor to Definitions

Status: uninvestigated

## Current gap

The placement toolbar still exposes hard-coded openable template classes while
the supported runtime object path is definition and capability driven.

## Investigation boundary

- Trace all toolbar entries, spawned classes, serialized assets, and consumers
  of ProjectOpenableTemplates.
- Define placement entries through the existing ObjectDefinition identity and
  spawn route.
- Remove template classes and plugin dependencies only after reference, editor
  drag/drop, capability, save/reload, and cook evidence is clean.

Do not add a second placement registry beside ObjectDefinition authority.
