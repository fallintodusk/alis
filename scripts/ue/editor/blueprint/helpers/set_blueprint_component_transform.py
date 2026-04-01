"""
Update a Blueprint component template transform through SubobjectDataSubsystem.

Blueprint inherited component overrides live on the Blueprint's component templates.
Editing only the generated-class CDO can leave stale inherited defaults behind, so
this helper targets the template object owned by the asset itself.
"""

import math
import os

import unreal


TOLERANCE = 0.01


def require_env(name):
    value = os.environ.get(name, "").strip()
    if not value:
        raise RuntimeError(f"Missing required environment variable: {name}")
    return value


def parse_triplet(name, text):
    parts = [item.strip() for item in text.split(",")]
    if len(parts) != 3:
        raise RuntimeError(f"{name} must have 3 comma-separated numbers, got: {text}")

    try:
        return tuple(float(item) for item in parts)
    except ValueError as exc:
        raise RuntimeError(f"{name} contains a non-numeric value: {text}") from exc


def resolve_blueprint_paths(raw_path):
    asset_name = raw_path.rsplit("/", 1)[-1]
    if "." in asset_name:
        package_path = raw_path.rsplit(".", 1)[0]
        object_path = raw_path
    else:
        package_path = raw_path
        object_path = f"{raw_path}.{asset_name}"
    return package_path, object_path


def tuples_match(lhs, rhs):
    return all(math.isclose(left, right, abs_tol=TOLERANCE) for left, right in zip(lhs, rhs))


def vector_tuple(value):
    return (float(value.x), float(value.y), float(value.z))


def rotator_tuple(value):
    return (float(value.pitch), float(value.yaw), float(value.roll))


def format_triplet(value):
    return f"({value[0]:.2f}, {value[1]:.2f}, {value[2]:.2f})"


def load_blueprint(object_path):
    blueprint = unreal.EditorAssetLibrary.load_asset(object_path)
    if not blueprint:
        raise RuntimeError(f"Failed to load Blueprint asset: {object_path}")
    return blueprint


def find_component_template(blueprint, variable_name, object_name):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    if not subsystem:
        raise RuntimeError("Failed to get SubobjectDataSubsystem")

    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    if not handles:
        raise RuntimeError(f"No Blueprint subobjects were found for {blueprint.get_name()}")

    variable_match = None
    object_match = None

    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        data_variable_name = unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data)
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)

        if not component:
            continue

        if variable_name and str(data_variable_name) == variable_name:
            variable_match = component
            break

        if object_name and component.get_name() == object_name:
            object_match = component

    return variable_match or object_match


def compile_blueprint(blueprint):
    if hasattr(unreal, "KismetEditorUtilities") and hasattr(unreal.KismetEditorUtilities, "compile_blueprint"):
        unreal.KismetEditorUtilities.compile_blueprint(blueprint)
        return

    if hasattr(unreal, "BlueprintEditorLibrary") and hasattr(unreal.BlueprintEditorLibrary, "compile_blueprint"):
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        return

    unreal.log_warning("No Blueprint compile API was available; saving without an explicit compile step")


def main():
    raw_asset_path = require_env("ALIS_BP_ASSET_PATH")
    package_path, object_path = resolve_blueprint_paths(raw_asset_path)
    component_variable_name = os.environ.get("ALIS_BP_COMPONENT_VARIABLE_NAME", "").strip()
    component_object_name = os.environ.get("ALIS_BP_COMPONENT_OBJECT_NAME", "").strip()

    if not component_variable_name and not component_object_name:
        raise RuntimeError(
            "Provide ALIS_BP_COMPONENT_VARIABLE_NAME or ALIS_BP_COMPONENT_OBJECT_NAME so the target template is explicit"
        )

    location_tuple = parse_triplet("ALIS_BP_RELATIVE_LOCATION", require_env("ALIS_BP_RELATIVE_LOCATION"))
    rotation_tuple = parse_triplet("ALIS_BP_RELATIVE_ROTATION", require_env("ALIS_BP_RELATIVE_ROTATION"))

    target_location = unreal.Vector(*location_tuple)
    target_rotation = unreal.Rotator(pitch=rotation_tuple[0], yaw=rotation_tuple[1], roll=rotation_tuple[2])

    blueprint = load_blueprint(object_path)
    component = find_component_template(blueprint, component_variable_name, component_object_name)
    if not component:
        raise RuntimeError(
            "Failed to find Blueprint component template "
            f"(variable='{component_variable_name}', object='{component_object_name}')"
        )

    current_location = component.get_editor_property("relative_location")
    current_rotation = component.get_editor_property("relative_rotation")
    current_location_tuple = vector_tuple(current_location)
    current_rotation_tuple = rotator_tuple(current_rotation)

    location_changed = not tuples_match(current_location_tuple, location_tuple)
    rotation_changed = not tuples_match(current_rotation_tuple, rotation_tuple)

    if not location_changed and not rotation_changed:
        unreal.log(
            f"[OK] {blueprint.get_name()}:{component.get_name()} already matches "
            f"location={format_triplet(location_tuple)} rotation={format_triplet(rotation_tuple)}"
        )
        return

    blueprint.modify()
    component.modify()

    if location_changed:
        component.set_editor_property("relative_location", target_location)
        unreal.log(
            f"[OK] Updated {component.get_name()}.relative_location "
            f"{format_triplet(current_location_tuple)} -> {format_triplet(location_tuple)}"
        )

    if rotation_changed:
        component.set_editor_property("relative_rotation", target_rotation)
        unreal.log(
            f"[OK] Updated {component.get_name()}.relative_rotation "
            f"{format_triplet(current_rotation_tuple)} -> {format_triplet(rotation_tuple)}"
        )

    # Compile after the template edit so generated defaults stay in sync with the asset template.
    compile_blueprint(blueprint)

    if not unreal.EditorAssetLibrary.save_asset(package_path, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save Blueprint asset: {package_path}")

    unreal.log(f"[OK] Saved Blueprint asset: {package_path}")


if __name__ == "__main__":
    main()
