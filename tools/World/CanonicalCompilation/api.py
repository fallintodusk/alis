"""Public canonical-authority surface for other World components."""

from pathlib import Path

from .app.contracts import CompilerError as CanonicalCompilationError
from .app.incremental import terrain_impact_cells
from .app.pipeline import load_profile
from .app.terrain_refinement import refine_terrain_cells
from .app.terrain_refinement_artifacts import prepare_terrain_refinement
from .app.promotion import (
    canonical_active_path,
    materialize_canonical,
    validate_canonical_authority,
)


def terrain_impact_cells_for_profile(
    profile_path: Path,
    bounds: tuple[float, float, float, float],
) -> tuple[tuple[int, int], ...]:
    profile = load_profile(profile_path)
    return tuple(sorted(terrain_impact_cells(profile, [bounds])))


__all__ = (
    "CanonicalCompilationError",
    "canonical_active_path",
    "materialize_canonical",
    "refine_terrain_cells",
    "prepare_terrain_refinement",
    "terrain_impact_cells_for_profile",
    "validate_canonical_authority",
)
