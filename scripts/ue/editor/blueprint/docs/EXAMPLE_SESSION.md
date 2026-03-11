# Example Session - Real World Usage

## Scenario: Gameplay Balancing for Enemy Blueprints

### Context
Game designer wants to rebalance all enemy Blueprints:
- Increase difficulty by 25%
- Make all enemies invincible during testing
- Verify no broken asset references

---

## Session Timeline

### Step 1: Export All Enemy Blueprints

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")

from export_blueprint import export_blueprint_to_json, save_json

enemies = [
    "/Game/Enemies/BP_Goblin.BP_Goblin",
    "/Game/Enemies/BP_Orc.BP_Orc",
    "/Game/Enemies/BP_Troll.BP_Troll",
    "/Game/Enemies/BP_Dragon.BP_Dragon"
]

for enemy_path in enemies:
    data = export_blueprint_to_json(enemy_path)
    save_json(data)
    print(f"Exported: {enemy_path}")
```

**Output:**
```
Exported: /Game/Enemies/BP_Goblin.BP_Goblin
  → Saved/AI_Snapshots/blueprint_20250114_120000.json

Exported: /Game/Enemies/BP_Orc.BP_Orc
  → Saved/AI_Snapshots/blueprint_20250114_120001.json

... (4 files total)
```

---

### Step 2: Send to Claude

**User Message to Claude:**
```
I have 4 enemy Blueprint JSON files. Please:

1. Set can_be_damaged to false for ALL enemies (testing mode)
2. Increase custom_time_dilation by 25% for each
3. Verify all asset references are valid (no "/Game/Default/" paths)
4. Return modified JSONs + validation report

[Attached: 4 JSON files]
```

**Claude Response:**
```
Validation Report:

[OK] BP_Goblin: All asset references valid
[OK] BP_Orc: All asset references valid
[WARN] BP_Troll: Uses default material "/Game/Default/M_Basic"
[OK] BP_Dragon: All asset references valid

Modifications Applied:
- can_be_damaged: true → false (all 4 enemies)
- custom_time_dilation:
  - BP_Goblin: 1.0 → 1.25
  - BP_Orc: 1.2 → 1.5
  - BP_Troll: 0.8 → 1.0
  - BP_Dragon: 1.5 → 1.875

Recommendation: Replace BP_Troll's default material with project-specific asset.

[Returns 4 modified JSON files]
```

---

### Step 3: Review and Import

```python
from import_blueprint import import_from_file, list_available_snapshots

# List all snapshots
list_available_snapshots()

# Import each with dry-run first
import_from_file("blueprint_20250114_120000.json", dry_run=True)
import_from_file("blueprint_20250114_120001.json", dry_run=True)
import_from_file("blueprint_20250114_120002.json", dry_run=True)
import_from_file("blueprint_20250114_120003.json", dry_run=True)

# If all validations pass, apply changes
import_from_file("blueprint_20250114_120000.json", dry_run=False)
import_from_file("blueprint_20250114_120001.json", dry_run=False)
import_from_file("blueprint_20250114_120002.json", dry_run=False)
import_from_file("blueprint_20250114_120003.json", dry_run=False)
```

**Output:**
```
[DRY RUN] Would set 'can_be_damaged' = False
[DRY RUN] Would set 'custom_time_dilation' = 1.25
[OK] Dry run successful - no errors

[OK] Set 'can_be_damaged' = False
[OK] Set 'custom_time_dilation' = 1.25
[OK] Saved Blueprint: /Game/Enemies/BP_Goblin.BP_Goblin

... (4 Blueprints updated)
```

---

### Step 4: Verify in Editor

1. Open BP_Goblin in Blueprint Editor
2. Check Details panel:
   - can_be_damaged: ✓ False
   - custom_time_dilation: ✓ 1.25
3. Compile and test in-game

---

### Step 5: Fix BP_Troll Material (Follow-up)

**User Message to Claude:**
```
BP_Troll is using "/Game/Default/M_Basic".
Available materials:
- /Game/Materials/M_Enemy_Green
- /Game/Materials/M_Enemy_Stone

Which one should BP_Troll use based on the name?
Update the JSON accordingly.
```

**Claude Response:**
```
Based on the name "Troll" (typically stone/rock creatures),
I recommend: /Game/Materials/M_Enemy_Stone

[Returns updated BP_Troll JSON with material reference changed]
```

**Import Updated JSON:**
```python
import_from_file("BP_Troll_fixed_material.json", dry_run=False)
```

---

## Session Results

### Before
| Enemy | can_be_damaged | custom_time_dilation | Material |
|-------|----------------|----------------------|----------|
| Goblin | true | 1.0 | M_Enemy_Green |
| Orc | true | 1.2 | M_Enemy_Red |
| Troll | true | 0.8 | M_Basic (default) |
| Dragon | true | 1.5 | M_Enemy_Scale |

### After
| Enemy | can_be_damaged | custom_time_dilation | Material |
|-------|----------------|----------------------|----------|
| Goblin | **false** | **1.25** | M_Enemy_Green |
| Orc | **false** | **1.5** | M_Enemy_Red |
| Troll | **false** | **1.0** | **M_Enemy_Stone** |
| Dragon | **false** | **1.875** | M_Enemy_Scale |

---

## Time Saved

**Manual Process:**
- Open 4 Blueprints in editor: ~5 min
- Find and modify properties: ~15 min
- Verify asset references: ~10 min
- Fix material reference: ~5 min
**Total: ~35 minutes**

**With Blueprint JSON Converter + Claude:**
- Export: 30 seconds
- Claude processing: 2 minutes
- Import + verify: 2 minutes
**Total: ~5 minutes**

**Time savings: 30 minutes (85% faster)**

---

## Lessons Learned

1. **Always validate first** - Dry-run caught potential issues before applying
2. **Batch processing wins** - Handling 4 Blueprints simultaneously saved significant time
3. **Claude catches mistakes** - Identified default material that would've been missed manually
4. **Clear requests** - Specific instructions to Claude yielded better results

---

## Other Real-World Examples

### Example 2: Network Optimization

**Task:** Reduce network bandwidth for background NPCs

```
"Set net_priority to 0.5 for all NPC Blueprints
that don't have 'Boss' or 'Important' in their name"
```

### Example 3: Spawn Location Adjustment

**Task:** Move all enemy spawn points 500 units higher

```
"Increase relative_location.z by 500 for all enemy Blueprints"
```

### Example 4: Debug Mode Toggle

**Task:** Enable god mode for player testing

```
"Set can_be_damaged to false and custom_time_dilation to 0.5
for /Game/Player/BP_PlayerCharacter"
```

---

## Best Practices from This Session

1. Export all related Blueprints at once
2. Provide Claude with context about the game/project
3. Request validation in addition to modifications
4. Use dry-run for all imports
5. Document changes for team review
6. Keep original JSON exports as backups

---

## References

- [CLAUDE_WORKFLOW.md](CLAUDE_WORKFLOW.md) - More Claude usage patterns
- [USAGE.md](USAGE.md) - Detailed usage instructions
- [LIMITATIONS.md](LIMITATIONS.md) - System capabilities and limitations
