from __future__ import annotations

import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, run_tool


def parse_building_relation_memberships(
    path: Path,
    selected_feature_ids: set[str],
) -> dict[str, list[dict[str, str]]]:
    memberships: dict[str, set[tuple[str, str]]] = defaultdict(set)
    try:
        iterator = ET.iterparse(path, events=("end",))
        for _, element in iterator:
            if element.tag != "relation":
                continue
            tags = {item.attrib["k"]: item.attrib["v"] for item in element.findall("tag")}
            if tags.get("type") == "building":
                relation_id = f"relation/{element.attrib['id']}"
                for member in element.findall("member"):
                    role = member.attrib.get("role", "")
                    member_id = f"{member.attrib['type']}/{member.attrib['ref']}"
                    if role in {"outline", "part"} and member_id in selected_feature_ids:
                        memberships[member_id].add((relation_id, role))
            element.clear()
    except (ET.ParseError, KeyError, OSError) as exc:
        raise ExecutionEnvironmentError(
            "building_relation_parse_failed",
            "OSM building relation membership could not be parsed",
        ) from exc
    return {
        member_id: [
            {
                "provider_relation_id": relation_id,
                "relation_type": "building",
                "role": role,
            }
            for relation_id, role in sorted(values)
        ]
        for member_id, values in sorted(memberships.items())
    }


def extract_building_relation_memberships(
    repo_root: Path,
    source_path: Path,
    selected_feature_ids: set[str],
    scratch_root: Path,
) -> dict[str, list[dict[str, str]]]:
    scratch_root.mkdir(parents=True, exist_ok=True)
    relation_pbf = scratch_root / "type_building_relations.osm.pbf"
    relation_xml = scratch_root / "type_building_relations.osm"
    relation_pbf.unlink(missing_ok=True)
    relation_xml.unlink(missing_ok=True)
    try:
        run_tool(repo_root, "osmium", [
            "tags-filter", "-R", "-o", str(relation_pbf), str(source_path), "r/type=building",
        ])
        run_tool(repo_root, "osmium", [
            "cat", "-f", "osm", "-o", str(relation_xml), str(relation_pbf),
        ])
        return parse_building_relation_memberships(relation_xml, selected_feature_ids)
    finally:
        relation_pbf.unlink(missing_ok=True)
        relation_xml.unlink(missing_ok=True)
