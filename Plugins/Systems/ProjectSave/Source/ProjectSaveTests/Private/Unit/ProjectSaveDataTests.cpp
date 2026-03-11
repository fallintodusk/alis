// Copyright ALIS. All Rights Reserved.

/**
 * Comprehensive Unit Tests for ProjectSave Data Structures
 *
 * Tests verify save data structures, player profiles, game settings,
 * and world progress tracking.
 */

#include "Misc/AutomationTest.h"
#include "Data/ProjectSaveData.h"
#include "Data/ProjectProgressSnapshot.h"
#include "ProjectSaveSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Player Profile Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_PlayerProfile_DefaultConstruction,
	"ProjectSave.PlayerProfile.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_PlayerProfile_DefaultConstruction::RunTest(const FString& Parameters)
{
	FProjectPlayerProfile Profile;

	TestTrue(TEXT("ProfileName starts empty"), Profile.ProfileName.IsEmpty());
	TestEqual(TEXT("TotalPlaytimeSeconds starts at 0"), Profile.TotalPlaytimeSeconds, 0.0f);
	TestEqual(TEXT("CurrentLevel starts at 1"), Profile.CurrentLevel, 1);
	TestEqual(TEXT("CompletionPercentage starts at 0"), Profile.CompletionPercentage, 0.0f);
	TestTrue(TEXT("LastCheckpointMap starts empty"), Profile.LastCheckpointMap.IsEmpty());
	TestEqual(TEXT("LastCheckpointLocation is zero"), Profile.LastCheckpointLocation, FVector::ZeroVector);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_PlayerProfile_Properties,
	"ProjectSave.PlayerProfile.Properties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_PlayerProfile_Properties::RunTest(const FString& Parameters)
{
	FProjectPlayerProfile Profile;

	// Set properties
	Profile.ProfileName = TEXT("TestPlayer");
	Profile.TotalPlaytimeSeconds = 3600.5f;
	Profile.CurrentLevel = 5;
	Profile.CompletionPercentage = 0.35f;
	Profile.LastCheckpointMap = TEXT("KazanMain");
	Profile.LastCheckpointLocation = FVector(100, 200, 300);

	// Verify properties
	TestEqual(TEXT("ProfileName set correctly"), Profile.ProfileName, TEXT("TestPlayer"));
	TestEqual(TEXT("TotalPlaytimeSeconds set correctly"), Profile.TotalPlaytimeSeconds, 3600.5f);
	TestEqual(TEXT("CurrentLevel set correctly"), Profile.CurrentLevel, 5);
	TestEqual(TEXT("CompletionPercentage set correctly"), Profile.CompletionPercentage, 0.35f);
	TestEqual(TEXT("LastCheckpointMap set correctly"), Profile.LastCheckpointMap, TEXT("KazanMain"));
	TestEqual(TEXT("LastCheckpointLocation set correctly"), Profile.LastCheckpointLocation, FVector(100, 200, 300));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_PlayerProfile_Timestamps,
	"ProjectSave.PlayerProfile.Timestamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_PlayerProfile_Timestamps::RunTest(const FString& Parameters)
{
	FProjectPlayerProfile Profile;

	// Set timestamps
	FDateTime CreationTime = FDateTime(2024, 10, 29, 12, 0, 0);
	FDateTime LastSaveTime = FDateTime(2024, 10, 29, 15, 30, 0);

	Profile.CreationTime = CreationTime;
	Profile.LastSaveTime = LastSaveTime;

	// Verify timestamps
	TestTrue(TEXT("CreationTime set correctly"), Profile.CreationTime == CreationTime);
	TestTrue(TEXT("LastSaveTime set correctly"), Profile.LastSaveTime == LastSaveTime);

	// Verify time difference
	FTimespan TimeDiff = Profile.LastSaveTime - Profile.CreationTime;
	TestTrue(TEXT("Time difference is positive"), TimeDiff.GetTotalSeconds() > 0);

	return true;
}

// ============================================================================
// Game Settings Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_GameSettings_DefaultConstruction,
	"ProjectSave.GameSettings.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_GameSettings_DefaultConstruction::RunTest(const FString& Parameters)
{
	FProjectGameSettings Settings;

	// Graphics defaults
	TestEqual(TEXT("GraphicsQuality defaults to 3"), Settings.GraphicsQuality, 3);
	TestTrue(TEXT("VSync defaults to true"), Settings.bVSync);
	TestEqual(TEXT("ResolutionWidth defaults to 1920"), Settings.ResolutionWidth, 1920);
	TestEqual(TEXT("ResolutionHeight defaults to 1080"), Settings.ResolutionHeight, 1080);
	TestTrue(TEXT("Fullscreen defaults to true"), Settings.bFullscreen);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_GameSettings_GraphicsProperties,
	"ProjectSave.GameSettings.GraphicsProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_GameSettings_GraphicsProperties::RunTest(const FString& Parameters)
{
	FProjectGameSettings Settings;

	// Modify graphics settings
	Settings.GraphicsQuality = 4; // Epic
	Settings.bVSync = false;
	Settings.ResolutionWidth = 2560;
	Settings.ResolutionHeight = 1440;
	Settings.bFullscreen = false;

	// Verify changes
	TestEqual(TEXT("GraphicsQuality updated"), Settings.GraphicsQuality, 4);
	TestFalse(TEXT("VSync updated"), Settings.bVSync);
	TestEqual(TEXT("ResolutionWidth updated"), Settings.ResolutionWidth, 2560);
	TestEqual(TEXT("ResolutionHeight updated"), Settings.ResolutionHeight, 1440);
	TestFalse(TEXT("Fullscreen updated"), Settings.bFullscreen);

	return true;
}

// ============================================================================
// Save Subsystem Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Subsystem_ClassDefinition,
	"ProjectSave.Subsystem.ClassDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Subsystem_ClassDefinition::RunTest(const FString& Parameters)
{
	UClass* SubsystemClass = UProjectSaveSubsystem::StaticClass();

	TestNotNull(TEXT("Subsystem class exists"), SubsystemClass);
	TestTrue(TEXT("Subsystem is GameInstanceSubsystem"),
		SubsystemClass->IsChildOf(UGameInstanceSubsystem::StaticClass()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Subsystem_SlotNames,
	"ProjectSave.Subsystem.SlotNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Subsystem_SlotNames::RunTest(const FString& Parameters)
{
	// Test static slot name functions
	FString DefaultSlot = UProjectSaveSubsystem::GetDefaultSlotName();
	FString AutoSaveSlot = UProjectSaveSubsystem::GetAutoSaveSlotName();

	TestFalse(TEXT("Default slot name not empty"), DefaultSlot.IsEmpty());
	TestFalse(TEXT("AutoSave slot name not empty"), AutoSaveSlot.IsEmpty());
	TestNotEqual(TEXT("Slot names are different"), DefaultSlot, AutoSaveSlot);
	TestTrue(TEXT("Default slot contains 'Default'"), DefaultSlot.Contains(TEXT("Default")));
	TestTrue(TEXT("AutoSave slot contains 'AutoSave'"), AutoSaveSlot.Contains(TEXT("AutoSave")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Subsystem_CreateNewSave,
	"ProjectSave.Subsystem.CreateNewSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Subsystem_CreateNewSave::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("TestProfile"));

	// Verify current save exists
	const UProjectSaveGame* CurrentSave = Subsystem->GetCurrentSave();
	TestNotNull(TEXT("Current save created"), CurrentSave);

	if (CurrentSave)
	{
		TestEqual(TEXT("Profile name set correctly"),
			CurrentSave->PlayerProfile.ProfileName, TEXT("TestProfile"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Subsystem_UpdatePlayerProfile,
	"ProjectSave.Subsystem.UpdatePlayerProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Subsystem_UpdatePlayerProfile::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("InitialProfile"));

	// Update profile
	FProjectPlayerProfile NewProfile;
	NewProfile.ProfileName = TEXT("UpdatedProfile");
	NewProfile.CurrentLevel = 10;
	NewProfile.TotalPlaytimeSeconds = 7200.0f;

	Subsystem->UpdatePlayerProfile(NewProfile);

	// Verify update
	const UProjectSaveGame* CurrentSave = Subsystem->GetCurrentSave();
	TestNotNull(TEXT("Current save exists"), CurrentSave);

	if (CurrentSave)
	{
		TestEqual(TEXT("Profile name updated"),
			CurrentSave->PlayerProfile.ProfileName, TEXT("UpdatedProfile"));
		TestEqual(TEXT("CurrentLevel updated"),
			CurrentSave->PlayerProfile.CurrentLevel, 10);
		TestEqual(TEXT("TotalPlaytimeSeconds updated"),
			CurrentSave->PlayerProfile.TotalPlaytimeSeconds, 7200.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Subsystem_UpdateGameSettings,
	"ProjectSave.Subsystem.UpdateGameSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Subsystem_UpdateGameSettings::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("TestProfile"));

	// Update settings
	FProjectGameSettings NewSettings;
	NewSettings.GraphicsQuality = 4;
	NewSettings.bVSync = false;
	NewSettings.ResolutionWidth = 3840;

	Subsystem->UpdateGameSettings(NewSettings);

	// Verify update
	const UProjectSaveGame* CurrentSave = Subsystem->GetCurrentSave();
	TestNotNull(TEXT("Current save exists"), CurrentSave);

	if (CurrentSave)
	{
		TestEqual(TEXT("GraphicsQuality updated"),
			CurrentSave->GameSettings.GraphicsQuality, 4);
		TestFalse(TEXT("VSync updated"),
			CurrentSave->GameSettings.bVSync);
		TestEqual(TEXT("ResolutionWidth updated"),
			CurrentSave->GameSettings.ResolutionWidth, 3840);
	}

	return true;
}

// ============================================================================
// Milestone Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Milestones_AddCompleted,
	"ProjectSave.Milestones.AddCompleted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Milestones_AddCompleted::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("TestProfile"));

	// Add milestone
	FName WorldId = FName(TEXT("KazanMain"));
	FName MilestoneId = FName(TEXT("TutorialComplete"));

	Subsystem->AddCompletedMilestone(WorldId, MilestoneId);

	// Verify milestone
	bool bCompleted = Subsystem->IsMilestoneCompleted(WorldId, MilestoneId);
	TestTrue(TEXT("Milestone marked as completed"), bCompleted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Milestones_NotCompleted,
	"ProjectSave.Milestones.NotCompleted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Milestones_NotCompleted::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("TestProfile"));

	// Check non-existent milestone
	FName WorldId = FName(TEXT("KazanMain"));
	FName MilestoneId = FName(TEXT("NonExistentMilestone"));

	bool bCompleted = Subsystem->IsMilestoneCompleted(WorldId, MilestoneId);
	TestFalse(TEXT("Non-existent milestone not completed"), bCompleted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Milestones_MultipleMilestones,
	"ProjectSave.Milestones.MultipleMilestones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Milestones_MultipleMilestones::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("TestProfile"));

	FName WorldId = FName(TEXT("KazanMain"));

	// Add multiple milestones
	Subsystem->AddCompletedMilestone(WorldId, FName(TEXT("Milestone1")));
	Subsystem->AddCompletedMilestone(WorldId, FName(TEXT("Milestone2")));
	Subsystem->AddCompletedMilestone(WorldId, FName(TEXT("Milestone3")));

	// Verify all milestones
	TestTrue(TEXT("Milestone1 completed"),
		Subsystem->IsMilestoneCompleted(WorldId, FName(TEXT("Milestone1"))));
	TestTrue(TEXT("Milestone2 completed"),
		Subsystem->IsMilestoneCompleted(WorldId, FName(TEXT("Milestone2"))));
	TestTrue(TEXT("Milestone3 completed"),
		Subsystem->IsMilestoneCompleted(WorldId, FName(TEXT("Milestone3"))));

	// Verify non-added milestone still false
	TestFalse(TEXT("Milestone4 not completed"),
		Subsystem->IsMilestoneCompleted(WorldId, FName(TEXT("Milestone4"))));

	return true;
}

// ============================================================================
// Location Discovery Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Locations_AddDiscovered,
	"ProjectSave.Locations.AddDiscovered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Locations_AddDiscovered::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("TestProfile"));

	// Add discovered location
	FName WorldId = FName(TEXT("KazanMain"));
	FName LocationId = FName(TEXT("KremlinTower"));

	Subsystem->AddDiscoveredLocation(WorldId, LocationId);

	// Get world progress and verify
	FProjectWorldProgress Progress = Subsystem->GetWorldProgress(WorldId);
	TestTrue(TEXT("World progress retrieved"),
		Progress.DiscoveredLocations.Contains(LocationId));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSave_Locations_MultipleLocations,
	"ProjectSave.Locations.MultipleLocations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectSave_Locations_MultipleLocations::RunTest(const FString& Parameters)
{
	UProjectSaveSubsystem* Subsystem = NewObject<UProjectSaveSubsystem>();

	// Create new save
	Subsystem->CreateNewSave(TEXT("TestProfile"));

	FName WorldId = FName(TEXT("KazanMain"));

	// Add multiple locations
	Subsystem->AddDiscoveredLocation(WorldId, FName(TEXT("Location1")));
	Subsystem->AddDiscoveredLocation(WorldId, FName(TEXT("Location2")));
	Subsystem->AddDiscoveredLocation(WorldId, FName(TEXT("Location3")));

	// Get world progress
	FProjectWorldProgress Progress = Subsystem->GetWorldProgress(WorldId);

	// Verify all locations
	TestEqual(TEXT("3 locations discovered"),
		Progress.DiscoveredLocations.Num(), 3);
	TestTrue(TEXT("Location1 discovered"),
		Progress.DiscoveredLocations.Contains(FName(TEXT("Location1"))));
	TestTrue(TEXT("Location2 discovered"),
		Progress.DiscoveredLocations.Contains(FName(TEXT("Location2"))));
	TestTrue(TEXT("Location3 discovered"),
		Progress.DiscoveredLocations.Contains(FName(TEXT("Location3"))));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
