# Alis Content Folder Structure

This document provides a comprehensive overview of the `Content/` directory organization for the Alis project.

## Table of Contents

- [Overview](#overview)
- [Project Core (`Project/`)](#project-core-project)
- [Third-Party Assets](#third-party-assets)
- [Marketplace Plugins](#marketplace-plugins)
- [Developer Folders](#developer-folders)
- [Naming Conventions](#naming-conventions)

---

## Overview

The Content folder is organized into two main categories:

1. **Project/** - Core game content and custom-built assets
2. **Third-Party Assets** - Marketplace packs, plugins, and external content

### Root-Level Structure

```
Content/
+-- Project/                    # Core game content (PRIMARY WORK AREA)
+-- InventorySystem/            # Inventory plugin assets
+-- AdvancedGlassPack/          # Glass materials marketplace pack
+-- MaleActionHeroes/           # Character models pack
+-- Personage/                  # Character customization
+-- Megascans/                  # Quixel Megascans assets
+-- M_S_House/                  # Modular house assets
+-- Custom/                     # Custom materials library
+-- Materials/                  # Shared materials
+-- Widgets/                    # Top-level UI widgets
+-- Localization/               # Localization files
+-- Collections/                # Asset collections
+-- Developers/                 # Per-developer workspaces
+-- [Other Asset Packs]/        # Various marketplace content
```

---

## Project Core (`Project/`)

The **Project/** folder contains all custom game content and is the primary work area for development.

### Project/ Structure Overview

```
Project/
+-- Core/                       # Fundamental game systems
+-- Maps/                       # Game levels
+-- Placeables/                 # Interactive world objects
+-- NPC/                        # Non-player characters
+-- UI/                         # User interface
+-- Resources/                  # Shared resources and assets
+-- Inventory/                  # Inventory system integration
+-- Journal/                    # Quest/journal system
+-- Audio/                      # Game audio
+-- Save/                       # Save game data
+-- Test/                       # Development testing
+-- Utility/                    # Utility blueprints
```

---

### 1. Core Systems (`Project/Core/`)

Foundation systems that power the entire game.

```
Core/
+-- System/                     # Game instance, managers, singletons
|   +-- BP_AlisGInstance        # Main game instance (extends C++ UAlisGI)
+-- Components/                 # Reusable actor components
+-- Interfaces/                 # Blueprint interfaces for communication
+-- Templates/                  # Base classes for actors
```

**Purpose:**
- **System/** - Persistent game state, managers, core singletons
- **Components/** - Modular actor components (health, inventory, interaction, etc.)
- **Interfaces/** - Communication contracts between Blueprints
- **Templates/** - Abstract base classes for specialized actors

---

### 2. Maps (`Project/Maps/`)

Game levels and world streaming setup.

```
Maps/
+-- KazanMain/                  # Primary game world (Kazan city)
|   +-- Streaming/              # Level streaming sub-levels
+-- Hrush/                      # Secondary location
|   +-- Streaming/              # Level streaming sub-levels
+-- Map_Main_Menu.umap          # Main menu level
```

**Key Maps:**
- **KazanMain** - Main game world, post-apocalyptic Russian city
- **Hrush** - Additional exploration area
- **Map_Main_Menu** - UI-only level for main menu

**Streaming Architecture:**
The maps use Unreal's level streaming for performance optimization. Sub-levels in `Streaming/` folders are loaded/unloaded dynamically.

---

### 3. Placeables (`Project/Placeables/`)

All interactive and environmental world objects.

```
Placeables/
+-- Environment/                # Environmental systems and decorations
|   +-- DayNightSystem/         # Dynamic day/night cycle
|   +-- FogSheet/               # Volumetric fog effects
|       +-- Blueprints/         # Fog actor logic
|       +-- Materials/          # Fog shaders
|       +-- Meshes/             # Fog plane meshes
|       +-- Textures/           # Fog patterns
+-- Interaction/                # Interactive objects
|   +-- Doors/                  # Door systems
|   +-- Area/                   # Trigger volumes, interactive zones
+-- InventorySystem/            # Loot containers, pickups
+-- Effects/                    # Visual effects
```

**Organization Pattern:**
Each placeable category follows the structure:
- **Blueprints/** - Actor logic
- **Materials/** - Visual appearance
- **Meshes/** - 3D models
- **Textures/** - Texture maps

---

### 4. NPCs (`Project/NPC/`)

Non-player character systems.

```
NPC/
+-- Dialogue/                   # Dialogue system assets
```

**Integration:**
Uses the **DialoguePlugin** (marketplace plugin) for conversation trees and quest triggers.

---

### 5. User Interface (`Project/UI/`)

All UI widgets, screens, and HUD elements.

```
UI/
+-- CustomUI/                   # Custom UI system
    +-- Blueprints/             # UI logic and controllers
    +-- Widgets/                # Reusable UI widgets
    |   +-- Backgrounds/
    |   +-- Buttons/
    |   |   +-- BaseClass/      # Base button widget
    |   |   +-- Buttons/        # Specific button types
    |   +-- CheckBox/
    |   +-- ComboBox/
    |   +-- RadioButtons/
    |   +-- Sliders/
    |   +-- TextBox/
    |   +-- Hints/              # Tooltips and hints
    |   +-- Menus/              # Menu screens
    |   +-- Popups/             # Dialog boxes
    |   +-- Screens/            # Full-screen UIs
    |   +-- TitleBar/           # Window title bars
    |   +-- WidgetAnimator/     # Animation system for widgets
    +-- Materials/              # UI materials and effects
    |   +-- Backgrounds/
    |   +-- Buttons/
    |   +-- Icons/
    |   +-- ProgressBars/
    |   +-- _BaseClass/         # Base material classes
    +-- Textures/               # UI textures
    |   +-- backgrounds/
    |   +-- borders/
    |   +-- buttons/
    |   +-- icons/
    +-- Fonts/                  # Font assets
    |   +-- RobotoCondensed/
    +-- Sounds/                 # UI audio feedback
    |   +-- Cues/               # Sound cues
    |   +-- Wavs/               # Wave files
    +-- Levels/                 # UI test levels
    +-- Materials/              # Shader effects for UI
```

**UI Widget Hierarchy:**
- **BaseClass/** folders contain abstract parent widgets
- Specific widgets inherit from base classes
- Promotes reusability and consistent styling

---

### 6. Resources (`Project/Resources/`)

Shared assets, materials, and game objects.

```
Resources/
+-- Objects/                    # All in-game objects
|   +-- HumanMade/              # Man-made objects
|   |   +-- Building/           # Buildings and structures
|   |   |   +-- Apartment/      # Apartment buildings
|   |   |   +-- House/          # Houses
|   |   |   +-- Industrial/     # Industrial buildings
|   |   |   +-- SmallArchForm/  # Small structures
|   |   |   +-- Elements/       # Building components
|   |   |       +-- Fence/
|   |   |       +-- Stairs/
|   |   |       +-- Wall/
|   |   +-- Furniture/          # Indoor furniture
|   |   |   +-- ForSit/         # Chairs, benches
|   |   |   |   +-- Chair/
|   |   |   |   +-- Couch/
|   |   |   |   +-- Stool/
|   |   |   +-- ForSleep/       # Beds, mattresses
|   |   |   +-- Locker/         # Storage furniture
|   |   |   |   +-- Bookcase/
|   |   |   |   +-- Cabinet/
|   |   |   |   +-- Drawer/
|   |   |   |   +-- Wardrobe/
|   |   |   +-- Table/          # Tables and desks
|   |   |   +-- Bathroom/       # Bathroom fixtures
|   |   +-- Household/          # Household items
|   |   |   +-- Appliances/     # Appliances
|   |   |   |   +-- Kitchen/    # Kitchen appliances
|   |   |   |   |   +-- Fridge/
|   |   |   |   |   +-- Stove/
|   |   |   |   +-- Wash/       # Washing machines
|   |   |   +-- Container/      # Containers and boxes
|   |   |   |   +-- Basket/
|   |   |   |   +-- Bin/
|   |   |   |   +-- Bottle/
|   |   |   |   +-- Bucket/
|   |   |   |   +-- Can/
|   |   |   |   +-- Canister/
|   |   |   |   +-- Flask/
|   |   |   |   +-- Jar/
|   |   |   +-- Dishes/         # Tableware
|   |   |   |   +-- Cutlery/
|   |   |   |   +-- Drinkware/
|   |   |   |   +-- Plate/
|   |   |   +-- HouseholdItem/  # General household items
|   |   |   |   +-- Broom/
|   |   |   |   +-- Hanger/
|   |   |   |   +-- Trashbag/
|   |   |   +-- Lightening/     # Lighting fixtures
|   |   |   |   +-- Lamp/
|   |   |   +-- PersonalCare/   # Personal care items
|   |   |       +-- Towel/
|   |   +-- PersonalEquipment/  # Player equipment
|   |   |   +-- Backpack/
|   |   |   +-- Clothing/       # Wearable clothing
|   |   |   |   +-- Headdress/
|   |   |   +-- ProtectiveGear/ # Armor and protective gear
|   |   |   |   +-- Helmet/
|   |   |   |   +-- Mask/
|   |   |   +-- Shoes/
|   |   +-- Transport/          # Vehicles
|   |   |   +-- Land/
|   |   |   |   +-- PassengerCar/
|   |   |   |   |   +-- Crossover_City/
|   |   |   |   |   +-- Crossover_Family/
|   |   |   |   |   +-- Sedan/
|   |   |   |   |   +-- Sedan_Long/
|   |   |   |   |   +-- Sport_Car/
|   |   |   |   |   +-- Van/
|   |   |   |   +-- Parts/      # Vehicle parts
|   |   |   |       +-- Tire/
|   |   |   |       +-- Wheel/
|   |   |   +-- ToDo/           # Vehicles in development
|   |   +-- VitalNeed/          # Survival items
|   |   |   +-- Food/           # Food items
|   |   |   |   +-- Baked/      # Bread, pastries
|   |   |   |   +-- Canned/     # Canned goods
|   |   |   |   +-- Fruit/
|   |   |   |   +-- Meat/
|   |   |   |   +-- Mushroom/
|   |   |   |   +-- Vegetable/
|   |   |   +-- Drink/          # Beverages
|   |   |   |   +-- AlcoDrink/
|   |   |   |   +-- ColdDrink/
|   |   |   |   +-- HotDrink/
|   |   |   +-- MedicalCare/    # Medical items
|   |   |       +-- Medkit_1/
|   |   |       +-- Pill/
|   |   |       +-- PillBottle_1/
|   |   +-- WorkTool/           # Tools and utilities
|   |       +-- CarpentryTool/  # Carpentry tools
|   |       |   +-- Axe/        # Axes (multiple types)
|   |       +-- Cutting_Sawing/ # Cutting tools
|   |       |   +-- Hand/       # Knives
|   |       +-- EarthmovingTool/# Digging tools
|   |       |   +-- Shovel/
|   |       +-- Fasteners_Installation/ # Assembly tools
|   |       |   +-- Crowbar/
|   |       |   +-- Hammer/
|   |       |   +-- Key/        # Wrenches
|   |       |   +-- Screwdriver/
|   |       +-- Consumable/     # Consumable tools
|   |       |   +-- DuctTape_1/
|   |       |   +-- LuciferMatch/
|   |       |   +-- Nail/
|   |       +-- Communication_Gear/ # Communication devices
|   |       |   +-- Telecom_Device/
|   |       +-- SurveillanceGear/ # Surveillance equipment
|   +-- Nature/                 # Natural objects
|       +-- ExteriorPlant/      # Outdoor plants
|       |   +-- Tree/           # Trees
|       |   |   +-- AmurCork/
|       |   |   +-- Hornbeam/
|       |   +-- Bush/           # Bushes and shrubs
|       |   |   +-- Fern/
|       |   |   +-- RedBerry/
|       |   |   +-- Taro/
|       |   +-- Grass/          # Ground vegetation
|       |   |   +-- Weed/
|       |   |   +-- YellowArchangel/
|       |   +-- Flower/         # Flowers
|       |       +-- Crownbeard/
|       |       +-- WhiteWind/
|       +-- InteriorPlant/      # Indoor plants
|       +-- Mushroom/           # Mushrooms
|       +-- Rock/               # Rocks and stones
|       |   +-- Flint_1/
|       |   +-- Rock_1/
|       +-- Landscape/          # Landscape materials
|       |   +-- Material/
|       |       +-- Texture/
|       |           +-- Asphalt_Damaged/
|       |           +-- Grass_Lawn/
|       |           +-- Soil_Leaf/
|       |           +-- Soil_Rock/
|       +-- ToDo/               # Plants being migrated
|           +-- Old_Plant/
+-- Texture/                    # Shared textures
|   +-- Base/                   # Base textures
|   +-- Cubemap/                # Environment maps
|   +-- Decal/                  # Decal textures
|   |   +-- 17_High_Velocity_Blood_Spatter_sgepbixp/
|   |   +-- 22_High_Velocity_Blood_Spatter_se1sfhkp/
|   |   +-- 25_High_Velocity_Blood_Spatter_sgekfhvp/
|   |   +-- Master_Materials/
|   |   +-- RoadDecal/
|   +-- Pattern/                # Tileable patterns
|   |   +-- Drop/
|   |   +-- Erosion/
|   |   +-- Fiber/
|   |   +-- Imperfection/
|   |   +-- Noise/
|   |   |   +-- Perlin/
|   |   +-- Rough/
|   |   +-- Scratch/
|   +-- ToDo/                   # Textures to organize
+-- UI/                         # UI-specific resources
|   +-- Hud/                    # HUD elements
|   +-- Menu/                   # Menu backgrounds
|       +-- Backgrounds/
+-- Utility/                    # Utility resources
    +-- GoogleMaps/             # Map reference materials
```

**Object Asset Pattern:**

Each object type follows a consistent structure:
```
ObjectName/
+-- BP_ObjectName.uasset       # Blueprint actor
+-- SM_ObjectName.uasset       # Static mesh
+-- M_ObjectName.uasset        # Material
+-- T_ObjectName.uasset        # Texture
```

**Naming Prefixes:**
- `BP_` - Blueprint
- `SM_` - Static Mesh
- `SK_` - Skeletal Mesh
- `M_` - Material
- `MI_` - Material Instance
- `T_` - Texture
- `DT_` - Data Table
- `E_` - Enum
- `F_` - Struct
- `WBP_` - Widget Blueprint

---

### 7. Inventory (`Project/Inventory/`)

Custom inventory system integration (interfaces with InventorySystem plugin).

---

### 8. Journal (`Project/Journal/`)

Quest journal and note-taking system.

---

### 9. Audio (`Project/Audio/`)

Game-specific audio assets (music, ambient sounds, effects).

---

### 10. Save (`Project/Save/`)

Save game data structures.

```
Save/
+-- SaveGameState/              # Save game blueprints
```

---

### 11. Test (`Project/Test/`)

Development testing area.

```
Test/
+-- Blueprints/                 # Test blueprints
+-- Maps/                       # Test levels
+-- Materials/                  # Material experiments
|   +-- MaterialAliceTest/
|   +-- Wind_Layer/
+-- Bunny_2/                    # Test models
+-- DVT/                        # Development verification tests
```

---

### 12. Utility (`Project/Utility/`)

Utility blueprints and helper functions.

---

## Third-Party Assets

### InventorySystem

Full-featured inventory plugin from the marketplace.

```
InventorySystem/
+-- Blueprints/                 # Core inventory logic
|   +-- Components/             # Inventory components
|   +-- Inventory/              # Inventory actors
|   +-- Usables/                # Usable items
|   +-- Weapons/                # Weapon system
|   +-- Demo/                   # Example implementations
|       +-- Example_01/
|       +-- Example_02/
+-- Items/                      # Item definitions
|   +-- Consumables/            # Food, potions, etc.
|   |   +-- Icons/
|   |   +-- Materials/
|   |   +-- Meshes/
|   |   +-- Textures/
|   +-- Equipment/              # Armor, gear
|   |   +-- Icons/
|   |   +-- Materials/
|   |   +-- Meshes/
|   |   +-- Textures/
|   +-- Weapons/                # Weapons
|   |   +-- Icon/
|   |   +-- Materials/
|   |   +-- Meshes/
|   |   +-- Textures/
|   +-- Miscellaneous/          # Other items
|   +-- LootTables/             # Random loot generation
+-- Character/                  # Character integration
|   +-- Materials/
|   +-- Mesh/
|   +-- Textures/
+-- UI/                         # Inventory UI
|   +-- Blueprints/
|   |   +-- Inventory/
|   |   +-- Demo/
|   +-- Textures/
+-- Environment/                # Loot containers
|   +-- Materials/
|   +-- Meshes/
|   +-- Textures/
+-- Animations/                 # Item animations
+-- Maps/                       # Demo maps
+-- Materials/                  # Shared materials
+-- Sounds/                     # Inventory sounds
    +-- Environment/
```

---

### AdvancedGlassPack

Professional glass materials with various effects.

```
AdvancedGlassPack/
+-- Materials/
|   +-- 01_Clean/               # Clean glass
|   |   +-- _Examples/
|   +-- 02_Dirty/               # Dirty glass
|   |   +-- _Examples/
|   +-- 03_Frozen/              # Frosted glass
|   |   +-- _Exmaples/
|   +-- 04_Shattered/           # Broken glass
|   |   +-- _Examples/
|   +-- 05_Wet/                 # Wet glass
|       +-- _Examples/
+-- StaticMesh/
|   +-- GlassBrick/             # Glass block meshes
|   +-- GlassPieces/            # Shattered pieces
+-- Textures/
|   +-- DirtMask/
|   +-- FrostMask/
|   +-- Patterns/
|   +-- RainMask/
|   +-- Scratches/
|   +-- ShatteredMask/
+-- Maps/                       # Demo scenes
```

---

### M_S_House

Modular house building system with interior materials.

```
M_S_House/
+-- Instances/                  # Material instances
|   +-- Walls/                  # Exterior walls
|   +-- Windows/                # Window materials
|   +-- Balcony/                # Balcony elements
|   +-- Basement/               # Basement materials
|   +-- Interior/               # Interior surfaces
|       +-- Ceiling/            # Ceiling materials
|       +-- Linoleum/           # Linoleum floors
|       +-- Parquet/            # Wood floors
|       +-- PlinthAndCeil/      # Trim and molding
|       +-- Tile/               # Tile floors
|       +-- Wallpapers/         # Wall coverings
+-- Materials/                  # Master materials
|   +-- MF/                     # Material functions
+-- Textures/                   # Texture library
    +-- Dummy/
    +-- InsideDetails/
    |   +-- Ceiling/
    |   +-- Decoration/
    |   +-- Linoleum/
    |   +-- Parquet/
    |   +-- Tile/
    |   +-- Wallpapers/
    +-- OutsideDetails/
    +-- Walls/
    +-- Windows/
    +-- Trash/                  # Clutter materials
        +-- Materials/
            +-- Instances/
            +-- Masters/
```

---

### Personage (Character System)

Character models and customization.

```
Personage/
+-- MaleHeroParts/              # Character parts
|   +-- materials/
|   +-- meshes/
|   +-- textures/
|   +-- functions/
|   +-- SSprofile/              # Subsurface scattering profiles
+-- Mannequin/                  # Base mannequin
|   +-- Animations/
|   +-- Character/
|       +-- Materials/
|       |   +-- MaterialLayers/
|       +-- Mesh/
|       +-- Textures/
+-- ThirdPerson/                # Third-person setup
|   +-- Meshes/
+-- ThirdPersonBP/              # Third-person blueprints
    +-- Blueprints/
    +-- Maps/
```

---

### Megascans

Quixel Megascans photorealistic assets.

```
Megascans/
+-- 3D_Plants/
    +-- AlexandraPalm/          # Example: Alexandra Palm tree
```

**Note:** Megascans assets follow Quixel's naming conventions and include high-quality materials with UE5 features (Nanite, Virtual Texturing).

---

### MSPresets

Megascans material presets for foliage.

```
MSPresets/
+-- MS_Foliage_Material_LATEST/
    +-- MaterialFunctions/
    |   +-- Foliage/
    |   +-- Textures/
    +-- MaterialParameterCollection/
```

---

### Custom

Custom material library for the project.

```
Custom/
+-- CustomMaterials/
    +-- Materials/
        +-- Master_Materials/
        |   +-- Custom_MS_Master/
        |       +-- M_MS_Foliage_Overwrite/ # Overrides for Megascans
        +-- Material_Functions/
```

---

### Materials

Shared materials used across the project.

```
Materials/
+-- Road_AsphaultRaceWayLines/  # Road line materials
```

---

### Other Marketplace Packs

#### MegaGameMusicCollection
```
MegaGameMusicCollection/
+-- 8BitActionGameLoops/
    +-- Cues/
    +-- Wavs/
```

#### UniversalSoundFX
```
UniversalSoundFX/
+-- Sound_Effects/
    +-- CREAKS/
    +-- DOORS_GATES_DRAWERS/
    +-- FABRIC_CLOTHING/
    +-- FOLEY/
    |   +-- CARDBOARD/
    +-- LOCKS_KEYS/
    +-- SQUEAKS/
```

#### TextureNoiseAndPatternPack01
```
TextureNoiseAndPatternPack01/
+-- Noise/                      # Noise textures
+-- Patterns/                   # Procedural patterns
```

#### MaleActionHeroes
```
MaleActionHeroes/
+-- MaleHeroParts/
|   +-- meshes/
+-- Mannequin/
    +-- Character/
        +-- Mesh/
```

---

## Developer Folders

Personal workspaces for team members (excluded from builds).

```
Developers/
+-- Diana/
|   +-- Resources/
|       +-- MaterialLibrary/
+-- <user>/
    +-- Collections/
```

**Purpose:** Safe area for experiments without affecting main project.

---

## Localization

Game text translations.

```
Localization/
+-- Game/
    +-- en-US/                  # English (US)
    +-- ru/                     # Russian
```

---

## Collections

Asset collections for organization.

```
Collections/                    # Project-wide collections
```

---

## Widgets (Root Level)

High-level widget organization (alternative to `Project/UI/`).

```
Widgets/
+-- Dialogue/                   # Dialogue UI widgets
+-- GameHUD/                    # In-game HUD
+-- Menu/                       # Menu screens
```

---

## Naming Conventions

### Asset Prefixes

| Prefix | Type | Example |
|--------|------|---------|
| `BP_` | Blueprint Class | `BP_Door`, `BP_Player` |
| `WBP_` | Widget Blueprint | `WBP_MainMenu`, `WBP_InventorySlot` |
| `SM_` | Static Mesh | `SM_Chair`, `SM_Building` |
| `SK_` | Skeletal Mesh | `SK_Character`, `SK_Mannequin` |
| `M_` | Material | `M_Glass`, `M_Wood` |
| `MI_` | Material Instance | `MI_Glass_Clean`, `MI_Wood_Oak` |
| `MF_` | Material Function | `MF_Weathering`, `MF_DetailLayer` |
| `T_` | Texture | `T_WoodDiffuse`, `T_MetalNormal` |
| `DT_` | Data Table | `DT_Items`, `DT_Quests` |
| `E_` | Enum | `E_ItemType`, `E_DoorState` |
| `F_` | Struct | `F_ItemData`, `F_QuestInfo` |
| `AC_` | Actor Component | `AC_Health`, `AC_Inventory` |

### Folder Organization Patterns

1. **By Asset Type:**
   ```
   ObjectName/
   +-- Blueprints/
   +-- Materials/
   +-- Meshes/
   +-- Textures/
   ```

2. **By Feature:**
   ```
   FeatureName/
   +-- Actors/
   +-- UI/
   +-- Data/
   +-- Audio/
   ```

3. **Plugin Pattern:**
   ```
   PluginName/
   +-- Core/
   +-- Content/
   +-- Demo/
   ```

---

## Best Practices

### Where to Add New Content

1. **Custom game objects** -> `Project/Resources/Objects/`
2. **Game systems** -> `Project/Core/`
3. **UI elements** -> `Project/UI/CustomUI/`
4. **Maps/Levels** -> `Project/Maps/`
5. **Shared textures** -> `Project/Resources/Texture/`
6. **Testing** -> `Project/Test/`

### Organization Guidelines

1. **Avoid root clutter** - Don't create folders at `Content/` root
2. **Use consistent naming** - Follow prefix conventions
3. **Group by feature** - Keep related assets together
4. **Separate plugin content** - Keep marketplace assets in their own folders
5. **Use BaseClass pattern** - Create base widgets/blueprints for inheritance
6. **Clean up ToDo folders** - Migrate or delete old assets regularly

### Migration Path

When importing marketplace content:
1. Keep in original folder structure
2. Create instances/overrides in `Project/` folders
3. Reference, don't duplicate
4. Document dependencies in asset metadata

---

## Quick Reference

### Most Used Folders

| Path | Purpose |
|------|---------|
| `Project/Core/System/` | Game instance, managers |
| `Project/Maps/KazanMain/` | Main game level |
| `Project/Resources/Objects/HumanMade/` | Most game items |
| `Project/UI/CustomUI/Widgets/` | UI components |
| `Project/Placeables/Interaction/` | Interactive objects |
| `InventorySystem/Items/` | Inventory item definitions |

### Common Tasks

**Adding a new interactable object:**
1. Create Blueprint in `Project/Placeables/Interaction/`
2. Add mesh to `Project/Resources/Objects/HumanMade/[Category]/`
3. Create materials in same folder
4. Add to appropriate data table

**Creating a new UI widget:**
1. Create widget in `Project/UI/CustomUI/Widgets/[Category]/`
2. Inherit from appropriate BaseClass
3. Add textures to `Project/UI/CustomUI/Textures/`
4. Add sounds to `Project/UI/CustomUI/Sounds/`

**Adding a new item:**
1. Define in `InventorySystem/Items/[Category]/`
2. Create mesh in `Project/Resources/Objects/HumanMade/VitalNeed/` or `WorkTool/`
3. Add icon to item category
4. Add to loot tables if needed

---

## Version History

- **v1.0** (2025-10-16) - Initial content structure documentation
