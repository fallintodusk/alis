import math
import os

import unreal


PROJECT_ROOT = unreal.SystemLibrary.get_project_directory().rstrip("/\\")
REPORT_PATH = os.path.join(PROJECT_ROOT, "Saved", "Validation", "Reports", "fp_body_defaults_report.txt")
NATIVE_CLASS_PATH = "/Script/ProjectCharacter.ProjectCharacter"
BP_CLASS_PATH = "/ProjectObject/Human/Hero/BP_Hero"
TOLERANCE = 0.01

CHECKS = [
    ("CharacterMesh0", "relative_location"),
    ("CharacterMesh0", "relative_rotation"),
    ("FirstPersonCamera", "relative_location"),
    ("FirstPersonCamera", "relative_rotation"),
]


def ensure_report_dir():
    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)


def load_cdo(path):
    if path.startswith("/Script/"):
        loaded_class = unreal.load_class(None, path)
    else:
        loaded_class = unreal.EditorAssetLibrary.load_blueprint_class(path)

    if not loaded_class:
        raise RuntimeError(f"Failed to load class: {path}")

    default_object = unreal.get_default_object(loaded_class)
    if not default_object:
        raise RuntimeError(f"Failed to get default object: {path}")

    return default_object


def find_component(actor, component_name):
    for component in actor.get_components_by_class(unreal.ActorComponent):
        if component.get_name() == component_name:
            return component
    return None


def vector_tuple(value):
    return (float(value.x), float(value.y), float(value.z))


def rotator_tuple(value):
    return (float(value.pitch), float(value.yaw), float(value.roll))


def normalize_value(value):
    if isinstance(value, unreal.Vector):
        return ("Vector", vector_tuple(value))
    if isinstance(value, unreal.Rotator):
        return ("Rotator", rotator_tuple(value))
    return (type(value).__name__, value)


def values_match(lhs, rhs):
    lhs_kind, lhs_value = normalize_value(lhs)
    rhs_kind, rhs_value = normalize_value(rhs)

    if lhs_kind != rhs_kind:
        return False

    if lhs_kind in {"Vector", "Rotator"}:
        return all(math.isclose(a, b, abs_tol=TOLERANCE) for a, b in zip(lhs_value, rhs_value))

    return lhs_value == rhs_value


def render_value(value):
    kind, data = normalize_value(value)
    if kind == "Vector":
        return f"({data[0]:.2f}, {data[1]:.2f}, {data[2]:.2f})"
    if kind == "Rotator":
        return f"(pitch={data[0]:.2f}, yaw={data[1]:.2f}, roll={data[2]:.2f})"
    return str(data)


def get_prop(obj, prop_name):
    try:
        return obj.get_editor_property(prop_name)
    except Exception as exc:
        raise RuntimeError(f"Failed to read '{prop_name}' from '{obj.get_name()}': {exc}") from exc


def run_validation():
    ensure_report_dir()

    native_cdo = load_cdo(NATIVE_CLASS_PATH)
    bp_cdo = load_cdo(BP_CLASS_PATH)

    lines = ["FP body defaults validation", ""]
    failures = []

    for component_name, prop_name in CHECKS:
        native_component = find_component(native_cdo, component_name)
        bp_component = find_component(bp_cdo, component_name)

        if not native_component or not bp_component:
            failures.append(f"Missing component '{component_name}' in native={not native_component} bp={not bp_component}")
            continue

        native_value = get_prop(native_component, prop_name)
        bp_value = get_prop(bp_component, prop_name)

        lines.append(
            f"{component_name}.{prop_name}: native={render_value(native_value)} | bp={render_value(bp_value)}"
        )

        if not values_match(native_value, bp_value):
            failures.append(
                f"Mismatch: {component_name}.{prop_name} native={render_value(native_value)} bp={render_value(bp_value)}"
            )

    local_body_component = find_component(bp_cdo, "LocalBodyMesh")
    if local_body_component:
        anim_class = get_prop(local_body_component, "anim_class")
        if anim_class is not None:
            lines.append(f"Warning: LocalBodyMesh.anim_class is preset in BP_Hero: {anim_class.get_path_name()}")

    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines + ["", "Failures:"] + (failures or ["None"])))

    for line in lines:
        unreal.log(line)

    if failures:
        for failure in failures:
            unreal.log_error(failure)
        raise RuntimeError(f"FP body defaults validation failed with {len(failures)} mismatch(es)")

    unreal.log("[OK] FP body defaults validation passed")
    unreal.log(f"[OK] Report: {REPORT_PATH}")


run_validation()
