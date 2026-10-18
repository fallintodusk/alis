"""Domain entrypoint for one bridge in the common surface-refinement operation."""
from pathlib import Path
from .surface_publication import prepare_surface_refinement


def prepare_bridge_refinement(repo_root: Path, base_result_path: Path, output_root: Path,
                              feature_id: str, context_path: Path, expected_context_sha256: str,
                              expected_context_bytes: int) -> tuple[dict, Path]:
    operation = {'kind': 'bridge_elevation', 'feature_ids': [feature_id],
        'context_path': context_path.resolve().relative_to(repo_root.resolve()).as_posix(),
        'sha256': expected_context_sha256, 'byte_size': expected_context_bytes}
    return prepare_surface_refinement(repo_root, base_result_path, output_root, [operation])
