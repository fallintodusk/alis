# Follow-ups: ue-mcp inspect_cdo

**Repo:** G:\PublicRepos\Unreal_mcp (fork of ChiR24/Unreal_mcp)
**Parent task:** extend_ue_mcp_inspect_cdo.md
**Status:** Post-merge follow-ups

---

## 1. Enum display names in property reflection

**Problem:** `ExportObjectToJson` exports raw enum values (`NewEnumerator0`, `NewEnumerator1`) instead of human-readable names (`MovementStickMode`, `CameraStyle`, `Gait`).

**Scope:** Affects ALL tools using `McpPropertyReflection`, not just `inspect_cdo`.

**Fix location:** `McpPropertyReflection.cpp` - `ExportPropertyToJsonValue()` enum handling path. Needs `UEnum::GetNameStringByValue()` or `GetDisplayNameTextByValue()`.

**Risk:** Changes reflection output for every handler. Needs careful regression testing.

---

## 2. SCS attachParent missing in detailed mode

**Problem:** Summary mode correctly shows `attachParent` from `SCS_Node->ParentComponentOrVariableName`. But `detailed: true` property dump reads from the component template object where `AttachParent` is null (templates aren't attached at authoring time).

**Impact:** Cosmetic. The `attachParent` field IS present in the summary-level component object. The `properties` sub-object just has a redundant null `AttachParent`.

**Options:**
- [ ] Inject `attachParent` into the detailed properties object from SCS node data
- [ ] Document that `attachParent` is on the component-level object, not inside `properties`
- [ ] Filter out null `AttachParent` from detailed export

---

## 3. Integration test for inherited BP overrides

**Problem:** No live Unreal test verifies that child BP overrides (e.g. mesh swap on inherited component) return correct values.

**Fix:** Add case to `tests/integration.mjs`:
```js
{
  scenario: 'inspect_cdo returns child BP override for inherited mesh component',
  toolName: 'inspect',
  arguments: {
    action: 'inspect_cdo',
    blueprintPath: '/Game/Test/BP_ChildOverride',
    componentName: 'CharacterMesh0',
    propertyNames: ['SkeletalMesh', 'AnimClass']
  },
  expected: 'success'
}
```
Requires a test Blueprint pair (parent + child with override) in the test project.

---

## 4. Non-component UObject subobjects not expanded

**Problem:** CDO subobjects like `HealthAttributes`, `SurvivalAttributes`, `StaminaAttributes` show as path strings but their internal properties aren't dumped.

**Impact:** Medium. Can't inspect attribute default values through `inspect_cdo`.

**Fix:** Detect UObject-typed CDO properties that are subobjects and recursively export their properties.

---

## 5. Inherited vs overridden property distinction

**Problem:** No way to tell which CDO properties BP_Hero explicitly overrides vs inherits from parent (ProjectCharacter).

**Impact:** Medium. Would help identify what's intentionally customized in a child BP.

**Fix:** Compare child CDO property values against parent CDO. Mark properties that differ as `overridden: true`. Expensive but valuable for auditing.
