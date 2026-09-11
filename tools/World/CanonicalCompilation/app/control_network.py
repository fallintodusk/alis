from __future__ import annotations

import itertools
import math
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, execution_identity, transform_points

from .contracts import CompilerError, canonical_hash, file_hash, read_json, validate_document, write_json


WGS84_A = 6378137.0
WGS84_F = 1.0 / 298.257223563
WGS84_B = (1.0 - WGS84_F) * WGS84_A
RECEIPT_SCHEMA = "https://alis.world/schemas/world-compiler/control-network-receipt-v1.json"
HORIZONTAL_CLASS_LIMITS_M = {
    "precision": 2.0,
    "standard": 5.0,
    "context": None,
}


def geodesic_distance(left: list[float], right: list[float]) -> float:
    if left == right:
        return 0.0
    longitude_delta = math.radians(right[0] - left[0])
    reduced_left = math.atan((1.0 - WGS84_F) * math.tan(math.radians(left[1])))
    reduced_right = math.atan((1.0 - WGS84_F) * math.tan(math.radians(right[1])))
    sin_left, cos_left = math.sin(reduced_left), math.cos(reduced_left)
    sin_right, cos_right = math.sin(reduced_right), math.cos(reduced_right)
    value = longitude_delta
    for _ in range(100):
        sin_value, cos_value = math.sin(value), math.cos(value)
        sin_sigma = math.sqrt(
            (cos_right * sin_value) ** 2
            + (cos_left * sin_right - sin_left * cos_right * cos_value) ** 2
        )
        if sin_sigma == 0:
            return 0.0
        cos_sigma = sin_left * sin_right + cos_left * cos_right * cos_value
        sigma = math.atan2(sin_sigma, cos_sigma)
        sin_alpha = cos_left * cos_right * sin_value / sin_sigma
        cos_sq_alpha = 1.0 - sin_alpha**2
        cos_two_sigma = cos_sigma - 2.0 * sin_left * sin_right / cos_sq_alpha if cos_sq_alpha else 0.0
        coefficient = WGS84_F / 16.0 * cos_sq_alpha * (4.0 + WGS84_F * (4.0 - 3.0 * cos_sq_alpha))
        previous = value
        value = longitude_delta + (1.0 - coefficient) * WGS84_F * sin_alpha * (
            sigma + coefficient * sin_sigma * (
                cos_two_sigma + coefficient * cos_sigma * (-1.0 + 2.0 * cos_two_sigma**2)
            )
        )
        if abs(value - previous) <= 1e-12:
            break
    else:
        raise CompilerError("geodesic_nonconvergence", "WGS84 inverse distance did not converge")
    u_sq = cos_sq_alpha * (WGS84_A**2 - WGS84_B**2) / WGS84_B**2
    a_coefficient = 1.0 + u_sq / 16384.0 * (
        4096.0 + u_sq * (-768.0 + u_sq * (320.0 - 175.0 * u_sq))
    )
    b_coefficient = u_sq / 1024.0 * (256.0 + u_sq * (-128.0 + u_sq * (74.0 - 47.0 * u_sq)))
    delta_sigma = b_coefficient * sin_sigma * (
        cos_two_sigma
        + b_coefficient / 4.0 * (
            cos_sigma * (-1.0 + 2.0 * cos_two_sigma**2)
            - b_coefficient / 6.0 * cos_two_sigma
            * (-3.0 + 4.0 * sin_sigma**2) * (-3.0 + 4.0 * cos_two_sigma**2)
        )
    )
    return WGS84_B * a_coefficient * (sigma - delta_sigma)


