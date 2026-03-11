#!/usr/bin/env python3
"""
Fix migrated items:
1. Update $schema path for subfolder structure
2. Add default weight/volume if missing
"""

import json
from pathlib import Path

ITEMS_DIR = Path(__file__).parent.parent / "Plugins/Resources/ProjectItems/SOT/Items"

# Default values for missing fields
DEFAULTS = {
    "weight": 0.1,
    "volume": 0.05,
}

def fix_item(file_path: Path) -> bool:
    """Fix a single item file. Returns True if modified."""
    with open(file_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    modified = False

    # Fix $schema path for subfolder structure
    if "$schema" in data:
        old_schema = data["$schema"]
        # Items are now in subfolders, need ../../schema/
        if old_schema == "../schema/item.schema.json":
            data["$schema"] = "../../schema/item.schema.json"
            modified = True

    # Add default weight if missing
    if "weight" not in data:
        # Insert after description or icon
        data["weight"] = DEFAULTS["weight"]
        modified = True

    # Add default volume if missing
    if "volume" not in data:
        data["volume"] = DEFAULTS["volume"]
        modified = True

    if modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

    return modified

def main():
    if not ITEMS_DIR.exists():
        print(f"Error: {ITEMS_DIR} does not exist")
        return

    # Find all JSON files in subfolders
    json_files = list(ITEMS_DIR.glob("**/*.json"))

    print(f"Found {len(json_files)} JSON files")

    fixed = 0
    for json_file in json_files:
        try:
            if fix_item(json_file):
                rel_path = json_file.relative_to(ITEMS_DIR)
                print(f"  Fixed: {rel_path}")
                fixed += 1
        except Exception as e:
            print(f"  Error in {json_file.name}: {e}")

    print(f"\nFixed {fixed} files")

if __name__ == "__main__":
    main()
