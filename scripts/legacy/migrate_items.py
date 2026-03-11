#!/usr/bin/env python3
"""
Migrate legacy Items.json (UTF-16) to new per-item JSON files.

Usage:
    python scripts/migrate_items.py <path_to_old_Items.json>

Output:
    Creates individual JSON files in Plugins/Resources/ProjectItems/SOT/Items/
"""

import json
import os
import re
import sys
from pathlib import Path

# Output directory
OUTPUT_DIR = Path(__file__).parent.parent / "Plugins/Resources/ProjectItems/SOT/Items"

# Quality -> Tag mapping
QUALITY_TAGS = {
    "Poor": "Item.Quality.Poor",
    "Common": "Item.Quality.Common",
    "Rare": "Item.Quality.Rare",
    "Epic": "Item.Quality.Epic",
    "Legendary": "Item.Quality.Legendary",
}

# ItemType -> Tag mapping
ITEM_TYPE_TAGS = {
    "Consumable": "Item.Type.Consumable",
    "Equipment": "Item.Type.Equipment",
    "Weapon": "Item.Type.Weapon",
    "Armor": "Item.Type.Armor",
    "Backpack": "Item.Type.Backpack",
}

# EquipmentSlot -> slotTag mapping
SLOT_TAGS = {
    "Head": "Item.EquipmentSlot.Head",
    "Chest": "Item.EquipmentSlot.Chest",
    "Back": "Item.EquipmentSlot.Back",
    "Main-Hand": "Item.EquipmentSlot.MainHand",
    "MainHand": "Item.EquipmentSlot.MainHand",
    "Off-Hand": "Item.EquipmentSlot.OffHand",
    "OffHand": "Item.EquipmentSlot.OffHand",
    "Legs": "Item.EquipmentSlot.Legs",
    "Feet": "Item.EquipmentSlot.Feet",
}


def extract_asset_path(ue_path: str) -> str:
    """
    Extract clean asset path from UE full reference.
    "/Script/Engine.StaticMesh'/Game/Path/SM_Item.SM_Item'" -> "/Game/Path/SM_Item"
    """
    if not ue_path or ue_path == "None":
        return ""

    # Match pattern: '...'/Game/Path/Asset.Asset'
    match = re.search(r"'(/Game/[^']+)'", ue_path)
    if match:
        full_path = match.group(1)
        # Remove .AssetName suffix if present
        if '.' in full_path:
            full_path = full_path.rsplit('.', 1)[0]
        return full_path

    return ue_path


def convert_id_to_pascal(old_id: str) -> str:
    """
    Convert old ID format to PascalCase.
    "Food_Apple" -> "FoodApple"
    "Weapon_Rifle_Assault" -> "WeaponRifleAssault"
    """
    parts = old_id.split('_')
    return ''.join(part.capitalize() for part in parts)


def convert_item(old_item: dict) -> dict:
    """Convert a single old item to new schema format."""

    old_id = old_item.get("ID", old_item.get("Name", "Unknown"))
    new_id = convert_id_to_pascal(old_id)

    # Build tags list
    tags = []

    # Quality tag
    quality = old_item.get("Quality", "")
    if quality in QUALITY_TAGS:
        tags.append(QUALITY_TAGS[quality])

    # ItemType tag
    item_type = old_item.get("ItemType", "")
    if item_type in ITEM_TYPE_TAGS:
        tags.append(ITEM_TYPE_TAGS[item_type])

    # Build new item
    new_item = {
        "$schema": "../schema/item.schema.json",
        "id": new_id,
        "displayName": old_item.get("Name", "").replace("_", " "),
        "description": old_item.get("Description", ""),
    }

    # Icon
    icon_path = extract_asset_path(old_item.get("Icon", ""))
    if icon_path:
        new_item["icon"] = icon_path

    # World mesh
    world_mesh = extract_asset_path(old_item.get("WorldMesh", ""))
    if world_mesh:
        new_item["world"] = {
            "staticMesh": world_mesh
        }

    # Stack settings
    is_stackable = old_item.get("IsStackable", False)
    max_stack = old_item.get("MaxStackSize", 0)
    if is_stackable and max_stack > 1:
        new_item["maxStack"] = max_stack
    else:
        new_item["maxStack"] = 1

    # Droppable
    if not old_item.get("IsDroppable", True):
        new_item["canBeDropped"] = False

    # Tags
    if tags:
        new_item["tags"] = tags

    # Consumable magnitudes (only if item has values)
    health = old_item.get("Health", 0)
    energy = old_item.get("Energy", 0)
    thirst = old_item.get("Thirst", 0)
    sleep = old_item.get("Sleep", 0)

    if item_type == "Consumable" and (health or energy or thirst or sleep):
        magnitudes = {}
        if health:
            magnitudes["SetByCaller.Health"] = float(health)
        if energy:
            magnitudes["SetByCaller.Energy"] = float(energy)
        if thirst:
            # Note: positive thirst in old = reduces thirst = negative in new schema
            magnitudes["SetByCaller.Thirst"] = float(-thirst)
        if sleep:
            magnitudes["SetByCaller.Sleep"] = float(sleep)

        if magnitudes:
            new_item["consumable"] = {
                "consumeOnUse": True,
                "effects": ["/Game/GAS/Effects/GE_GenericInstant"],
                "magnitudes": magnitudes
            }

    # Equipment
    weapon_actor = old_item.get("WeaponActorClass", "None")
    equip_slot = old_item.get("EquipmentSlot", "")
    equip_type = old_item.get("EquipmentType", "")

    is_equipment = (
        item_type in ["Equipment", "Weapon", "Armor", "Backpack"] or
        (weapon_actor and weapon_actor != "None") or
        (equip_type and equip_type != "Armor")  # Default is "Armor", ignore unless meaningful
    )

    if is_equipment:
        equipment = {}

        # Slot tag
        if equip_slot in SLOT_TAGS:
            equipment["slotTag"] = SLOT_TAGS[equip_slot]

        # Equipped actor class (for weapons)
        actor_path = extract_asset_path(weapon_actor)
        if actor_path:
            equipment["equippedActorClass"] = actor_path

        # TODO: abilitySet would need to be created separately
        # equipment["abilitySet"] = f"/Game/GAS/AbilitySets/AS_{new_id}"

        if equipment:
            new_item["equipment"] = equipment

    return new_item


def main():
    if len(sys.argv) < 2:
        print("Usage: python scripts/migrate_items.py <path_to_old_Items.json>")
        sys.exit(1)

    input_path = Path(sys.argv[1])

    if not input_path.exists():
        print(f"Error: File not found: {input_path}")
        sys.exit(1)

    # Read UTF-16 encoded file
    print(f"Reading: {input_path}")
    with open(input_path, 'r', encoding='utf-16') as f:
        old_items = json.load(f)

    print(f"Found {len(old_items)} items")

    # Ensure output directory exists
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Convert each item
    converted = 0
    skipped = 0

    for old_item in old_items:
        try:
            new_item = convert_item(old_item)

            # Generate filename from old ID (lowercase with underscores)
            old_id = old_item.get("ID", old_item.get("Name", "unknown"))
            filename = old_id.lower() + ".json"
            output_path = OUTPUT_DIR / filename

            # Write JSON file
            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(new_item, f, indent=2, ensure_ascii=False)

            print(f"  Created: {filename} -> {new_item['id']}")
            converted += 1

        except Exception as e:
            print(f"  Error converting {old_item.get('ID', 'unknown')}: {e}")
            skipped += 1

    print(f"\nMigration complete: {converted} converted, {skipped} skipped")
    print(f"Output directory: {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
