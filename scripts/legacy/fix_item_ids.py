#!/usr/bin/env python3
"""
Fix item IDs - remove redundant prefixes and update schema paths.
"""

import json
import re
from pathlib import Path

ITEMS_DIR = Path(__file__).parent.parent / "Plugins/Resources/ProjectItems/SOT/Items"

# Prefixes to remove from IDs
REMOVE_PREFIXES = [
    "Armor",
    "Food",
    "Equip",
    "Currency",
    "Weapon",
    "Drugs",
    "Misc",
]

def clean_id(old_id: str) -> str:
    """Remove redundant prefix from ID."""
    for prefix in REMOVE_PREFIXES:
        if old_id.startswith(prefix) and len(old_id) > len(prefix):
            # Check if next char is uppercase (proper PascalCase)
            rest = old_id[len(prefix):]
            if rest[0].isupper():
                return rest
    return old_id

def clean_display_name(old_name: str) -> str:
    """Remove redundant prefix from display name."""
    for prefix in REMOVE_PREFIXES:
        patterns = [
            f"{prefix} ",      # "Food Apple"
            f"{prefix}_",      # "Food_Apple"
        ]
        for pattern in patterns:
            if old_name.startswith(pattern):
                return old_name[len(pattern):]
    return old_name

def fix_item(file_path: Path) -> tuple[bool, str]:
    """Fix a single item file. Returns (modified, change_description)."""
    with open(file_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    changes = []

    # Fix $schema path
    if "$schema" in data:
        old_schema = data["$schema"]
        if old_schema == "../schema/item.schema.json":
            data["$schema"] = "../../schema/item.schema.json"
            changes.append("schema path")

    # Fix ID
    if "id" in data:
        old_id = data["id"]
        new_id = clean_id(old_id)
        if new_id != old_id:
            data["id"] = new_id
            changes.append(f"id: {old_id} -> {new_id}")

    # Fix display name
    if "displayName" in data:
        old_name = data["displayName"]
        new_name = clean_display_name(old_name)
        if new_name != old_name:
            data["displayName"] = new_name
            changes.append(f"name: {old_name} -> {new_name}")

    # Add weight if missing
    if "weight" not in data:
        data["weight"] = 0.1
        changes.append("added weight")

    # Add volume if missing
    if "volume" not in data:
        data["volume"] = 0.05
        changes.append("added volume")

    if changes:
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        return True, ", ".join(changes)

    return False, ""

def main():
    if not ITEMS_DIR.exists():
        print(f"Error: {ITEMS_DIR} does not exist")
        return

    # Find all JSON files in subfolders
    json_files = list(ITEMS_DIR.glob("**/*.json"))

    print(f"Found {len(json_files)} JSON files\n")

    fixed = 0
    for json_file in json_files:
        try:
            modified, changes = fix_item(json_file)
            if modified:
                rel_path = json_file.relative_to(ITEMS_DIR)
                print(f"  {rel_path}: {changes}")
                fixed += 1
        except Exception as e:
            print(f"  Error in {json_file.name}: {e}")

    print(f"\nFixed {fixed} files")

if __name__ == "__main__":
    main()
