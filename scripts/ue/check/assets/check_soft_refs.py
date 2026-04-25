"""UE Editor Python script: scan level actors for null material slots.

Run via: UnrealEditor-Cmd -run=PythonScript -Script=<this_file>

Scope: StaticMeshComponent and SkeletalMeshComponent material slots.
Checks both mesh-level defaults and per-instance overrides. Reports actors
where both are None (truly unresolvable null material at render time).

This is NOT a general soft-reference validator. It targets the specific
"Invalid ShaderMap material (None)" shipping log error caused by mesh
components with no material assigned at any level.
"""

import unreal


def main():
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    null_materials = []
    null_meshes = []

    for actor in actors:
        actor_name = actor.get_name()

        # Check static mesh components
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            sm = comp.static_mesh
            if sm is None:
                null_meshes.append(
                    f"{actor_name} -> {comp.get_name()}: static mesh is None"
                )
                continue

            mesh_mats = sm.get_editor_property("static_materials")
            num_slots = comp.get_num_materials()

            for i in range(num_slots):
                inst_mat = comp.get_material(i)
                if inst_mat is not None:
                    continue
                mesh_mat = None
                if i < len(mesh_mats):
                    mesh_mat = mesh_mats[i].material_interface
                if mesh_mat is None:
                    mesh_path = sm.get_path_name()
                    null_materials.append(
                        f"Actor={actor_name} Mesh={mesh_path} Slot={i}/{num_slots}"
                    )

        # Check skeletal mesh components
        for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            mesh = comp.get_editor_property("skeletal_mesh_asset")
            num_slots = comp.get_num_materials()
            for i in range(num_slots):
                inst_mat = comp.get_material(i)
                if inst_mat is not None:
                    continue
                # Check mesh-level default before reporting
                mesh_mat = None
                if mesh:
                    mesh_mats = mesh.get_editor_property("materials")
                    if i < len(mesh_mats):
                        mesh_mat = mesh_mats[i].material_interface
                if mesh_mat is None:
                    mesh_path = mesh.get_path_name() if mesh else "None"
                    null_materials.append(
                        f"Actor={actor_name} SkMesh={mesh_path} Slot={i}/{num_slots}"
                    )

    # Report
    for entry in null_materials:
        unreal.log_warning(f"NULL_MATERIAL: {entry}")

    for entry in null_meshes:
        unreal.log_warning(f"NULL_MESH: {entry}")

    total_issues = len(null_materials) + len(null_meshes)

    if total_issues > 0:
        unreal.log_error(
            f"SOFT_REF_CHECK: FAIL - {len(null_materials)} null material slots, "
            f"{len(null_meshes)} null mesh refs"
        )
    else:
        unreal.log(
            f"SOFT_REF_CHECK: PASS - scanned {len(actors)} actors, "
            f"no null material slots found"
        )


main()
