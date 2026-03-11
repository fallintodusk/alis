# ProjectDefinitionGenerator - Auto-Parse Migration

## Vision

Migrate from explicit FieldMappings to pure auto-parsing via `FJsonObjectConverter::JsonObjectToUStruct`.

**Principle:** JSON is Source of Truth, USTRUCT mirrors JSON structure.

**Requirements for auto-parse:**
- USTRUCT field names match JSON keys (case-insensitive)
- Nested structs match nested JSON objects
- Custom callback handles edge cases (array vectors, soft pointer normalization)

---

## Current State

| Definition Type | FieldMappings | Status |
|-----------------|---------------|--------|
| ObjectDefinition | Has mappings | LEGACY - migrate |
| (others) | TBD | Check schemas |

---

## Tasks

### Phase 1: ObjectDefinition Migration

- [ ] Verify `UObjectDefinition` USTRUCT field names match JSON keys
  - `id` -> `ObjectId` (mismatch - need JSON field "objectId" or rename property)
  - `meshes` -> `Meshes` (match)
  - `capabilities` -> `Capabilities` (match)

- [ ] Decide: Change JSON "id" to "objectId" OR rename `ObjectId` property?
  - Option A: JSON uses "objectId" (breaking change for existing JSONs)
  - Option B: Keep special handling for "id" -> ID property (current approach)

- [ ] Remove `FieldMappings` from `object.schema.json` after verification

- [ ] Test: Generate object definitions with empty FieldMappings

### Phase 2: Other Definition Types (if added)

- [ ] Verify USTRUCT field names match JSON keys
- [ ] Remove `FieldMappings` from schema after verification
- [ ] Test: Generate definitions with empty FieldMappings

### Phase 3: Cleanup Legacy Code

After all definition types migrated:

- [ ] Remove `EDefinitionFieldType` enum (or keep minimal for edge cases)

- [ ] Simplify `SetPropertyFromJson` - remove giant switch statement

- [ ] Remove `FieldMappings` parsing from schema reader

- [ ] Update documentation

### Phase 4: Callback Enhancement

If needed, extend `CustomJsonImportCallback` for additional edge cases:

- [ ] `TArray<FName>` from JSON string array (if UE doesn't handle)
- [ ] `TMap<FName, FString>` from JSON object (if UE doesn't handle)
- [ ] `FGameplayTag` from string
- [ ] `FGameplayTagContainer` from string array

---

## Edge Cases Handled by Callback

Currently implemented:

| JSON Format | Target Type | Callback Action |
|-------------|-------------|-----------------|
| `[X, Y, Z]` | `FVector` | Parse array to vector |
| `[P, Y, R]` | `FRotator` | Parse array to rotator |
| `"/Game/Foo"` | `TSoftObjectPtr` | Normalize to `/Game/Foo.Foo` |
| `"/Game/Foo"` | `TSoftClassPtr` | Normalize to `/Game/Foo.Foo_C` |

---

## Notes

- **ID Property:** Currently handled specially because property name varies (`ObjectId`, `Id`, etc.). Consider standardizing to `Id` across all definition types.

- **Internal Flags:** Some structs have internal flags like `FObjectMeshPhysics.bIsSet` that aren't in JSON. These require manual fix-up after auto-parse.

- **Backward Compatibility:** Keep legacy path until all definition types are migrated and tested.