def geodesic_destination(origin: list[float], distance_m: float, bearing_degrees: float) -> list[float]:
    alpha_one = math.radians(bearing_degrees)
    tangent = (1.0 - WGS84_F) * math.tan(math.radians(origin[1]))
    cos_reduced = 1.0 / math.sqrt(1.0 + tangent**2)
    sin_reduced = tangent * cos_reduced
    sigma_one = math.atan2(tangent, math.cos(alpha_one))
    sin_alpha = cos_reduced * math.sin(alpha_one)
    cos_sq_alpha = 1.0 - sin_alpha**2
    u_sq = cos_sq_alpha * (WGS84_A**2 - WGS84_B**2) / WGS84_B**2
    a_coefficient = 1.0 + u_sq / 16384.0 * (
        4096.0 + u_sq * (-768.0 + u_sq * (320.0 - 175.0 * u_sq))
    )
    b_coefficient = u_sq / 1024.0 * (256.0 + u_sq * (-128.0 + u_sq * (74.0 - 47.0 * u_sq)))
    sigma = distance_m / (WGS84_B * a_coefficient)
    for _ in range(100):
        two_sigma = 2.0 * sigma_one + sigma
        delta = b_coefficient * math.sin(sigma) * (
            math.cos(two_sigma)
            + b_coefficient / 4.0 * (
                math.cos(sigma) * (-1.0 + 2.0 * math.cos(two_sigma) ** 2)
                - b_coefficient / 6.0 * math.cos(two_sigma)
                * (-3.0 + 4.0 * math.sin(sigma) ** 2)
                * (-3.0 + 4.0 * math.cos(two_sigma) ** 2)
            )
        )
        updated = distance_m / (WGS84_B * a_coefficient) + delta
        if abs(updated - sigma) <= 1e-12:
            sigma = updated
            break
        sigma = updated
    else:
        raise CompilerError("geodesic_nonconvergence", "WGS84 destination did not converge")
    latitude = math.atan2(
        sin_reduced * math.cos(sigma) + cos_reduced * math.sin(sigma) * math.cos(alpha_one),
        (1.0 - WGS84_F) * math.sqrt(
            sin_alpha**2
            + (sin_reduced * math.sin(sigma) - cos_reduced * math.cos(sigma) * math.cos(alpha_one)) ** 2
        ),
    )
    longitude = math.atan2(
        math.sin(sigma) * math.sin(alpha_one),
        cos_reduced * math.cos(sigma) - sin_reduced * math.sin(sigma) * math.cos(alpha_one),
    )
    coefficient = WGS84_F / 16.0 * cos_sq_alpha * (4.0 + WGS84_F * (4.0 - 3.0 * cos_sq_alpha))
    correction = (1.0 - coefficient) * WGS84_F * sin_alpha * (
        sigma + coefficient * math.sin(sigma) * (
            math.cos(2.0 * sigma_one + sigma)
            + coefficient * math.cos(sigma) * (-1.0 + 2.0 * math.cos(2.0 * sigma_one + sigma) ** 2)
        )
    )
    return [origin[0] + math.degrees(longitude - correction), math.degrees(latitude)]


def _non_collinear(points: list[list[float]]) -> bool:
    return any(
        abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) > 1.0
        for a, b, c in itertools.combinations(points, 3)
    )


