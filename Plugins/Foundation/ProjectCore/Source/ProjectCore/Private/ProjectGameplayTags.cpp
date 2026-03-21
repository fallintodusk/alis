#include "ProjectGameplayTags.h"

namespace ProjectTags
{
	// -------------------------------------------------------------------------
	// GAS - Abilities
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(Ability, "Ability");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ActivateFail_IsDead, "Ability.ActivateFail.IsDead");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ActivateFail_Cost, "Ability.ActivateFail.Cost");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ActivateFail_Blocked, "Ability.ActivateFail.Blocked");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action, "Ability.Type.Action");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Melee, "Ability.Type.Action.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Ranged, "Ability.Type.Action.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Passive, "Ability.Type.Passive");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Behavior_SurvivesDeath, "Ability.Behavior.SurvivesDeath");

	// -------------------------------------------------------------------------
	// GAS - Status Effects
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(Status, "Status");
	UE_DEFINE_GAMEPLAY_TAG(Status_Death_Dying, "Status.Death.Dying");
	UE_DEFINE_GAMEPLAY_TAG(Status_Death_Dead, "Status.Death.Dead");
	UE_DEFINE_GAMEPLAY_TAG(Status_Debuff_Stunned, "Status.Debuff.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(Status_Debuff_Rooted, "Status.Debuff.Rooted");
	UE_DEFINE_GAMEPLAY_TAG(Status_Debuff_Silenced, "Status.Debuff.Silenced");
	UE_DEFINE_GAMEPLAY_TAG(Status_Buff_Haste, "Status.Buff.Haste");
	UE_DEFINE_GAMEPLAY_TAG(Status_Buff_Shield, "Status.Buff.Shield");
	UE_DEFINE_GAMEPLAY_TAG(Status_Immunity_Damage, "Status.Immunity.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Status_Immunity_CC, "Status.Immunity.CC");

	// -------------------------------------------------------------------------
	// GAS - Vital State (Threshold-Based)
	// Written by ProjectVitals (server) based on attribute %. UI only reads.
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(State, "State");

	// Condition/Stamina/Calories/Hydration thresholds:
	// OK>70%, Low 40-70%, Critical 20-40%, Empty<=20%

	// Condition states (health/life bar)
	UE_DEFINE_GAMEPLAY_TAG(State_Condition, "State.Condition");
	UE_DEFINE_GAMEPLAY_TAG(State_Condition_OK, "State.Condition.OK");
	UE_DEFINE_GAMEPLAY_TAG(State_Condition_Low, "State.Condition.Low");
	UE_DEFINE_GAMEPLAY_TAG(State_Condition_Critical, "State.Condition.Critical");
	UE_DEFINE_GAMEPLAY_TAG(State_Condition_Empty, "State.Condition.Empty");

	// Stamina states (short-term resource)
	UE_DEFINE_GAMEPLAY_TAG(State_Stamina, "State.Stamina");
	UE_DEFINE_GAMEPLAY_TAG(State_Stamina_OK, "State.Stamina.OK");
	UE_DEFINE_GAMEPLAY_TAG(State_Stamina_Low, "State.Stamina.Low");
	UE_DEFINE_GAMEPLAY_TAG(State_Stamina_Critical, "State.Stamina.Critical");
	UE_DEFINE_GAMEPLAY_TAG(State_Stamina_Empty, "State.Stamina.Empty");

	// Calories states (food energy, 0-2500 kcal)
	UE_DEFINE_GAMEPLAY_TAG(State_Calories, "State.Calories");
	UE_DEFINE_GAMEPLAY_TAG(State_Calories_OK, "State.Calories.OK");
	UE_DEFINE_GAMEPLAY_TAG(State_Calories_Low, "State.Calories.Low");
	UE_DEFINE_GAMEPLAY_TAG(State_Calories_Critical, "State.Calories.Critical");
	UE_DEFINE_GAMEPLAY_TAG(State_Calories_Empty, "State.Calories.Empty");

	// Hydration states (water store, 0-3.0L)
	UE_DEFINE_GAMEPLAY_TAG(State_Hydration, "State.Hydration");
	UE_DEFINE_GAMEPLAY_TAG(State_Hydration_OK, "State.Hydration.OK");
	UE_DEFINE_GAMEPLAY_TAG(State_Hydration_Low, "State.Hydration.Low");
	UE_DEFINE_GAMEPLAY_TAG(State_Hydration_Critical, "State.Hydration.Critical");
	UE_DEFINE_GAMEPLAY_TAG(State_Hydration_Empty, "State.Hydration.Empty");

	// Fatigue thresholds (INVERTED: 0=good, 100=bad):
	// Rested<30%, Tired 30-60%, Exhausted 60-85%, Critical>=85%
	UE_DEFINE_GAMEPLAY_TAG(State_Fatigue, "State.Fatigue");
	UE_DEFINE_GAMEPLAY_TAG(State_Fatigue_Rested, "State.Fatigue.Rested");
	UE_DEFINE_GAMEPLAY_TAG(State_Fatigue_Tired, "State.Fatigue.Tired");
	UE_DEFINE_GAMEPLAY_TAG(State_Fatigue_Exhausted, "State.Fatigue.Exhausted");
	UE_DEFINE_GAMEPLAY_TAG(State_Fatigue_Critical, "State.Fatigue.Critical");

	// Light<60%, Medium 60-80%, Heavy 80-100%, Overweight>100%
	UE_DEFINE_GAMEPLAY_TAG(State_Weight, "State.Weight");
	UE_DEFINE_GAMEPLAY_TAG(State_Weight_Light, "State.Weight.Light");
	UE_DEFINE_GAMEPLAY_TAG(State_Weight_Medium, "State.Weight.Medium");
	UE_DEFINE_GAMEPLAY_TAG(State_Weight_Heavy, "State.Weight.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(State_Weight_Overweight, "State.Weight.Overweight");

	// -------------------------------------------------------------------------
	// GAS - Gameplay Cues
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue, "GameplayCue");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Character_Hit, "GameplayCue.Character.Hit");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Character_Death, "GameplayCue.Character.Death");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Activate, "GameplayCue.Ability.Activate");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Fire, "GameplayCue.Weapon.Fire");

	// -------------------------------------------------------------------------
	// GAS - SetByCaller
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller, "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage, "SetByCaller.Damage");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Duration, "SetByCaller.Duration");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Stacks, "SetByCaller.Stacks");

	// Attribute magnitudes (for items, environment, vitals tick)
	// Naming matches AttributeSets: Condition, Stamina, Hydration, Calories, Fatigue
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Condition, "SetByCaller.Condition");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Stamina, "SetByCaller.Stamina");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Hydration, "SetByCaller.Hydration");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Calories, "SetByCaller.Calories");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Fatigue, "SetByCaller.Fatigue");

	// Status effect magnitudes
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Bleeding, "SetByCaller.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Poisoned, "SetByCaller.Poisoned");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Radiation, "SetByCaller.Radiation");

	// -------------------------------------------------------------------------
	// Damage Types
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(Damage, "Damage");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Physical, "Damage.Type.Physical");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Physical_Blunt, "Damage.Type.Physical.Blunt");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Physical_Pierce, "Damage.Type.Physical.Pierce");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Physical_Slash, "Damage.Type.Physical.Slash");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Fire, "Damage.Type.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Ice, "Damage.Type.Ice");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Electric, "Damage.Type.Electric");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Poison, "Damage.Type.Poison");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Radiation, "Damage.Type.Radiation");

	// -------------------------------------------------------------------------
	// Items & Inventory
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(Item, "Item");
	UE_DEFINE_GAMEPLAY_TAG(Item_Pickup, "Item.Pickup");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container, "Item.Container");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_Hands, "Item.Container.Hands");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_LeftHand, "Item.Container.LeftHand");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_RightHand, "Item.Container.RightHand");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_Pockets, "Item.Container.Pockets");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_Pockets1, "Item.Container.Pockets1");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_Pockets2, "Item.Container.Pockets2");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_Pockets3, "Item.Container.Pockets3");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_Pockets4, "Item.Container.Pockets4");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_Backpack, "Item.Container.Backpack");
	UE_DEFINE_GAMEPLAY_TAG(Item_Container_WorldStorage, "Item.Container.WorldStorage");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Weapon, "Item.Type.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Weapon_Melee, "Item.Type.Weapon.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Weapon_Ranged, "Item.Type.Weapon.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Weapon_Thrown, "Item.Type.Weapon.Thrown");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Armor, "Item.Type.Armor");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Armor_Head, "Item.Type.Armor.Head");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Armor_Chest, "Item.Type.Armor.Chest");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Armor_Legs, "Item.Type.Armor.Legs");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Consumable, "Item.Type.Consumable");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Consumable_Health, "Item.Type.Consumable.Health");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Consumable_Ammo, "Item.Type.Consumable.Ammo");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Resource, "Item.Type.Resource");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Currency, "Item.Type.Currency");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Quest, "Item.Type.Quest");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Equipment, "Item.Type.Equipment");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Equipment_Crowbar, "Item.Type.Equipment.Crowbar");
	UE_DEFINE_GAMEPLAY_TAG(Item_Survival, "Item.Survival");
	UE_DEFINE_GAMEPLAY_TAG(Item_Survival_Hydration, "Item.Survival.Hydration");
	UE_DEFINE_GAMEPLAY_TAG(Item_Survival_Nutrition, "Item.Survival.Nutrition");

	// Keys (under Item.Type for UI filtering, full path for lock matching)
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Key, "Item.Type.Key");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Key_Apartment, "Item.Type.Key.Apartment");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Key_Apartment_Luxury, "Item.Type.Key.Apartment.Luxury");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Key_Apartment_Basic, "Item.Type.Key.Apartment.Basic");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Key_Apartment_Family, "Item.Type.Key.Apartment.Family");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Key_Vehicle, "Item.Type.Key.Vehicle");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Key_Safe, "Item.Type.Key.Safe");

	// Equipment Slots
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot, "Item.EquipmentSlot");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_MainHand, "Item.EquipmentSlot.MainHand");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_OffHand, "Item.EquipmentSlot.OffHand");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_Head, "Item.EquipmentSlot.Head");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_Chest, "Item.EquipmentSlot.Chest");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_Legs, "Item.EquipmentSlot.Legs");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_Feet, "Item.EquipmentSlot.Feet");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_Accessory, "Item.EquipmentSlot.Accessory");
	UE_DEFINE_GAMEPLAY_TAG(Item_EquipmentSlot_Back, "Item.EquipmentSlot.Back");

	// -------------------------------------------------------------------------
	// World Objects
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(World, "World");
	UE_DEFINE_GAMEPLAY_TAG(World_Actor, "World.Actor");
	UE_DEFINE_GAMEPLAY_TAG(World_POI, "World.POI");
	UE_DEFINE_GAMEPLAY_TAG(World_POI_Landmark, "World.POI.Landmark");
	UE_DEFINE_GAMEPLAY_TAG(World_POI_SafeZone, "World.POI.SafeZone");
	UE_DEFINE_GAMEPLAY_TAG(World_Interactable, "World.Interactable");
	UE_DEFINE_GAMEPLAY_TAG(World_Interactable_Door, "World.Interactable.Door");
	UE_DEFINE_GAMEPLAY_TAG(World_Interactable_Terminal, "World.Interactable.Terminal");
	UE_DEFINE_GAMEPLAY_TAG(World_Interactable_Switch, "World.Interactable.Switch");
	UE_DEFINE_GAMEPLAY_TAG(World_Trigger, "World.Trigger");
	UE_DEFINE_GAMEPLAY_TAG(World_NPC, "World.NPC");
	UE_DEFINE_GAMEPLAY_TAG(World_NPC_Friendly, "World.NPC.Friendly");
	UE_DEFINE_GAMEPLAY_TAG(World_NPC_Hostile, "World.NPC.Hostile");
	UE_DEFINE_GAMEPLAY_TAG(World_NPC_Neutral, "World.NPC.Neutral");

	// -------------------------------------------------------------------------
	// Input (Enhanced Input)
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(Input, "Input");
	UE_DEFINE_GAMEPLAY_TAG(Input_Move, "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look, "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump, "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Sprint, "Input.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Input_Interact, "Input.Interact");
	UE_DEFINE_GAMEPLAY_TAG(Input_Attack_Primary, "Input.Attack.Primary");
	UE_DEFINE_GAMEPLAY_TAG(Input_Attack_Secondary, "Input.Attack.Secondary");
	UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Slot1, "Input.Ability.Slot1");
	UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Slot2, "Input.Ability.Slot2");

	// -------------------------------------------------------------------------
	// Game Events
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(Event, "Event");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Hit, "Event.Combat.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Kill, "Event.Combat.Kill");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Death, "Event.Combat.Death");
	UE_DEFINE_GAMEPLAY_TAG(Event_Game_LevelUp, "Event.Game.LevelUp");
	UE_DEFINE_GAMEPLAY_TAG(Event_Game_QuestComplete, "Event.Game.QuestComplete");
	UE_DEFINE_GAMEPLAY_TAG(Event_World_ZoneEnter, "Event.World.ZoneEnter");
	UE_DEFINE_GAMEPLAY_TAG(Event_World_ZoneExit, "Event.World.ZoneExit");

	// -------------------------------------------------------------------------
	// UI Layers (Phase 10.5 - input routing)
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(UI_Layer, "UI.Layer");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Game, "UI.Layer.Game");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_GameMenu, "UI.Layer.GameMenu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Modal, "UI.Layer.Modal");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Notification, "UI.Layer.Notification");

	// -------------------------------------------------------------------------
	// HUD Slots (Phase 10 - slot-based widget composition)
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(HUD_Slot, "HUD.Slot");
	UE_DEFINE_GAMEPLAY_TAG(HUD_Slot_VitalsMini, "HUD.Slot.VitalsMini");
	UE_DEFINE_GAMEPLAY_TAG(HUD_Slot_StatusIcons, "HUD.Slot.StatusIcons");
	UE_DEFINE_GAMEPLAY_TAG(HUD_Slot_MindThought, "HUD.Slot.MindThought");
	UE_DEFINE_GAMEPLAY_TAG(HUD_Slot_Compass, "HUD.Slot.Compass");
	UE_DEFINE_GAMEPLAY_TAG(HUD_Slot_Minimap, "HUD.Slot.Minimap");

	// -------------------------------------------------------------------------
	// Scenario (scripted events, unique locks, quest-specific)
	// -------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG(Scenario, "Scenario");
	UE_DEFINE_GAMEPLAY_TAG(Scenario_GrandpaDoor, "Scenario.GrandpaDoor");
}
