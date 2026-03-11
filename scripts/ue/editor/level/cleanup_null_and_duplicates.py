"""
Level Cleanup Script - Remove NULL meshes and duplicate actors
Addresses Map Check warnings for NULL StaticMesh properties and overlapping actors

Usage:
1. Open your level in UE Editor
2. Window → Developer Tools → Output Log
3. Cmd (backtick key) → py "path/to/this/script.py"

Or from Python console:
>>> exec(open('scripts/ue/editor/level/cleanup_null_and_duplicates.py').read())
"""

import unreal

print("=" * 80)
print("Level Cleanup: NULL Meshes & Duplicate Actors")
print("=" * 80)

# Get current level
world = unreal.EditorLevelLibrary.get_editor_world()
all_actors = unreal.EditorLevelLibrary.get_all_level_actors()

print(f"\nTotal actors in level: {len(all_actors)}")

# ============================================================================
# STEP 1: Find NULL StaticMesh actors
# ============================================================================

print("\n" + "=" * 80)
print("STEP 1: Finding NULL StaticMesh actors")
print("=" * 80)

null_static_mesh_actors = []
null_skeletal_mesh_actors = []
null_instanced_foliage = []
null_foliage_components = []  # Store (actor, component) tuples

for actor in all_actors:
    actor_name = actor.get_name()

    # Check StaticMeshActor
    if isinstance(actor, unreal.StaticMeshActor):
        static_mesh_component = actor.static_mesh_component
        if static_mesh_component:
            static_mesh = static_mesh_component.static_mesh
            if static_mesh is None:
                null_static_mesh_actors.append(actor)
                print(f"  ⚠ NULL StaticMesh: {actor_name}")

    # Check SkeletalMeshActor
    elif isinstance(actor, unreal.SkeletalMeshActor):
        skeletal_mesh_component = actor.skeletal_mesh_component
        if skeletal_mesh_component:
            skeletal_mesh = skeletal_mesh_component.skeletal_mesh
            if skeletal_mesh is None:
                null_skeletal_mesh_actors.append(actor)
                print(f"  ⚠ NULL SkeletalMesh: {actor_name}")

    # Check InstancedFoliageActor
    elif actor_name.startswith("InstancedFoliageActor"):
        # Check each component for NULL mesh
        components = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
        valid_component_count = 0
        null_component_count = 0

        for comp in components:
            if comp.static_mesh is None:
                # Found NULL mesh component
                null_foliage_components.append((actor, comp))
                null_component_count += 1
            else:
                valid_component_count += 1

        # If ALL components are NULL, mark actor for deletion
        if valid_component_count == 0 and null_component_count > 0:
            null_instanced_foliage.append(actor)
            print(f"  ⚠ Empty InstancedFoliageActor: {actor_name} (all {null_component_count} components NULL)")
        # If SOME components are NULL, report them
        elif null_component_count > 0:
            print(f"  ⚠ InstancedFoliageActor: {actor_name} has {null_component_count} NULL components (will remove components only)")

print(f"\nFound:")
print(f"  - NULL StaticMesh actors: {len(null_static_mesh_actors)}")
print(f"  - NULL SkeletalMesh actors: {len(null_skeletal_mesh_actors)}")
print(f"  - Empty InstancedFoliageActors (delete entire actor): {len(null_instanced_foliage)}")
print(f"  - NULL Foliage components (delete components only): {len(null_foliage_components)}")

# ============================================================================
# STEP 2: Find overlapping/duplicate actors
# ============================================================================

print("\n" + "=" * 80)
print("STEP 2: Finding overlapping actors (same location)")
print("=" * 80)

# Group actors by location
from collections import defaultdict

location_groups = defaultdict(list)

for actor in all_actors:
    if isinstance(actor, unreal.StaticMeshActor):
        location = actor.get_actor_location()
        # Round to 2 decimal places to handle floating point precision
        loc_key = (round(location.x, 2), round(location.y, 2), round(location.z, 2))

        static_mesh_component = actor.static_mesh_component
        if static_mesh_component and static_mesh_component.static_mesh:
            mesh_name = static_mesh_component.static_mesh.get_name()
            # Include mesh name in key to find true duplicates
            full_key = (loc_key, mesh_name)
            location_groups[full_key].append(actor)

