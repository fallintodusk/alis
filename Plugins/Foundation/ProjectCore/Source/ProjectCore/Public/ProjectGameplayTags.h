#pragma once

#include "NativeGameplayTags.h"

/**
 * Centralized gameplay tag declarations for the project.
 *
 * All plugins depend on ProjectCore, so tags defined here are accessible everywhere.
 * Uses hierarchical naming: Category.Subcategory.Specific
 *
 * Categories:
 * - Ability.*      - GAS ability states and types
 * - Status.*       - Character status effects
 * - GameplayCue.*  - Visual/audio feedback
 * - SetByCaller.*  - Dynamic GAS values
 * - Damage.*       - Damage types
 * - Item.*         - Items and inventory
 * - World.*        - World objects
 * - Input.*        - Enhanced input
 * - Event.*        - Game events
 */
namespace ProjectTags
{
	// -------------------------------------------------------------------------
	// GAS - Abilities
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_IsDead);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cooldown);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cost);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Blocked);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action_Melee);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action_Ranged);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Passive);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Behavior_SurvivesDeath);

	// -------------------------------------------------------------------------
	// GAS - Status Effects
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dying);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dead);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Debuff_Stunned);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Debuff_Rooted);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Debuff_Silenced);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Buff_Haste);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Buff_Shield);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Immunity_Damage);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Immunity_CC);

	// -------------------------------------------------------------------------
	// GAS - Vital State (Threshold-Based)
	// Written by ProjectVitals (server) based on attribute %. UI only reads.
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State);

	// Condition/Stamina/Calories/Hydration thresholds:
	// OK>70%, Low 40-70%, Critical 20-40%, Empty<=20%

	// Condition states (health/life bar)
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Condition);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Condition_OK);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Condition_Low);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Condition_Critical);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Condition_Empty);

	// Stamina states (short-term resource)
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stamina);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stamina_OK);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stamina_Low);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stamina_Critical);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stamina_Empty);

	// Calories states (food energy, 0-2500 kcal)
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Calories);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Calories_OK);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Calories_Low);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Calories_Critical);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Calories_Empty);

	// Hydration states (water store, 0-3.0L)
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hydration);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hydration_OK);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hydration_Low);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hydration_Critical);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hydration_Empty);

	// Fatigue thresholds (INVERTED: 0=good, 100=bad):
	// Rested<30%, Tired 30-60%, Exhausted 60-85%, Critical>=85%
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Fatigue);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Fatigue_Rested);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Fatigue_Tired);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Fatigue_Exhausted);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Fatigue_Critical);

	// Weight/encumbrance thresholds (0-100% of max carry):
	// Light<60%, Medium 60-80%, Heavy 80-100%, Overweight>100%
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weight);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weight_Light);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weight_Medium);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weight_Heavy);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weight_Overweight);

	// -------------------------------------------------------------------------
	// GAS - Gameplay Cues
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Character_Hit);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Character_Death);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Activate);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);

	// -------------------------------------------------------------------------
	// GAS - SetByCaller
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stacks);

	// Attribute magnitudes (for items, environment, vitals tick)
	// Naming matches AttributeSets: Condition, Stamina, Hydration, Calories, Fatigue
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Condition);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stamina);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Hydration);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Calories);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Fatigue);

	// Status effect magnitudes
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Bleeding);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Poisoned);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Radiation);

	// -------------------------------------------------------------------------
	// Damage Types
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Physical);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Physical_Blunt);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Physical_Pierce);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Physical_Slash);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Fire);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Ice);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Electric);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Poison);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Radiation);

	// -------------------------------------------------------------------------
	// Items & Inventory
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Pickup);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_Hands);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_LeftHand);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_RightHand);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_Pockets);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_Pockets1);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_Pockets2);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_Pockets3);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_Pockets4);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_Backpack);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Container_WorldStorage);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Weapon);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Weapon_Melee);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Weapon_Ranged);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Weapon_Thrown);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Armor);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Armor_Head);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Armor_Chest);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Armor_Legs);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Consumable);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Consumable_Health);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Consumable_Ammo);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Resource);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Quest);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Equipment);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Equipment_Crowbar);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Survival);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Survival_Hydration);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Survival_Nutrition);

	// Keys (under Item.Type for UI filtering, full path for lock matching)
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Key);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Key_Apartment);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Key_Apartment_Luxury);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Key_Apartment_Basic);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Key_Apartment_Family);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Key_Vehicle);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Key_Safe);

	// Equipment Slots
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_MainHand);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_OffHand);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_Head);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_Chest);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_Back);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_Legs);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_Feet);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_EquipmentSlot_Accessory);

	// -------------------------------------------------------------------------
	// World Objects
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_Actor);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_POI);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_POI_Landmark);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_POI_SafeZone);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_Interactable);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_Interactable_Door);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_Interactable_Terminal);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_Interactable_Switch);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_Trigger);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_NPC);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_NPC_Friendly);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_NPC_Hostile);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(World_NPC_Neutral);

	// -------------------------------------------------------------------------
	// Input (Enhanced Input)
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Move);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Look);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Jump);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Crouch);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Sprint);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Interact);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Attack_Primary);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Attack_Secondary);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Ability_Slot1);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Ability_Slot2);

	// -------------------------------------------------------------------------
	// Game Events
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Hit);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Kill);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Death);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Game_LevelUp);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Game_QuestComplete);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_World_ZoneEnter);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_World_ZoneExit);

	// -------------------------------------------------------------------------
	// UI Layers (Phase 10.5 - input routing)
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Game);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_GameMenu);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Modal);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Notification);

	// -------------------------------------------------------------------------
	// HUD Slots (Phase 10 - slot-based widget composition)
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_VitalsMini);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_StatusIcons);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_MindThought);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_Compass);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_Minimap);

	// -------------------------------------------------------------------------
	// Scenario (scripted events, unique locks, quest-specific)
	// -------------------------------------------------------------------------

	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Scenario);
	PROJECTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Scenario_GrandpaDoor);
}
