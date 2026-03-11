# Claude Workflow Examples

## Typical Tasks for Claude

### 1. Validation and Verification

**Request:**
```
"Validate this Blueprint JSON:
1. Check all Vector values for NaN/Inf
2. Verify custom_time_dilation is between 0.1 and 5.0
3. Ensure all asset references use valid paths
4. List any suspicious or default values"
```

**Claude Response:**
```
Validation Results:

[OK] Vectors: All valid, no NaN/Inf detected
[OK] custom_time_dilation: 1.0 (within range)
[WARN] Asset Reference: path="/Game/Default/Cube" (using default asset)
[INFO] relative_location: (0, 0, 0) - default spawn position

Recommendations:
- Consider replacing default Cube reference
- Set meaningful spawn location
```

---

### 2. Property Modification

**Request:**
```
"Modify this Blueprint JSON:
1. Set can_be_damaged to false
2. Increase custom_time_dilation by 50%
3. Set relative_location.z to 500
4. Return only the modified JSON"
```

**Claude Response:**
```json
{
  "asset_path": "/Game/Project/BP_Example.BP_Example",
  "properties": {
    "can_be_damaged": {
      "value": false,
      "type": "bool"
    },
    "custom_time_dilation": {
      "value": 1.5,
      "type": "float"
    },
    "relative_location": {
      "value": {"x": 0, "y": 0, "z": 500},
      "type": "Vector"
    }
  }
}
```

---

### 3. Batch Processing

**Request:**
```
"I have 5 enemy Blueprint JSONs. For each:
1. Increase MaxHealth by 30%
2. Set can_be_damaged to true
3. Add difficulty suffix to asset_path (_Easy, _Normal, _Hard based on health)
4. Return all modified JSONs"
```

**Claude Response:**
Returns 5 modified JSON files with calculated difficulty tiers.

---

### 4. Documentation Generation

**Request:**
```
"Generate markdown documentation for all exported properties:
- Group by category (Gameplay, Network, Transform, etc.)
- Include current values
- Add brief description of each property
- Highlight non-default values"
```

**Claude Response:**
```markdown
# Blueprint Properties: BP_Example

## Gameplay
- **can_be_damaged**: true (default)
  - Controls whether actor takes damage
- **custom_time_dilation**: 2.0 [MODIFIED]
  - Time scaling factor for this actor

## Transform
- **relative_location**: (0, 0, 500) [MODIFIED]
  - Actor spawn position in world space
...
```

---

### 5. Diff Generation

**Request:**
```
"Compare these two Blueprint JSON exports (before/after).
Generate a diff showing what changed."
```

**Claude Response:**
```
Changes detected:

custom_time_dilation:
  - Before: 1.0
  + After:  2.0

can_be_damaged:
  - Before: true
  + After:  false

relative_location.z:
  - Before: 100
  + After:  500

Summary: 3 properties modified
```

---

### 6. Property Search

**Request:**
```
"Find all properties in this Blueprint JSON that:
1. Contain 'health' or 'damage' in the name
2. Have Vector type
3. Have non-default values (not 0, false, or empty)
4. Are asset references"
```

**Claude Response:**
```
Search Results:

Name contains 'health' or 'damage':
  - can_be_damaged: false

Vector type properties:
  - relative_location: (0, 0, 500) [non-default]
  - relative_rotation: (0, 0, 0) [default]

Asset references:
  - WeaponMesh: "/Game/Weapons/SM_Rifle.SM_Rifle"
```

---

### 7. Value Clamping

**Request:**
```
"Clamp all numeric properties to safe ranges:
- custom_time_dilation: 0.1 to 5.0
- MaxHealth: 1 to 10000
- All Vector components: -100000 to 100000
Return modified JSON if any clamping was applied"
```

**Claude Response:**
```
Clamping applied:

custom_time_dilation: 10.5 → 5.0 (exceeded max)
MaxHealth: 0 → 1 (below min)

[Returns modified JSON with clamped values]
```

---

### 8. Consistency Check

**Request:**
```
"Check consistency across these 10 enemy Blueprint JSONs:
1. Do they all have the same properties?
2. Are value ranges consistent?
3. Do they follow naming conventions?
4. Report any outliers"
```

**Claude Response:**
```
Consistency Report:

[OK] All 10 Blueprints have matching property sets
[WARN] MaxHealth variance: 50-500 (10x difference)
[ERROR] BP_Orc missing 'can_be_damaged' property
[INFO] Naming: 8/10 follow BP_Enemy_* convention

Outliers:
- BP_Dragon: MaxHealth=500 (5x higher than average)
- BP_Slime: custom_time_dilation=0.5 (only slow enemy)
```

---

## Best Practices

### 1. Clear Instructions

**Good:**
```
"Set can_be_damaged to false and increase custom_time_dilation to 2.0"
```

**Bad:**
```
"Make the actor invincible and slower"
```

### 2. Specify Output Format

**Good:**
```
"Return only the modified properties section as JSON"
```

**Bad:**
```
"Change the values"
```

### 3. Request Validation

**Good:**
```
"Modify these values, then validate the result and list any potential issues"
```

**Bad:**
```
"Just change the numbers"
```

### 4. Use Dry-Run First

Always import with `dry_run=True` before applying Claude's changes to verify compatibility.

---

## Workflow Template

```
Step 1: Export Blueprint
  → Run export_blueprint.py
  → Get JSON file

Step 2: Send to Claude
  → Provide clear instructions
  → Specify desired output format
  → Request validation if needed

Step 3: Review Claude's Response
  → Check for logical errors
  → Verify value ranges
  → Confirm property names match

Step 4: Import with Dry-Run
  → import_from_file(filename, dry_run=True)
  → Check for import errors

Step 5: Apply Changes
  → import_from_file(filename, dry_run=False)
  → Verify in UE Editor
```

---

## Error Handling

### Claude Returns Invalid JSON

**Problem:** Claude adds markdown formatting or explanations.

**Solution:**
```
"Return ONLY valid JSON, no markdown code blocks, no explanations"
```

### Property Name Typos

**Problem:** Claude misspells property name.

**Solution:** Dry-run will catch this. Ask Claude to fix.

### Type Mismatches

**Problem:** Claude returns string for numeric property.

**Solution:**
```
"Ensure all property types match the original JSON exactly"
```

---

## Integration Tips

1. **Version Control** - Commit JSON snapshots before/after Claude modifications
2. **Automated Testing** - Round-trip test: Export → Import → Export → Compare
3. **Batch Processing** - Process multiple Blueprints in single Claude session
4. **Documentation** - Ask Claude to document changes for team review

---

## References

See [EXAMPLE_SESSION.md](EXAMPLE_SESSION.md) for real-world usage examples.
