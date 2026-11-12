"""Invalidate only Water fits whose previously consumed Terrain samples changed."""
from .water_geometry import TerrainSampler, polygon_sampling_dependencies


def changed_water_sources(repo_root, profile, previous_terrain, current_terrain, features, scratch_root):
    candidates = [feature for rows in features.by_owner.values() for feature in rows
                  if feature.get('feature_class') == 'water'
                  and feature.get('attributes', {}).get('surface_function', {}).get('vertical_basis')
                  in ('canonical_terrain_polygon_quantile', 'canonical_terrain_robust_fit')]
    if not candidates:
        return set()
    previous = TerrainSampler.from_cells(previous_terrain)
    current = TerrainSampler.from_cells(current_terrain)
    if previous.values == current.values:
        return set()
    polygons = {feature['feature_id']: feature['geometry'] for feature in candidates
                if feature['attributes']['surface_function']['vertical_basis'] == 'canonical_terrain_polygon_quantile'}
    points, fallbacks = ({}, {})
    if polygons:
        points, fallbacks = polygon_sampling_dependencies(
            repo_root, scratch_root, polygons, previous.core_view(), profile['grid']['canonical_crs'])
    changed = set()
    for feature in candidates:
        identity = feature['feature_id']
        function = feature['attributes']['surface_function']
        if identity in polygons:
            selected = points[identity]
            affected = (any(previous.values[xy] != current.values.get(xy) for xy in selected) if selected else
                        previous.core_view().sample(fallbacks[identity]) != current.core_view().sample(fallbacks[identity]))
        else:
            affected = any(previous.sample(knot[:2]) != current.sample(knot[:2]) for knot in function['knots'])
        if affected:
            changed.update(ref['provider_feature_id'] for ref in feature['source_refs'])
            changed.update(feature['attributes'].get('surface_group_members', []))
    return changed
