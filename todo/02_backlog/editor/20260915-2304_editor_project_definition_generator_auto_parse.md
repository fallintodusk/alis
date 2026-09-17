# Project Definition Generator Auto-Parse

Status: uninvestigated

## Problem

ProjectDefinitionGenerator still combines schema field mappings with
`FJsonObjectConverter` behavior. The desired single parsing authority is not
yet proven for identifier aliases, vectors, rotators, soft references,
gameplay tags, maps, or internal-only fields.

## Investigation boundary

- Inventory every current definition schema and generated target type.
- Compare JSON keys with reflected field names and custom import callbacks.
- Select one parsing owner without retaining parallel field-mapping behavior.
- Prove generation, regeneration, reference stability, and representative
  packaged consumers before removing the current mapping route.

Do not change public JSON identity merely to simplify reflection names.
