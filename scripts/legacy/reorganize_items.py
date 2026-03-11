#!/usr/bin/env python3
"""
Reorganize SOT/Items into category subfolders.

Structure:
  equipment/ - equip_*, armor_*
  consumable/ - food_*, drugs_*, water_*, health_potion
  currency/ - currency_*
  weapon/ - weapon_*
  misc/ - misc_*, anything else
"""

import os
import shutil
from pathlib import Path

ITEMS_DIR = Path(__file__).parent.parent / "Plugins/Resources/ProjectItems/SOT/Items"

# Mapping: prefix -> (folder, strip_prefix)
CATEGORY_MAP = {
    "equip_": ("equipment", True),
    "armor_": ("equipment", True),
    "food_": ("consumable", True),
    "drugs_": ("consumable", True),
    "water_": ("consumable", True),
    "health_": ("consumable", True),
    "currency_": ("currency", True),
    "weapon_": ("weapon", True),
    "misc_": ("misc", True),
}

def get_category(filename: str) -> tuple[str, str]:
    """Return (category_folder, new_filename) for a given filename."""
    for prefix, (folder, strip) in CATEGORY_MAP.items():
        if filename.lower().startswith(prefix):
            if strip:
                new_name = filename[len(prefix):]
            else:
                new_name = filename
            return folder, new_name

    # Default to misc
    return "misc", filename

def main():
    if not ITEMS_DIR.exists():
        print(f"Error: {ITEMS_DIR} does not exist")
        return

    # Get all JSON files (excluding schema folder)
    json_files = [f for f in ITEMS_DIR.glob("*.json")]

    print(f"Found {len(json_files)} JSON files to reorganize")

    # Create category folders
    categories = set(folder for folder, _ in CATEGORY_MAP.values())
    categories.add("misc")

    for cat in categories:
        (ITEMS_DIR / cat).mkdir(exist_ok=True)
        print(f"  Created folder: {cat}/")

    # Move files
    moved = 0
    for json_file in json_files:
        folder, new_name = get_category(json_file.name)
        dest_path = ITEMS_DIR / folder / new_name

        # Move file
        shutil.move(str(json_file), str(dest_path))
        print(f"  {json_file.name} -> {folder}/{new_name}")
        moved += 1

    print(f"\nReorganization complete: {moved} files moved")

if __name__ == "__main__":
    main()