# Find duplicates (more than 1 actor at same location with same mesh)
duplicate_groups = {k: v for k, v in location_groups.items() if len(v) > 1}

print(f"\nFound {len(duplicate_groups)} groups of overlapping actors:")
duplicates_to_remove = []

for (loc_key, mesh_name), actors in duplicate_groups.items():
    print(f"\n  Location {loc_key}, Mesh: {mesh_name}")
    print(f"    Duplicates: {len(actors)} actors")

    # Keep first, mark rest for deletion
    for i, actor in enumerate(actors):
        if i == 0:
            print(f"      ✓ KEEP: {actor.get_name()}")
        else:
            print(f"      ✗ DELETE: {actor.get_name()}")
            duplicates_to_remove.append(actor)

print(f"\nTotal duplicate actors to remove: {len(duplicates_to_remove)}")

# ============================================================================
# STEP 3: Summary and Confirmation
# ============================================================================

print("\n" + "=" * 80)
print("CLEANUP SUMMARY")
print("=" * 80)

total_to_delete = (
    len(null_static_mesh_actors) +
    len(null_skeletal_mesh_actors) +
    len(null_instanced_foliage) +
    len(duplicates_to_remove)
)

print(f"\nActors to delete:")
print(f"  - NULL StaticMesh actors: {len(null_static_mesh_actors)}")
print(f"  - NULL SkeletalMesh actors: {len(null_skeletal_mesh_actors)}")
print(f"  - Empty InstancedFoliageActors: {len(null_instanced_foliage)}")
print(f"  - Duplicate actors: {len(duplicates_to_remove)}")
print(f"  TOTAL: {total_to_delete} actors")
print(f"\nComponents to delete:")
print(f"  - NULL Foliage components: {len(null_foliage_components)}")

# ============================================================================
# STEP 4: Deletion (uncomment to execute)
# ============================================================================

print("\n" + "=" * 80)
print("READY TO DELETE")
print("=" * 80)
print("\nTo proceed with deletion, uncomment the deletion code below and re-run.")
print("Or manually delete actors listed above using World Outliner.")

# UNCOMMENT BELOW TO DELETE:

print("\nDeleting actors...")

deleted_count = 0

# Delete NULL mesh actors
for actor in null_static_mesh_actors:
    unreal.EditorLevelLibrary.destroy_actor(actor)
    deleted_count += 1
    print(f"  ✓ Deleted NULL StaticMesh: {actor.get_name()}")

for actor in null_skeletal_mesh_actors:
    unreal.EditorLevelLibrary.destroy_actor(actor)
    deleted_count += 1
    print(f"  ✓ Deleted NULL SkeletalMesh: {actor.get_name()}")

for actor in null_instanced_foliage:
    unreal.EditorLevelLibrary.destroy_actor(actor)
    deleted_count += 1
    print(f"  ✓ Deleted empty InstancedFoliageActor: {actor.get_name()}")

# Delete NULL foliage components (not entire actors)
component_deleted_count = 0
for actor, comp in null_foliage_components:
    try:
        comp_name = comp.get_name() if hasattr(comp, 'get_name') else str(comp)
        # Correct way to destroy component in Unreal Python API
        comp.destroy_component(actor)
        component_deleted_count += 1
        print(f"  ✓ Deleted NULL component from {actor.get_name()}: {comp_name}")
    except Exception as e:
        print(f"  ✗ Failed to delete component from {actor.get_name()}: {e}")

if component_deleted_count > 0:
    print(f"\n  Total NULL components deleted: {component_deleted_count}")

# Delete duplicates
for actor in duplicates_to_remove:
    unreal.EditorLevelLibrary.destroy_actor(actor)
    deleted_count += 1
    print(f"  ✓ Deleted duplicate: {actor.get_name()}")

print(f"\n✅ Cleanup complete!")
print(f"  - Deleted {deleted_count} actors")
if 'component_deleted_count' in locals() and component_deleted_count > 0:
    print(f"  - Deleted {component_deleted_count} NULL components")
print("\nSave your level to persist changes:")
print("  unreal.EditorLevelLibrary.save_current_level()")

print("\n" + "=" * 80)
print("Script complete.")
print("=" * 80)