def qualify_control_network(repo_root: Path, profile_path: Path) -> tuple[dict[str, Any], Path]:
    profile = read_json(profile_path)
    validate_document(profile, profile_path)
    controls = profile["controls"]
    identifiers = [control["control_id"] for control in controls]
    if len(set(identifiers)) != len(identifiers) or profile["product_center_control_id"] not in identifiers:
        raise CompilerError("control_identity_invalid", "Control IDs must be unique and include the product centre")
    expected = [[float(value) for value in control["expected_projected"]] for control in controls]
    if not _non_collinear(expected) or sum(control["role"] == "boundary" for control in controls) < 2:
        raise CompilerError("control_geometry_invalid", "Control network requires two boundary controls and non-collinearity")
    try:
        projected = transform_points(
            repo_root, [control["wgs84"] for control in controls], profile["horizontal_crs"], profile["projected_crs"]
        )
        inverse = transform_points(repo_root, expected, profile["projected_crs"], profile["horizontal_crs"])
        environment = execution_identity(repo_root, True)
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error)) from error

    gates = profile["gates"]
    quantization = float(profile["coordinate_quantization_m"])
    results = []
    forward_errors: list[float] = []
    inverse_errors: list[float] = []
    for control, actual, restored in zip(controls, projected, inverse):
        accuracy = control["horizontal_provenance"]["accuracy_m"]
        if accuracy is None:
            raise CompilerError("control_accuracy_unknown", "Control horizontal accuracy is unknown", control_id=control["control_id"])
        class_limit = HORIZONTAL_CLASS_LIMITS_M[control["horizontal_class"]]
        if class_limit is not None and control["horizontal_tolerance_m"] > class_limit:
            raise CompilerError(
                "control_horizontal_class_error",
                "Control tolerance exceeds its horizontal class limit",
                control_id=control["control_id"],
            )
        forward_error = math.dist(actual, control["expected_projected"])
        inverse_error = geodesic_distance(restored, control["wgs84"])
        numeric_error = max(forward_error, inverse_error)
        total_error = float(accuracy) + numeric_error + quantization
        if numeric_error > gates["numeric_forward_inverse_m"]:
            raise CompilerError("control_numeric_error", "Control transform exceeds the numeric gate", control_id=control["control_id"])
        if forward_error > float(accuracy) + gates["control_residual_allowance_m"]:
            raise CompilerError("control_residual_error", "Control residual exceeds source accuracy", control_id=control["control_id"])
        if total_error > control["horizontal_tolerance_m"]:
            raise CompilerError("control_horizontal_class_error", "Control exceeds its horizontal class", control_id=control["control_id"])
        vertical = control["vertical"]
        if vertical["mode"] == "absolute" and (vertical["provenance_ref"] is None or vertical["accuracy_m"] is None):
            raise CompilerError("control_vertical_error", "Absolute Z requires qualified vertical provenance", control_id=control["control_id"])
        if vertical["mode"] == "unknown" and (vertical["provenance_ref"] is not None or vertical["accuracy_m"] is not None):
            raise CompilerError("control_vertical_error", "Unknown Z cannot claim vertical provenance", control_id=control["control_id"])
        forward_errors.append(forward_error)
        inverse_errors.append(inverse_error)
        results.append({
            "control_id": control["control_id"],
            "forward_error_m": forward_error,
            "inverse_error_m": inverse_error,
            "source_accuracy_m": accuracy,
            "worst_case_horizontal_error_m": total_error,
            "horizontal_class": control["horizontal_class"],
            "horizontal_accepted": True,
            "absolute_z_qualified": vertical["mode"] == "absolute",
        })

    pairwise_errors = [
        abs(math.dist(projected[left], projected[right]) - math.dist(expected[left], expected[right]))
        for left, right in itertools.combinations(range(len(controls)), 2)
    ]
    max_pairwise = max(pairwise_errors, default=0.0)
    max_boundary = max(
        (forward_errors[index] for index, control in enumerate(controls) if control["role"] == "boundary"),
        default=0.0,
    )
    center = next(control for control in controls if control["control_id"] == profile["product_center_control_id"])
    radius = float(profile["product_radius_m"])
    circle = [geodesic_destination(center["wgs84"], radius, bearing) for bearing in range(0, 360, 45)]
    projected_circle = transform_points(repo_root, [center["wgs84"], *circle], profile["horizontal_crs"], profile["projected_crs"])
    scale_deltas = [abs(math.dist(projected_circle[0], point) - radius) for point in projected_circle[1:]]
    max_scale_delta = max(scale_deltas)
    if max_pairwise > gates["pairwise_distance_error_m"]:
        raise CompilerError("control_pairwise_error", "Control pairwise distance exceeds its gate")
    if max_boundary > gates["boundary_corner_error_m"]:
        raise CompilerError("control_boundary_error", "Boundary control exceeds its gate")
    if max_scale_delta > gates["projection_scale_delta_m"]:
        raise CompilerError("projection_scale_error", "UTM scale delta exceeds its product-radius gate")

    implementation_root = Path(__file__).resolve().parents[1]
    implementation_hash = canonical_hash({
        "files": {
            path.relative_to(implementation_root).as_posix(): file_hash(path)
            for path in (
                Path(__file__).resolve(),
                implementation_root / "contracts" / "control-network.schema.json",
                implementation_root / "contracts" / "control-network-receipt.schema.json",
            )
        }
    })
    receipt_inputs_hash = canonical_hash({
        "profile_sha256": file_hash(profile_path),
        "implementation_sha256": implementation_hash,
        "execution_identity_sha256": environment["identity_sha256"],
    })
    receipt = {
        "$schema": RECEIPT_SCHEMA,
        "schema_version": 1,
        "operation": "qualify_control_network",
        "status": "accepted",
        "control_network_id": profile["control_network_id"],
        "profile_sha256": file_hash(profile_path),
        "implementation_sha256": implementation_hash,
        "inputs_hash": receipt_inputs_hash,
        "execution_identity": environment,
        "controls": results,
        "metrics": {
            "max_forward_error_m": max(forward_errors),
            "rms_forward_error_m": math.sqrt(sum(value**2 for value in forward_errors) / len(forward_errors)),
            "max_inverse_error_m": max(inverse_errors),
            "max_pairwise_distance_error_m": max_pairwise,
            "max_boundary_corner_error_m": max_boundary,
            "max_projection_scale_delta_m": max_scale_delta,
            "projection_radius_m": radius,
            "non_collinear": True,
        },
        "errors": [],
    }
    output_root = (
        repo_root / "tmp" / "world" / "canonical_compilation" / "control_network"
        / profile["control_network_id"] / receipt_inputs_hash
    )
    receipt_path = output_root / "control_network_receipt.json"
    if receipt_path.is_file():
        existing = read_json(receipt_path)
        validate_document(existing, receipt_path)
        if canonical_hash(existing) != canonical_hash(receipt):
            raise CompilerError("control_receipt_conflict", "Existing control receipt differs at the same identity")
        return existing, receipt_path
    write_json(receipt_path, receipt)
    return receipt, receipt_path
