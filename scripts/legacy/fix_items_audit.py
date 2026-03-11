#!/usr/bin/env python3
"""
Fix all item JSON issues found in audit:
1. Unwrap mesh paths from /Script/Engine.StaticMesh'...' format
2. Fix ID casing to proper PascalCase
3. Add missing equipment blocks with slot tags
"""

import json
import re
from pathlib import Path

ITEMS_DIR = Path(r"<project-root>\Plugins\Resources\ProjectItems\SOT\Items")

# ID casing fixes: old -> new
ID_CASING_FIXES = {
    "Breadbig": "BreadBig",
    "Breadblackquarters": "BreadBlackQuarters",
    "Braisedbeans": "BraisedBeans",
    "Breadslice": "BreadSlice",
    "Chiliconcarne": "ChiliConCarne",
    "Cigarettepacket": "CigarettePacket",
    "Fishpaste": "FishPaste",
    "Hambone": "HamBone",
    "Peasstew": "PeasStew",
    "Stewedbeef": "StewedBeef",
    "Waterbottle": "WaterBottleMedium",
    "Waterbottlebig": "WaterBottleBig",
    "Waterbottlesmall": "WaterBottleSmall",
}

# Equipment items missing slot tags
EQUIPMENT_SLOTS = {
    "BoneAmulet": "Item.EquipmentSlot.Neck",
    "LeatherPouches": "Item.EquipmentSlot.Belt",
    "LeatherRing": "Item.EquipmentSlot.Finger",
    "Lighter": "Item.EquipmentSlot.Pocket",
    "SpikedRing": "Item.EquipmentSlot.Finger",
}


def unwrap_mesh_path(path: str) -> str:
    """
    Convert /Script/Engine.StaticMesh'/Path/To/SM_Asset.SM_Asset'
    to /Path/To/SM_Asset
    """
    match = re.match(r"/Script/Engine\.StaticMesh'([^']+)'", path)
    if match:
        full_path = match.group(1)
        # Remove the .AssetName suffix (e.g., /Path/SM_Foo.SM_Foo -> /Path/SM_Foo)
        if '.' in full_path.split('/')[-1]:
            full_path = full_path.rsplit('.', 1)[0]
        return full_path
    return path


def fix_id_casing(item_id: str) -> str:
    """Fix ID to proper PascalCase."""
    return ID_CASING_FIXES.get(item_id, item_id)


def fix_display_name(item_id: str, display_name: str) -> str:
    """Fix display name based on new ID."""
    # Map of new IDs to proper display names
    display_fixes = {
        "BreadBig": "Big Bread",
        "BreadBlackQuarters": "Black Bread Quarters",
        "BraisedBeans": "Braised Beans",
        "BreadSlice": "Bread Slice",
        "ChiliConCarne": "Chili Con Carne",
        "CigarettePacket": "Cigarette Packet",
        "FishPaste": "Fish Paste",
        "HamBone": "Ham Bone",
        "PeasStew": "Peas Stew",
        "StewedBeef": "Stewed Beef",
        "WaterBottleMedium": "Water Bottle (Medium)",
        "WaterBottleBig": "Water Bottle (Big)",
        "WaterBottleSmall": "Water Bottle (Small)",
    }
    return display_fixes.get(item_id, display_name)


def process_item(file_path: Path) -> tuple[bool, list[str]]:
    """Process a single item JSON file. Returns (modified, changes)."""
    changes = []

    with open(file_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    original = json.dumps(data, indent=2)

    # 1. Fix ID casing
    old_id = data.get("id", "")
    new_id = fix_id_casing(old_id)
    if new_id != old_id:
        data["id"] = new_id
        changes.append(f"ID: {old_id} -> {new_id}")

        # Also fix displayName if needed
        old_display = data.get("displayName", "")
        new_display = fix_display_name(new_id, old_display)
        if new_display != old_display:
            data["displayName"] = new_display
            changes.append(f"displayName: {old_display} -> {new_display}")

    # 2. Unwrap mesh paths
    if "world" in data and "staticMesh" in data["world"]:
        old_mesh = data["world"]["staticMesh"]
        new_mesh = unwrap_mesh_path(old_mesh)
        if new_mesh != old_mesh:
            data["world"]["staticMesh"] = new_mesh
            changes.append(f"staticMesh: unwrapped")

    # 3. Add missing equipment block
    current_id = data.get("id", "")
    if current_id in EQUIPMENT_SLOTS:
        if "equipment" not in data:
            data["equipment"] = {"slotTag": EQUIPMENT_SLOTS[current_id]}
            changes.append(f"Added equipment.slotTag: {EQUIPMENT_SLOTS[current_id]}")

    # Check if modified
    modified = json.dumps(data, indent=2) != original

    if modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
            f.write('\n')

    return modified, changes


def main():
    print("Fixing item JSON files...\n")

    total_modified = 0
    all_changes = []

    for json_file in ITEMS_DIR.rglob("*.json"):
        modified, changes = process_item(json_file)
        if modified:
            total_modified += 1
            rel_path = json_file.relative_to(ITEMS_DIR)
            all_changes.append((rel_path, changes))
            print(f"✓ {rel_path}")
            for change in changes:
                print(f"    - {change}")

    print(f"\n{'='*50}")
    print(f"Modified: {total_modified} files")

    if total_modified == 0:
        print("No changes needed - all files are clean!")


if __name__ == "__main__":
    main()
