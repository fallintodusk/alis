# Level Cleanup Scripts

Utilities for cleaning up levels from problematic actors.

## cleanup_null_and_duplicates.py

Finds and removes:
- ✗ Actors with NULL StaticMesh
- ✗ Actors with NULL SkeletalMesh
- ✗ Empty InstancedFoliageActor
- ✗ Duplicate actors (same meshes at same location)

### Usage:

#### Method 1: Via Python Console (Recommended)

1. Open your level in Unreal Editor
2. **Window → Developer Tools → Python Console**
3. Execute:

```python
exec(open('scripts/ue/editor/level/cleanup_null_and_duplicates.py').read())
```

#### Method 2: Via Output Log

1. Open your level
2. **Window → Developer Tools → Output Log**
3. Press **Cmd** (backtick key `)
4. Enter:

```
py "<project-root>/scripts/ue/editor/level/cleanup_null_and_duplicates.py"
```

### Safety:

By default, the script **DOES NOT delete** actors, it only shows the list.

To **actually delete**:
1. Review the actor list in Output Log
2. Open `cleanup_null_and_duplicates.py`
3. Find the **STEP 4: Deletion** section
4. The code is already active (deletion enabled by default)
5. Re-run the script

### Example Output:

```
================================================================================
Level Cleanup: NULL Meshes & Duplicate Actors
================================================================================

Total actors in level: 15234

================================================================================
STEP 1: Finding NULL StaticMesh actors
================================================================================
  ⚠ NULL StaticMesh: SM_IndoorPlant3_UAID_DBBBC1F9FD8C8DAA02_2046361220
  ⚠ NULL StaticMesh: Bush_A2_UAID_DBBBC1F9FD8C8DAA02_2046348212
  ⚠ Empty InstancedFoliageActor: InstancedFoliageActor_25600_0_0_-1

Found:
  - NULL StaticMesh actors: 15
  - NULL SkeletalMesh actors: 1
  - Empty InstancedFoliageActors: 2

================================================================================
STEP 2: Finding overlapping actors (same location)
================================================================================

Found 45 groups of overlapping actors:

  Location (12345.67, -8901.23, 456.78), Mesh: SM_Wall_01
    Duplicates: 2 actors
      ✓ KEEP: StaticMeshActor_UAID_DBBBC1F9FD8C8DAA02_2046477283
      ✗ DELETE: StaticMeshActor_UAID_DBBBC1F9FD8C8DAA02_2046477263

Total duplicate actors to remove: 78

================================================================================
CLEANUP SUMMARY
================================================================================

Actors to delete:
  - NULL StaticMesh actors: 15
  - NULL SkeletalMesh actors: 1
  - Empty InstancedFoliageActors: 2
  - Duplicate actors: 78
  TOTAL: 96 actors
```

### After Deletion:

**Important: Save your level:**

```python
unreal.EditorLevelLibrary.save_current_level()
```

Or via menu: **File → Save Current Level**

### Verify Results:

Run Map Check:
1. **Window → Developer Tools → Message Log**
2. **Tools → Map Check** (or Ctrl+Shift+M)
3. Verify that warnings have decreased

---

## Troubleshooting

**Q: Script can't find Python Console**
A: Ensure that Python Editor Script Plugin is enabled:
   - Edit → Plugins → search "Python"
   - Enable "Python Editor Script Plugin"
   - Restart the editor

**Q: Script doesn't delete actors**
A: Verify that deletion code is active in STEP 4

**Q: Deleted too much, how to undo?**
A: Ctrl+Z or close level without saving and reopen
