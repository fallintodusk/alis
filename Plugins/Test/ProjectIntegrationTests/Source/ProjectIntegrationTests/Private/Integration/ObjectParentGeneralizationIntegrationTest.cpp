// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Data/ObjectDefinition.h"
#include "DefinitionJsonParser.h"
#include "DefinitionTypeInfo.h"
#include "DefinitionActorSyncSubsystem.h"
#include "ObjectDefinitionHostHelpers.h"
#include "ProjectObjectActorFactory.h"
#include "ProjectServiceLocator.h"
#include "Services/IObjectSpawnService.h"
#include "CapabilityRegistry.h"
#include "Spawning/ObjectSpawnUtility.h"
#include "Template/Interactable/InteractableActor.h"

#include "InteractionComponent.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/IDialogueService.h"
#include "Components/ProjectDialogueComponent.h"
#include "Access/ActorWatcherComponent.h"
#include "Access/LockableComponent.h"
#include "Components/SpringRotatorComponent.h"
#include "Support/ObjectParentGeneralizationTestDoubles.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
UWorld* OPG_ResolveAutomationTestWorld()
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (World)
	{
		return World;
	}

	if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
	{
		return nullptr;
	}

	return AutomationCommon::GetAnyGameWorld();
}

TSharedPtr<FJsonObject> OPG_MakeMinimalObjectJson(
	const FString& SpawnClassPath,
	const FString& AttachToComponentTag = FString(),
	const TArray<FString>& ActorTags = TArray<FString>())
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("id"), TEXT("TmpSpawnClassTest"));
	if (!SpawnClassPath.IsEmpty())
	{
		Root->SetStringField(TEXT("spawnClass"), SpawnClassPath);
	}
	if (!AttachToComponentTag.IsEmpty())
	{
		Root->SetStringField(TEXT("attachToComponentTag"), AttachToComponentTag);
	}
	if (ActorTags.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> JsonActorTags;
		for (const FString& Tag : ActorTags)
		{
			JsonActorTags.Add(MakeShared<FJsonValueString>(Tag));
		}
		Root->SetArrayField(TEXT("actorTags"), JsonActorTags);
	}

	TSharedPtr<FJsonObject> Mesh = MakeShared<FJsonObject>();
	Mesh->SetStringField(TEXT("id"), TEXT("body"));
	Mesh->SetStringField(TEXT("asset"), TEXT("/Engine/BasicShapes/Cube"));

	TArray<TSharedPtr<FJsonValue>> Meshes;
	Meshes.Add(MakeShared<FJsonValueObject>(Mesh));
	Root->SetArrayField(TEXT("meshes"), Meshes);

	return Root;
}

UDefinitionActorSyncSubsystem* OPG_ResolveDefinitionActorSyncSubsystem()
{
	if (!GEditor)
	{
		return nullptr;
	}

	return GEditor->GetEditorSubsystem<UDefinitionActorSyncSubsystem>();
}

UObjectDefinition* OPG_MakeTransientObjectDefinition(
	const FName ObjectId,
	const FString& StructureHash,
	const FString& ContentHash)
{
	UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Def->ObjectId = ObjectId;
	Def->DefinitionStructureHash = StructureHash;
	Def->DefinitionContentHash = ContentHash;
	return Def;
}

AActor* OPG_FindActorByLabel(UWorld* World, const FString& Label, const AActor* Exclude)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == Exclude)
		{
			continue;
		}

		if (Candidate->GetActorLabel() == Label)
		{
			return Candidate;
		}
	}

	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SpawnClassParserNormalizationTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.SpawnClass.ParserNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SpawnClassParserNormalizationTest::RunTest(const FString& Parameters)
{
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	{
		UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		FString Error;
		const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
			TypeInfo,
			OPG_MakeMinimalObjectJson(TEXT("/Game/Project/Resources/Characters/GrandPa/GrandPa_BP")),
			Def,
			Error);
		TestTrue(TEXT("Parser should accept short /Game spawnClass"), bParsed);
		TestTrue(TEXT("SpawnClass should be set for valid /Game path"), !Def->SpawnClass.IsNull());
		if (!Def->SpawnClass.IsNull())
		{
			TestEqual(
				TEXT("SpawnClass should normalize to _C class object path"),
				Def->SpawnClass.ToSoftObjectPath().ToString(),
				FString(TEXT("/Game/Project/Resources/Characters/GrandPa/GrandPa_BP.GrandPa_BP_C")));
		}
	}

	{
		UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		AddExpectedError(TEXT("Invalid spawnClass"), EAutomationExpectedErrorFlags::Contains, 1);
		FString Error;
		const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
			TypeInfo,
			OPG_MakeMinimalObjectJson(TEXT("/Game/Project/Resources/Characters/GrandPa/GrandPa_BP.GrandPa_BP")),
			Def,
			Error);
		TestTrue(TEXT("Parser should continue on invalid explicit object path"), bParsed);
		TestTrue(TEXT("Invalid spawnClass object path should be ignored"), Def->SpawnClass.IsNull());
	}

	{
		UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		FString Error;
		const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
			TypeInfo,
			OPG_MakeMinimalObjectJson(TEXT("/Script/Engine.Character")),
			Def,
			Error);
		TestTrue(TEXT("Parser should accept native /Script class path"), bParsed);
		TestTrue(TEXT("Native spawnClass should be set"), !Def->SpawnClass.IsNull());
		if (!Def->SpawnClass.IsNull())
		{
			TestEqual(
				TEXT("Native class path should remain unchanged"),
				Def->SpawnClass.ToSoftObjectPath().ToString(),
				FString(TEXT("/Script/Engine.Character")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_AttachToComponentTagParserTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Parser.AttachToComponentTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_AttachToComponentTagParserTest::RunTest(const FString& Parameters)
{
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	FString Error;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT(""), TEXT("ObjectAttachRoot")),
		Def,
		Error);
	TestTrue(TEXT("Parser should accept attachToComponentTag"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *Error));
		return false;
	}

	TestEqual(
		TEXT("attachToComponentTag should parse to AttachToComponentTag"),
		Def->AttachToComponentTag,
		FName(TEXT("ObjectAttachRoot")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_ActorTagsParserTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Parser.ActorTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_ActorTagsParserTest::RunTest(const FString& Parameters)
{
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	const TArray<FString> ActorTags = {
		TEXT("Scenario.GrandpaDoor"),
		TEXT("Scenario.Test")
	};

	UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	FString Error;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT(""), TEXT(""), ActorTags),
		Def,
		Error);
	TestTrue(TEXT("Parser should accept actorTags"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *Error));
		return false;
	}

	TestEqual(TEXT("actorTags should parse count"), Def->ActorTags.Num(), ActorTags.Num());
	if (Def->ActorTags.Num() == ActorTags.Num())
	{
		TestEqual(TEXT("First actor tag should match"), Def->ActorTags[0], FName(TEXT("Scenario.GrandpaDoor")));
		TestEqual(TEXT("Second actor tag should match"), Def->ActorTags[1], FName(TEXT("Scenario.Test")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_ActorTagsSpawnApplicationTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Spawn.ActorTagsApplied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_ActorTagsSpawnApplicationTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	const TArray<FString> SpawnActorTags = { TEXT("Scenario.GrandpaDoor") };
	FString Error;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT(""), TEXT(""), SpawnActorTags),
		Def,
		Error);
	TestTrue(TEXT("Definition parse should succeed"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *Error));
		return false;
	}

	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def,
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Definition spawn should succeed"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	TestTrue(
		TEXT("Spawned actor should include actor tag from definition"),
		Spawned->Tags.Contains(FName(TEXT("Scenario.GrandpaDoor"))));

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SpawnClassResolutionOrderTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.SpawnClass.ResolutionOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SpawnClassResolutionOrderTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT("/Script/Engine.Character")),
		Def.Get(),
		ParseError);
	TestTrue(TEXT("Definition parse should succeed"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	AActor* OverrideSpawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError,
		AActor::StaticClass());
	TestNotNull(TEXT("Override class spawn should succeed"), OverrideSpawned);
	if (OverrideSpawned)
	{
		TestEqual(
			TEXT("Runtime override class should win over definition spawnClass"),
			OverrideSpawned->GetClass(),
			AActor::StaticClass());
		OverrideSpawned->Destroy();
	}

	AActor* DefinitionSpawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Definition spawnClass spawn should succeed"), DefinitionSpawned);
	if (DefinitionSpawned)
	{
		TestTrue(
			TEXT("Definition spawnClass should be used when override is absent"),
			DefinitionSpawned->IsA(ACharacter::StaticClass()));
		DefinitionSpawned->Destroy();
	}

	Def->SpawnClass.Reset();
	AActor* FallbackSpawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Fallback spawn should succeed"), FallbackSpawned);
	if (FallbackSpawned)
	{
		TestTrue(
			TEXT("Fallback should use AInteractableActor when spawnClass is unset"),
			FallbackSpawned->IsA(AInteractableActor::StaticClass()));
		FallbackSpawned->Destroy();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SpawnClassSecurityFallbackTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.SpawnClass.SecurityFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SpawnClassSecurityFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT("/Script/Engine.Actor")),
		Def.Get(),
		ParseError);
	TestTrue(TEXT("Definition parse should succeed"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	AddExpectedError(TEXT("Rejected spawn class"), EAutomationExpectedErrorFlags::Contains, 2);

	FText SpawnError;
	AActor* ForbiddenSpawn = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError,
		AGameModeBase::StaticClass());
	TestNotNull(TEXT("Forbidden spawnClass should fallback to spawnable class"), ForbiddenSpawn);
	if (ForbiddenSpawn)
	{
		TestTrue(
			TEXT("Forbidden class should fallback to AInteractableActor"),
			ForbiddenSpawn->IsA(AInteractableActor::StaticClass()));
		ForbiddenSpawn->Destroy();
	}

	AActor* AbstractSpawn = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError,
		AController::StaticClass());
	TestNotNull(TEXT("Abstract/forbidden class should fallback to spawnable class"), AbstractSpawn);
	if (AbstractSpawn)
	{
		TestTrue(
			TEXT("Abstract or forbidden class should fallback to AInteractableActor"),
			AbstractSpawn->IsA(AInteractableActor::StaticClass()));
		AbstractSpawn->Destroy();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SpawnRejectsInvalidDefinitionIdTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Spawn.InvalidDefinitionId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SpawnRejectsInvalidDefinitionIdTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	UObjectDefinition* Def = OPG_MakeTransientObjectDefinition(NAME_None, TEXT("S_InvalidId"), TEXT("C_InvalidId"));
	TestNotNull(TEXT("Transient definition should be created"), Def);
	if (!Def)
	{
		return false;
	}

	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def,
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);

	TestNull(TEXT("Spawn should fail when definition id is empty"), Spawned);
	TestTrue(
		TEXT("Spawn failure should provide clear invalid-id error"),
		SpawnError.ToString().Contains(TEXT("empty id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_PlacementFactorySpawnTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Placement.EditorFactorySpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_PlacementFactorySpawnTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	ULevel* Level = World->GetCurrentLevel();
	TestNotNull(TEXT("Current level should be available"), Level);
	if (!Level)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();
	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT("/Script/Engine.Actor")),
		Def.Get(),
		ParseError);
	TestTrue(TEXT("Definition parse should succeed"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	UProjectObjectActorFactory* Factory = NewObject<UProjectObjectActorFactory>(GetTransientPackage());
	TestNotNull(TEXT("ProjectObjectActorFactory should be instantiable"), Factory);
	if (!Factory)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transactional;
	AActor* Spawned = Factory->SpawnActor(Def.Get(), Level, FTransform::Identity, Params);
	TestNotNull(TEXT("Factory spawn should succeed"), Spawned);
	if (!Spawned)
	{
		return false;
	}

	FPrimaryAssetId HostedDefinitionId;
	FString HostedStructureHash;
	FString HostedContentHash;
	const bool bHasHostState = ProjectWorldDefinitionHost::ReadHostState(
		Spawned, HostedDefinitionId, HostedStructureHash, HostedContentHash);
	TestTrue(TEXT("Factory-spawned actor should expose definition host state"), bHasHostState);

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_RuntimeSpawnServiceTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.SpawnService.RuntimeServicePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_RuntimeSpawnServiceTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	TSharedPtr<IObjectSpawnService> SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
	TestTrue(TEXT("IObjectSpawnService should be registered"), SpawnService.IsValid());
	if (!SpawnService.IsValid())
	{
		return false;
	}

	const FPrimaryAssetId GrandPaId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("GrandPa")));
	FText SpawnError;
	AActor* Spawned = SpawnService->SpawnFromDefinition(
		World,
		GrandPaId,
		FTransform::Identity,
		&SpawnError);
	TestNotNull(TEXT("Runtime spawn service should spawn GrandPa definition"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn service error: %s"), *SpawnError.ToString()));
		return false;
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_BaselineCapabilityCoverageTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Baseline.CapabilityCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_BaselineCapabilityCoverageTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	struct FCapabilityExpectation
	{
		FPrimaryAssetId DefinitionId;
		FName CapabilityType;
	};

	const TArray<FCapabilityExpectation> Expectations = {
		{ FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Door_GrandPa"))), FName(TEXT("Lockable")) },
		{ FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Door_GrandPa"))), FName(TEXT("Hinged")) },
		{ FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Cigarette"))), FName(TEXT("Pickup")) },
		{ FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("GrandPa"))), FName(TEXT("Dialogue")) },
		{ FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("GrandPa"))), FName(TEXT("ActorWatcher")) }
	};

	for (const FCapabilityExpectation& Expectation : Expectations)
	{
		UClass* CapabilityClass = FCapabilityRegistry::GetCapabilityClass(Expectation.CapabilityType);
		TestNotNull(
			*FString::Printf(TEXT("Capability '%s' should be registered"), *Expectation.CapabilityType.ToString()),
			CapabilityClass);
		if (!CapabilityClass)
		{
			return false;
		}

		FText SpawnError;
		AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
			World,
			Expectation.DefinitionId,
			FTransform::Identity,
			FActorSpawnParameters(),
			&SpawnError);
		TestNotNull(
			*FString::Printf(TEXT("Definition '%s' should spawn"), *Expectation.DefinitionId.ToString()),
			Spawned);
		if (!Spawned)
		{
			AddError(FString::Printf(TEXT("Spawn failed for %s: %s"),
				*Expectation.DefinitionId.ToString(),
				*SpawnError.ToString()));
			return false;
		}

		UActorComponent* CapabilityComponent = Spawned->GetComponentByClass(CapabilityClass);
		TestNotNull(
			*FString::Printf(TEXT("Definition '%s' should include capability '%s'"),
				*Expectation.DefinitionId.ToString(),
				*Expectation.CapabilityType.ToString()),
			CapabilityComponent);

		Spawned->Destroy();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_DoorGrandPaSpawnContractTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Baseline.DoorGrandPaSpawnContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_DoorGrandPaSpawnContractTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	const FPrimaryAssetId DoorId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Door_GrandPa")));

	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		DoorId,
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Door_GrandPa definition should spawn"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn failed: %s"), *SpawnError.ToString()));
		return false;
	}

	FPrimaryAssetId HostedId;
	FString HostedStructureHash;
	FString HostedContentHash;
	const bool bHasHostState = ProjectWorldDefinitionHost::ReadHostState(
		Spawned, HostedId, HostedStructureHash, HostedContentHash);
	TestTrue(TEXT("Spawned Door_GrandPa actor should expose host state"), bHasHostState);
	if (bHasHostState)
	{
		TestEqual(TEXT("Host DefinitionId should match Door_GrandPa"), HostedId.ToString(), DoorId.ToString());
	}

	TestTrue(
		TEXT("Spawned Door_GrandPa actor should include scenario actor tag"),
		Spawned->Tags.Contains(FName(TEXT("Scenario.GrandpaDoor"))));

	ULockableComponent* Lockable = Spawned->FindComponentByClass<ULockableComponent>();
	TestNotNull(TEXT("Door_GrandPa should have Lockable capability"), Lockable);
	if (Lockable)
	{
		TestTrue(TEXT("LockableComponent lock tag should be valid"), Lockable->GetLockTag().IsValid());
		TestEqual(
			TEXT("LockableComponent lock tag should match scenario key"),
			Lockable->GetLockTag().ToString(),
			FString(TEXT("Scenario.GrandpaDoor")));
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SpawnClassCookedPreflightTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.SpawnClass.CookedPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SpawnClassCookedPreflightTest::RunTest(const FString& Parameters)
{
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	UObjectDefinition* Def = NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	FString Error;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT("/Game/Project/Resources/Characters/GrandPa/GrandPa_BP")),
		Def,
		Error);
	TestTrue(TEXT("Parser should accept spawnClass for cook preflight"), bParsed);
	TestTrue(TEXT("spawnClass should be set for cook preflight"), !Def->SpawnClass.IsNull());
	if (Def->SpawnClass.IsNull())
	{
		return false;
	}

	const FString ClassObjectPath = Def->SpawnClass.ToSoftObjectPath().ToString();
	TestTrue(
		TEXT("spawnClass should be normalized to generated class object path"),
		ClassObjectPath.EndsWith(TEXT("_C")));

	UClass* LoadedClass = Def->SpawnClass.LoadSynchronous();
	TestNotNull(TEXT("spawnClass soft reference should resolve to loadable class"), LoadedClass);
	if (!LoadedClass)
	{
		return false;
	}

	TestTrue(TEXT("Loaded spawnClass must derive from AActor"), LoadedClass->IsChildOf(AActor::StaticClass()));
	TestEqual(
		TEXT("spawnClass package should match GrandPa blueprint package"),
		Def->SpawnClass.ToSoftObjectPath().GetLongPackageName(),
		FString(TEXT("/Game/Project/Resources/Characters/GrandPa/GrandPa_BP")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_DefinitionHostRoundTripTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Host.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_DefinitionHostRoundTripTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Actor should spawn"), Actor);
	if (!Actor)
	{
		return false;
	}

	UObject* InitialHost = ProjectWorldDefinitionHost::FindHostObject(Actor);
	TestNull(TEXT("Fresh actor should not have host object"), InitialHost);

	const FPrimaryAssetId ExpectedId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("HostRoundTrip")));
	const bool bWrite = ProjectWorldDefinitionHost::WriteHostState(
		Actor, ExpectedId, TEXT("StructHashA"), TEXT("ContentHashA"), true);
	TestTrue(TEXT("WriteHostState should inject and write host data"), bWrite);

	FPrimaryAssetId ReadId;
	FString ReadStructureHash;
	FString ReadContentHash;
	const bool bRead = ProjectWorldDefinitionHost::ReadHostState(
		Actor, ReadId, ReadStructureHash, ReadContentHash);
	TestTrue(TEXT("ReadHostState should succeed after injected host"), bRead);
	if (bRead)
	{
		TestEqual(TEXT("DefinitionId should round-trip"), ReadId.ToString(), ExpectedId.ToString());
		TestEqual(TEXT("Structure hash should round-trip"), ReadStructureHash, FString(TEXT("StructHashA")));
		TestEqual(TEXT("Content hash should round-trip"), ReadContentHash, FString(TEXT("ContentHashA")));
	}

	AActor* NoInjectActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("No-inject actor should spawn"), NoInjectActor);
	if (NoInjectActor)
	{
		const bool bWriteNoInject = ProjectWorldDefinitionHost::WriteHostState(
			NoInjectActor, ExpectedId, TEXT("X"), TEXT("Y"), false);
		TestFalse(TEXT("WriteHostState should fail when host missing and injection disabled"), bWriteNoInject);
		NoInjectActor->Destroy();
	}

	Actor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_HostSerializationPersistenceTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Host.SerializationPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_HostSerializationPersistenceTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	AActor* SourceActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Source actor should spawn"), SourceActor);
	if (!SourceActor)
	{
		return false;
	}

	const FPrimaryAssetId ExpectedId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("HostPersist")));
	const bool bWrite = ProjectWorldDefinitionHost::WriteHostState(
		SourceActor,
		ExpectedId,
		TEXT("StructPersist"),
		TEXT("ContentPersist"),
		true);
	TestTrue(TEXT("Source actor should receive injected host state"), bWrite);
	if (!bWrite)
	{
		SourceActor->Destroy();
		return false;
	}

	AActor* DuplicatedActor = Cast<AActor>(StaticDuplicateObject(
		SourceActor,
		World->GetCurrentLevel(),
		NAME_None,
		RF_Transient));
	TestNotNull(TEXT("Actor duplication should succeed"), DuplicatedActor);
	if (!DuplicatedActor)
	{
		SourceActor->Destroy();
		return false;
	}

	FPrimaryAssetId DuplicatedId;
	FString DuplicatedStructureHash;
	FString DuplicatedContentHash;
	const bool bReadDuplicated = ProjectWorldDefinitionHost::ReadHostState(
		DuplicatedActor,
		DuplicatedId,
		DuplicatedStructureHash,
		DuplicatedContentHash);
	TestTrue(TEXT("Duplicated actor should keep injected host state"), bReadDuplicated);
	if (bReadDuplicated)
	{
		TestEqual(TEXT("Duplicated definition id should persist"), DuplicatedId.ToString(), ExpectedId.ToString());
		TestEqual(TEXT("Duplicated structure hash should persist"), DuplicatedStructureHash, FString(TEXT("StructPersist")));
		TestEqual(TEXT("Duplicated content hash should persist"), DuplicatedContentHash, FString(TEXT("ContentPersist")));
	}

	DuplicatedActor->Destroy();
	SourceActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_InteractionSelectionMeshScopedWinsTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Interaction.MeshScopedSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_InteractionSelectionMeshScopedWinsTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	APawn* Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Pawn should spawn"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	UInteractionComponent* Interaction = NewObject<UInteractionComponent>(Pawn, TEXT("TestInteraction"));
	Pawn->AddInstanceComponent(Interaction);
	Interaction->RegisterComponent();

	AActor* Target = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Target should spawn"), Target);
	if (!Target)
	{
		Pawn->Destroy();
		return false;
	}

	USceneComponent* Root = NewObject<USceneComponent>(Target, TEXT("Root"));
	Target->SetRootComponent(Root);
	Target->AddInstanceComponent(Root);
	Root->RegisterComponent();

	UStaticMeshComponent* HitMesh = NewObject<UStaticMeshComponent>(Target, TEXT("HitMesh"));
	HitMesh->SetupAttachment(Root);
	Target->AddInstanceComponent(HitMesh);
	HitMesh->RegisterComponent();

	UStaticMeshComponent* OtherMesh = NewObject<UStaticMeshComponent>(Target, TEXT("OtherMesh"));
	OtherMesh->SetupAttachment(Root);
	Target->AddInstanceComponent(OtherMesh);
	OtherMesh->RegisterComponent();

	ULockableComponent* Lockable = NewObject<ULockableComponent>(Target, TEXT("Lockable"));
	Target->AddInstanceComponent(Lockable);
	Lockable->RegisterComponent();

	USpringRotatorComponent* Rotator = NewObject<USpringRotatorComponent>(Target, TEXT("Rotator"));
	Target->AddInstanceComponent(Rotator);
	Rotator->RegisterComponent();

	// Case A: mesh-scoped capability targets hit mesh, should win over actor-scoped lockable.
	IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(Rotator, HitMesh);
	Interaction->TestOnly_SetFocusedActor(Target, HitMesh);
	TestEqual(
		TEXT("Mesh-scoped capability should provide focus label"),
		Interaction->GetFocusedLabel_Implementation().ToString(),
		FString(TEXT("Open")));
	TestEqual(
		TEXT("Highlight should resolve to mesh-scoped target"),
		Interaction->GetFocusedComponent_Implementation(),
		static_cast<UPrimitiveComponent*>(HitMesh));

	// Case B: mesh-scoped capability targets another sibling mesh, actor-scoped fallback should win.
	IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(Rotator, OtherMesh);
	Interaction->TestOnly_SetFocusedActor(Target, HitMesh);
	TestEqual(
		TEXT("Actor-scoped fallback should use default interact label"),
		Interaction->GetFocusedLabel_Implementation().ToString(),
		FString(TEXT("Interact")));

	Target->Destroy();
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_InteractionActorInterfaceExecutionTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Interaction.ActorInterfaceExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_InteractionActorInterfaceExecutionTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	APawn* Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Pawn should spawn"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	UInteractionComponent* Interaction = NewObject<UInteractionComponent>(Pawn, TEXT("TestInteractionActorInterface"));
	Pawn->AddInstanceComponent(Interaction);
	Interaction->RegisterComponent();

	AProjectInteractionActorInterfaceTestActor* Target = World->SpawnActor<AProjectInteractionActorInterfaceTestActor>(
		AProjectInteractionActorInterfaceTestActor::StaticClass(),
		FTransform::Identity);
	TestNotNull(TEXT("Actor-interface target should spawn"), Target);
	if (!Target || !Target->TestMesh)
	{
		if (Target)
		{
			Target->Destroy();
		}
		Pawn->Destroy();
		return false;
	}

	UProjectInteractionCounterCapabilityComponent* FallbackComponent =
		NewObject<UProjectInteractionCounterCapabilityComponent>(Target, TEXT("FallbackCapability"));
	Target->AddInstanceComponent(FallbackComponent);
	FallbackComponent->RegisterComponent();
	FallbackComponent->InteractionLabel = FText::FromString(TEXT("Fallback"));
	FallbackComponent->InteractPriority = 999;
	IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(FallbackComponent, Target->TestMesh);

	Interaction->TestOnly_SetFocusedActor(Target, Target->TestMesh);
	TestEqual(
		TEXT("Focus should come from actor interface before component fallback"),
		Interaction->GetFocusedLabel_Implementation().ToString(),
		FString(TEXT("ActorInterface")));

#if WITH_DEV_AUTOMATION_TESTS
	const bool bHandled = Interaction->TestOnly_ExecuteInteraction(Target, Target->TestMesh, Pawn);
	TestTrue(TEXT("Actor-interface path should handle interaction"), bHandled);
#else
	AddError(TEXT("WITH_DEV_AUTOMATION_TESTS is disabled; actor-interface execution hook is unavailable."));
	const bool bHandled = false;
#endif

	TestEqual(TEXT("Actor OnInteract should execute exactly once"), Target->OnInteractCallCount, 1);
	TestEqual(TEXT("Fallback component should not execute when actor interface exists"), FallbackComponent->InteractionCallCount, 0);
	TestTrue(TEXT("Actor interface should receive pawn instigator"), Target->LastInstigator == Pawn);

	Target->Destroy();
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_InteractableActorSingleSelectionTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Interaction.ActorInterfaceSingleSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_InteractableActorSingleSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	APawn* Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Pawn should spawn"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	AInteractableActor* Target = World->SpawnActor<AInteractableActor>(AInteractableActor::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Interactable target should spawn"), Target);
	if (!Target)
	{
		Pawn->Destroy();
		return false;
	}

	USceneComponent* Root = NewObject<USceneComponent>(Target, TEXT("Root"));
	Target->SetRootComponent(Root);
	Target->AddInstanceComponent(Root);
	Root->RegisterComponent();

	UStaticMeshComponent* HitMesh = NewObject<UStaticMeshComponent>(Target, TEXT("HitMesh"));
	HitMesh->SetupAttachment(Root);
	Target->AddInstanceComponent(HitMesh);
	HitMesh->RegisterComponent();

	UProjectInteractionCounterCapabilityComponent* HighPriority =
		NewObject<UProjectInteractionCounterCapabilityComponent>(Target, TEXT("HighPriorityCap"));
	Target->AddInstanceComponent(HighPriority);
	HighPriority->RegisterComponent();
	HighPriority->InteractPriority = 200;
	HighPriority->InteractionLabel = FText::FromString(TEXT("High"));
	IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(HighPriority, HitMesh);

	UProjectInteractionCounterCapabilityComponent* LowPriority =
		NewObject<UProjectInteractionCounterCapabilityComponent>(Target, TEXT("LowPriorityCap"));
	Target->AddInstanceComponent(LowPriority);
	LowPriority->RegisterComponent();
	LowPriority->InteractPriority = 100;
	LowPriority->InteractionLabel = FText::FromString(TEXT("Low"));
	IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(LowPriority, HitMesh);

	Target->RefreshInteractableComponents();

	const FInteractionFocusInfo Focus = IInteractableTargetInterface::Execute_GetFocusInfo(Target, HitMesh);
	TestEqual(TEXT("Focus should resolve highest-priority capability label"), Focus.Label.ToString(), FString(TEXT("High")));

	const bool bHandledWithHit = IInteractableTargetInterface::Execute_OnInteract(Target, Pawn, HitMesh);
	TestTrue(TEXT("OnInteract with mesh hit should be handled"), bHandledWithHit);
	TestEqual(TEXT("High-priority capability should execute once"), HighPriority->InteractionCallCount, 1);
	TestEqual(TEXT("Low-priority capability should not execute"), LowPriority->InteractionCallCount, 0);

	const bool bHandledNoHit = IInteractableTargetInterface::Execute_OnInteract(Target, Pawn, nullptr);
	TestTrue(TEXT("OnInteract with no hit should still execute single selected capability"), bHandledNoHit);
	TestEqual(TEXT("No-hit branch should still use highest-priority capability"), HighPriority->InteractionCallCount, 2);
	TestEqual(TEXT("No-hit branch should not fan out to lower-priority capability"), LowPriority->InteractionCallCount, 0);

	Target->Destroy();
	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SyncPassiveLoadReapplyTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Sync.PassiveLoad.Reapply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SyncPassiveLoadReapplyTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	UDefinitionActorSyncSubsystem* SyncSubsystem = OPG_ResolveDefinitionActorSyncSubsystem();
	TestNotNull(TEXT("DefinitionActorSyncSubsystem should be available"), SyncSubsystem);
	if (!SyncSubsystem)
	{
		return false;
	}

	UObjectDefinition* Def = OPG_MakeTransientObjectDefinition(FName(TEXT("PassiveReapplyDef")), TEXT("S_Reapply"), TEXT("C_New"));
	const FPrimaryAssetId DefId = Def->GetPrimaryAssetId();
	TestTrue(TEXT("Reapply definition id should be valid"), DefId.IsValid());
	if (!DefId.IsValid())
	{
		return false;
	}

	UDefinitionActorSyncSubsystem::TestOnly_RegisterDefinitionOverride(DefId, Def);

	AProjectSyncDefinitionApplicableTestActor* Actor = World->SpawnActor<AProjectSyncDefinitionApplicableTestActor>(
		AProjectSyncDefinitionApplicableTestActor::StaticClass(),
		FTransform::Identity);
	TestNotNull(TEXT("Passive reapply actor should spawn"), Actor);
	if (!Actor)
	{
		UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides();
		return false;
	}

	Actor->bApplyDefinitionResult = true;
	const bool bWroteHost = ProjectWorldDefinitionHost::WriteHostState(
		Actor,
		DefId,
		TEXT("S_Reapply"),
		TEXT("C_Old"),
		true);
	TestTrue(TEXT("Passive reapply actor should have host state"), bWroteHost);

	TArray<AActor*> LoadedActors;
	LoadedActors.Add(Actor);
	SyncSubsystem->TestOnly_OnActorsLoadedIntoLevel(LoadedActors);

	TestEqual(TEXT("Passive load reapply should call ApplyDefinition once"), Actor->ApplyDefinitionCallCount, 1);
	TestTrue(TEXT("Passive load reapply should pass correct definition"), Actor->LastAppliedDefinition == Def);

	UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides();
	Actor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SyncPassiveLoadReplaceTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Sync.PassiveLoad.Replace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SyncPassiveLoadReplaceTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	UDefinitionActorSyncSubsystem* SyncSubsystem = OPG_ResolveDefinitionActorSyncSubsystem();
	TestNotNull(TEXT("DefinitionActorSyncSubsystem should be available"), SyncSubsystem);
	if (!SyncSubsystem)
	{
		return false;
	}

	UObjectDefinition* Def = OPG_MakeTransientObjectDefinition(FName(TEXT("PassiveReplaceDef")), TEXT("S_Replace"), TEXT("C_New"));
	const FPrimaryAssetId DefId = Def->GetPrimaryAssetId();
	TestTrue(TEXT("Replace definition id should be valid"), DefId.IsValid());
	if (!DefId.IsValid())
	{
		return false;
	}

	UDefinitionActorSyncSubsystem::TestOnly_RegisterDefinitionOverride(DefId, Def);

	AProjectSyncDefinitionApplicableTestActor* Actor = World->SpawnActor<AProjectSyncDefinitionApplicableTestActor>(
		AProjectSyncDefinitionApplicableTestActor::StaticClass(),
		FTransform::Identity);
	TestNotNull(TEXT("Passive replace actor should spawn"), Actor);
	if (!Actor)
	{
		UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides();
		return false;
	}

	const FString ReplaceLabel = TEXT("PassiveReplaceActor");
	Actor->SetActorLabel(ReplaceLabel);
	Actor->bApplyDefinitionResult = false;
	const bool bWroteHost = ProjectWorldDefinitionHost::WriteHostState(
		Actor,
		DefId,
		TEXT("S_Replace"),
		TEXT("C_Old"),
		true);
	TestTrue(TEXT("Passive replace actor should have host state"), bWroteHost);

	// Passive-load test runs in automation world where editor actor subsystem may log this
	// while still falling back to Actor->Destroy and completing replacement.
	AddExpectedError(TEXT("The Editor is currently in a play mode."), EAutomationExpectedErrorFlags::Contains, 1);

	TArray<AActor*> LoadedActors;
	LoadedActors.Add(Actor);
	SyncSubsystem->TestOnly_OnActorsLoadedIntoLevel(LoadedActors);

	TestEqual(TEXT("Passive load replace should call ApplyDefinition once before fallback"), Actor->ApplyDefinitionCallCount, 1);

	AActor* Replacement = OPG_FindActorByLabel(World, ReplaceLabel, Actor);
	TestNotNull(TEXT("Passive load replace should spawn replacement actor"), Replacement);
	if (Replacement)
	{
		TestFalse(TEXT("Replacement actor should not be same pointer as original"), Replacement == Actor);
		TestTrue(
			TEXT("Replacement from transient ObjectDefinition should follow legacy default spawn class"),
			Replacement->IsA(AInteractableActor::StaticClass()));
		Replacement->Destroy();
	}

	TestTrue(
		TEXT("Original actor should be destroyed during replace"),
		Actor->IsActorBeingDestroyed() || Actor->IsPendingKillPending());

	UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides();
	if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
	{
		Actor->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_SyncLegacyAndHostPathTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Sync.PassiveLoad.LegacyAndHostPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_SyncLegacyAndHostPathTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	UDefinitionActorSyncSubsystem* SyncSubsystem = OPG_ResolveDefinitionActorSyncSubsystem();
	TestNotNull(TEXT("DefinitionActorSyncSubsystem should be available"), SyncSubsystem);
	if (!SyncSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();
	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		OPG_MakeMinimalObjectJson(TEXT("")),
		Def.Get(),
		ParseError);
	TestTrue(TEXT("Sync legacy/host definition parse should succeed"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	Def->ObjectId = FName(TEXT("PassiveLegacyHostDef"));
	Def->DefinitionStructureHash = TEXT("S_LegacyHost");
	Def->DefinitionContentHash = TEXT("C_New");
	const FPrimaryAssetId DefId = Def->GetPrimaryAssetId();
	TestTrue(TEXT("Legacy/host definition id should be valid"), DefId.IsValid());
	if (!DefId.IsValid())
	{
		return false;
	}

	UDefinitionActorSyncSubsystem::TestOnly_RegisterDefinitionOverride(DefId, Def.Get());

	FText SpawnError;
	AActor* LegacyActor = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Legacy actor should spawn from definition"), LegacyActor);
	if (!LegacyActor)
	{
		UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides();
		return false;
	}

	const bool bLegacyWrite = ProjectWorldDefinitionHost::WriteHostState(
		LegacyActor,
		DefId,
		TEXT("S_LegacyHost"),
		TEXT("C_Old"),
		true);
	TestTrue(TEXT("Legacy actor host state should be writable"), bLegacyWrite);

	AProjectSyncDefinitionApplicableTestActor* HostComponentActor = World->SpawnActor<AProjectSyncDefinitionApplicableTestActor>(
		AProjectSyncDefinitionApplicableTestActor::StaticClass(),
		FTransform::Identity);
	TestNotNull(TEXT("Host-component actor should spawn"), HostComponentActor);
	if (!HostComponentActor)
	{
		UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides();
		LegacyActor->Destroy();
		return false;
	}

	HostComponentActor->bApplyDefinitionResult = true;
	const bool bHostWrite = ProjectWorldDefinitionHost::WriteHostState(
		HostComponentActor,
		DefId,
		TEXT("S_LegacyHost"),
		TEXT("C_Old"),
		true);
	TestTrue(TEXT("Host-component actor should receive host state"), bHostWrite);

	TArray<AActor*> LoadedActors;
	LoadedActors.Add(LegacyActor);
	LoadedActors.Add(HostComponentActor);
	SyncSubsystem->TestOnly_OnActorsLoadedIntoLevel(LoadedActors);

	TestEqual(
		TEXT("Host-component actor should reapply exactly once"),
		HostComponentActor->ApplyDefinitionCallCount,
		1);

	FPrimaryAssetId LegacyHostedId;
	FString LegacyStructureHash;
	FString LegacyContentHash;
	const bool bReadLegacyHost = ProjectWorldDefinitionHost::ReadHostState(
		LegacyActor,
		LegacyHostedId,
		LegacyStructureHash,
		LegacyContentHash);
	TestTrue(TEXT("Legacy actor should still expose host state after passive sync"), bReadLegacyHost);
	if (bReadLegacyHost)
	{
		TestEqual(
			TEXT("Legacy actor content hash should be updated by passive reapply"),
			LegacyContentHash,
			FString(TEXT("C_New")));
	}

	UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides();
	HostComponentActor->Destroy();
	LegacyActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_GrandPaPilotSpawnClassTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Pilot.GrandPaSpawnClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_GrandPaPilotSpawnClassTest::RunTest(const FString& Parameters)
{
	const FString JsonPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Plugins/Resources/ProjectObject/Content/Human/GrandPa/GrandPa.json"));

	FString JsonText;
	const bool bReadJson = FFileHelper::LoadFileToString(JsonText, *JsonPath);
	TestTrue(TEXT("GrandPa.json should be readable"), bReadJson);
	if (!bReadJson)
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	const bool bParsedJson = FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid();
	TestTrue(TEXT("GrandPa.json should parse"), bParsedJson);
	if (!bParsedJson)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	FString ParseError;
	const bool bParsedDef = FDefinitionJsonParser::ParseJsonToAsset(TypeInfo, JsonObject, Def.Get(), ParseError);
	TestTrue(TEXT("GrandPa definition parse should succeed"), bParsedDef);
	if (!bParsedDef)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	TestTrue(TEXT("GrandPa pilot must define spawnClass"), !Def->SpawnClass.IsNull());
	if (!Def->SpawnClass.IsNull())
	{
		TestEqual(
			TEXT("GrandPa spawnClass should normalize to class object path"),
			Def->SpawnClass.ToSoftObjectPath().ToString(),
			FString(TEXT("/Game/Project/Resources/Characters/GrandPa/GrandPa_BP.GrandPa_BP_C")));
	}

	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);

	TestNotNull(TEXT("GrandPa pilot should spawn from parsed definition"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn failed: %s"), *SpawnError.ToString()));
		return false;
	}

	TestFalse(
		TEXT("GrandPa pilot should not fall back to AInteractableActor"),
		Spawned->IsA(AInteractableActor::StaticClass()));

	FPrimaryAssetId HostedDefinitionId;
	FString HostedStructureHash;
	FString HostedContentHash;
	const bool bHasHostState = ProjectWorldDefinitionHost::ReadHostState(
		Spawned, HostedDefinitionId, HostedStructureHash, HostedContentHash);
	TestTrue(TEXT("Spawned pilot actor should expose definition host state"), bHasHostState);
	if (bHasHostState)
	{
		TestEqual(
			TEXT("Host definition id should match parsed definition id"),
			HostedDefinitionId.ToString(),
			Def->GetPrimaryAssetId().ToString());
	}

	if (ACharacter* SpawnedCharacter = Cast<ACharacter>(Spawned))
	{
		USkeletalMeshComponent* CharacterMesh = SpawnedCharacter->GetMesh();
		TestNotNull(TEXT("GrandPa spawnClass should expose CharacterMesh0"), CharacterMesh);
		if (CharacterMesh)
		{
			TestTrue(
				TEXT("CharacterMesh0 should be reused/tagged as DefMeshId=body (no duplicate generated body mesh)"),
				CharacterMesh->ComponentTags.Contains(FName(TEXT("DefMeshId=body"))));
		}
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_DoorGrandPaPilotLockTagTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Pilot.DoorGrandPaLockTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_DoorGrandPaPilotLockTagTest::RunTest(const FString& Parameters)
{
	const FString JsonPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Plugins/Resources/ProjectObject/Content/HumanMade/Building/Fenestration/Door/Scenario/Door_GrandPa.json"));

	FString JsonText;
	const bool bReadJson = FFileHelper::LoadFileToString(JsonText, *JsonPath);
	TestTrue(TEXT("Door_GrandPa.json should be readable"), bReadJson);
	if (!bReadJson)
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	const bool bParsedJson = FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid();
	TestTrue(TEXT("Door_GrandPa.json should parse"), bParsedJson);
	if (!bParsedJson)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	FString ParseError;
	const bool bParsedDef = FDefinitionJsonParser::ParseJsonToAsset(TypeInfo, JsonObject, Def.Get(), ParseError);
	TestTrue(TEXT("Door_GrandPa definition parse should succeed"), bParsedDef);
	if (!bParsedDef)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);

	TestNotNull(TEXT("Door_GrandPa should spawn from parsed definition"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn failed: %s"), *SpawnError.ToString()));
		return false;
	}

	ULockableComponent* Lockable = Spawned->FindComponentByClass<ULockableComponent>();
	TestNotNull(TEXT("Door_GrandPa should have LockableComponent"), Lockable);
	if (!Lockable)
	{
		Spawned->Destroy();
		return false;
	}

	FPrimaryAssetId HostedDefinitionId;
	FString HostedStructureHash;
	FString HostedContentHash;
	const bool bHasHostState = ProjectWorldDefinitionHost::ReadHostState(
		Spawned, HostedDefinitionId, HostedStructureHash, HostedContentHash);
	TestTrue(TEXT("Door_GrandPa spawned actor should expose definition host state"), bHasHostState);
	if (bHasHostState)
	{
		TestEqual(
			TEXT("Door_GrandPa host definition id should match parsed definition id"),
			HostedDefinitionId.ToString(),
			Def->GetPrimaryAssetId().ToString());
	}

	const FGameplayTag ExpectedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Scenario.GrandpaDoor")), false);
	TestTrue(TEXT("Scenario.GrandpaDoor gameplay tag must be registered"), ExpectedTag.IsValid());
	TestTrue(TEXT("Lockable LockTag should be valid"), Lockable->LockTag.IsValid());
	if (Lockable->LockTag.IsValid())
	{
		TestEqual(
			TEXT("Lockable LockTag should match scenario tag"),
			Lockable->LockTag.ToString(),
			FString(TEXT("Scenario.GrandpaDoor")));
	}

	UPrimitiveComponent* LockableTargetMesh =
		IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Lockable);
	TestNotNull(TEXT("Lockable should accept mesh scope from definition"), LockableTargetMesh);

	UStaticMeshComponent* HitMesh = Spawned->FindComponentByClass<UStaticMeshComponent>();
	TestNotNull(TEXT("Door_GrandPa should have static mesh component"), HitMesh);
	if (HitMesh)
	{
		TestTrue(
			TEXT("Lockable target mesh should match door static mesh"),
			LockableTargetMesh == HitMesh);
	}

	TestTrue(TEXT("Door_GrandPa should spawn locked by default"), Lockable->IsLocked());
	TestTrue(
		TEXT("Door_GrandPa spawned actor should include scenario actor tag"),
		Spawned->Tags.Contains(FName(TEXT("Scenario.GrandpaDoor"))));

	APawn* InstigatorPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Instigator pawn should spawn for interaction test"), InstigatorPawn);
	if (InstigatorPawn && HitMesh)
	{
		const bool bInteractionHandled = IInteractableTargetInterface::Execute_OnInteract(
			Spawned, InstigatorPawn, HitMesh);
		TestFalse(TEXT("Locked door interaction should be blocked by Lockable capability"), bInteractionHandled);
		InstigatorPawn->Destroy();
	}

	// PIE/world duplication guard: Lockable mesh scope binding must survive duplication.
	// If it is lost, mesh-scoped SpringRotator can win focus/interaction and door appears unlocked.
	AActor* DuplicatedActor = Cast<AActor>(StaticDuplicateObject(
		Spawned,
		World->GetCurrentLevel(),
		NAME_None,
		RF_Transient));
	TestNotNull(TEXT("Door_GrandPa actor duplication should succeed"), DuplicatedActor);
	if (DuplicatedActor)
	{
		AInteractableActor* DuplicatedInteractable = Cast<AInteractableActor>(DuplicatedActor);
		TestNotNull(TEXT("Duplicated actor should remain AInteractableActor"), DuplicatedInteractable);
		if (DuplicatedInteractable)
		{
			DuplicatedInteractable->RefreshInteractableComponents();
		}

		ULockableComponent* DuplicatedLockable = DuplicatedActor->FindComponentByClass<ULockableComponent>();
		TestNotNull(TEXT("Duplicated Door_GrandPa should have LockableComponent"), DuplicatedLockable);

		UStaticMeshComponent* DuplicatedHitMesh = DuplicatedActor->FindComponentByClass<UStaticMeshComponent>();
		TestNotNull(TEXT("Duplicated Door_GrandPa should have static mesh component"), DuplicatedHitMesh);

		if (DuplicatedLockable && DuplicatedHitMesh)
		{
			UPrimitiveComponent* DuplicatedLockableTargetMesh =
				IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(DuplicatedLockable);
			TestNotNull(TEXT("Duplicated Lockable should preserve mesh scope binding"), DuplicatedLockableTargetMesh);
			TestTrue(
				TEXT("Duplicated Lockable target mesh should match duplicated door mesh"),
				DuplicatedLockableTargetMesh == DuplicatedHitMesh);

			APawn* DuplicatedInstigatorPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
			TestNotNull(TEXT("Duplicated instigator pawn should spawn"), DuplicatedInstigatorPawn);
			if (DuplicatedInstigatorPawn)
			{
				const bool bDuplicatedInteractionHandled = IInteractableTargetInterface::Execute_OnInteract(
					DuplicatedActor, DuplicatedInstigatorPawn, DuplicatedHitMesh);
				TestFalse(
					TEXT("Duplicated locked door interaction should be blocked by Lockable capability"),
					bDuplicatedInteractionHandled);
				DuplicatedInstigatorPawn->Destroy();
			}
		}

		DuplicatedActor->Destroy();
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_DoorGrandPaWatchStartsDialogueTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Pilot.DoorGrandPaWatchStartsDialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_DoorGrandPaWatchStartsDialogueTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	TSharedPtr<IDialogueService> DialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TestTrue(TEXT("Dialogue service should be registered"), DialogueService.IsValid());
	if (!DialogueService.IsValid())
	{
		return false;
	}

	if (DialogueService->IsDialogueActive())
	{
		DialogueService->EndDialogue();
	}

	const FPrimaryAssetId DoorId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Door_GrandPa")));
	const FPrimaryAssetId GrandpaId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("GrandPa")));

	FText SpawnError;
	AActor* DoorActor = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		DoorId,
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Door_GrandPa should spawn"), DoorActor);
	if (!DoorActor)
	{
		AddError(FString::Printf(TEXT("Door spawn failed: %s"), *SpawnError.ToString()));
		return false;
	}

	FTransform GrandpaTransform = DoorActor->GetActorTransform();
	GrandpaTransform.SetLocation(DoorActor->GetActorLocation() + FVector(100.f, 0.f, 0.f));

	AActor* GrandpaActor = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		GrandpaId,
		GrandpaTransform,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("GrandPa should spawn"), GrandpaActor);
	if (!GrandpaActor)
	{
		AddError(FString::Printf(TEXT("GrandPa spawn failed: %s"), *SpawnError.ToString()));
		DoorActor->Destroy();
		return false;
	}

	UActorWatcherComponent* Watcher = GrandpaActor->FindComponentByClass<UActorWatcherComponent>();
	TestNotNull(TEXT("GrandPa should have ActorWatcher capability"), Watcher);
	if (Watcher)
	{
		TestTrue(
			TEXT("GrandPa watcher should resolve the spawned door"),
			Watcher->GetCurrentWatchedActor() == DoorActor);
	}

	ULockableComponent* Lockable = DoorActor->FindComponentByClass<ULockableComponent>();
	TestNotNull(TEXT("Door_GrandPa should have Lockable capability"), Lockable);

	UStaticMeshComponent* DoorMesh = DoorActor->FindComponentByClass<UStaticMeshComponent>();
	TestNotNull(TEXT("Door_GrandPa should have static mesh"), DoorMesh);

	APawn* InstigatorPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Instigator pawn should spawn"), InstigatorPawn);

	if (Lockable && DoorMesh && InstigatorPawn)
	{
		const bool bInteractionHandled = IInteractableTargetInterface::Execute_OnInteract(
			DoorActor, InstigatorPawn, DoorMesh);
		TestFalse(TEXT("Locked door interaction should be blocked"), bInteractionHandled);

		TestTrue(
			TEXT("Lock denial watcher path should activate dialogue service"),
			DialogueService->IsDialogueActive());
		if (DialogueService->IsDialogueActive())
		{
			TestEqual(
				TEXT("Watcher-started dialogue should expose expected tree id"),
				DialogueService->GetCurrentTreeId(),
				FName(TEXT("DLG_GrandPa_Door")));
			TestEqual(
				TEXT("Watcher-started dialogue should begin at greeting node"),
				DialogueService->GetCurrentNodeId(),
				FName(TEXT("greeting")));
			TestTrue(
				TEXT("Active dialogue owner should be GrandPa actor"),
				DialogueService->GetActiveDialogueOwner() == GrandpaActor);

			TestFalse(
				TEXT("Dialogue speaker should be populated after watcher autostart"),
				DialogueService->GetCurrentSpeaker().IsEmpty());

			TArray<FDialogueOptionView> Options;
			DialogueService->GetCurrentOptions(Options);
			bool bFoundGiveWater = false;
			bool bGiveWaterLocked = false;
			for (const FDialogueOptionView& Option : Options)
			{
				const FString Text = Option.Text.ToString();
				if (Text.Contains(TEXT("take this water"), ESearchCase::IgnoreCase))
				{
					bFoundGiveWater = true;
					bGiveWaterLocked = Option.bLocked;
					break;
				}
			}

			TestTrue(TEXT("Give-water option should remain visible without inventory item"), bFoundGiveWater);
			TestTrue(TEXT("Give-water option should be locked without inventory item"), bGiveWaterLocked);
			DialogueService->EndDialogue();
			TestFalse(TEXT("Dialogue should be inactive after EndDialogue"), DialogueService->IsDialogueActive());
			TestEqual(TEXT("Dialogue tree id should reset after EndDialogue"), DialogueService->GetCurrentTreeId(), NAME_None);
			TestEqual(TEXT("Dialogue node id should reset after EndDialogue"), DialogueService->GetCurrentNodeId(), NAME_None);
			TestTrue(TEXT("Active dialogue owner should reset after EndDialogue"), DialogueService->GetActiveDialogueOwner() == nullptr);
		}
	}

	if (InstigatorPawn)
	{
		InstigatorPawn->Destroy();
	}
	GrandpaActor->Destroy();
	DoorActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_DoorGrandPaScenarioEndToEndTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Pilot.DoorGrandPaScenarioEndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_DoorGrandPaScenarioEndToEndTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	TSharedPtr<IDialogueService> DialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TestTrue(TEXT("Dialogue service should be registered"), DialogueService.IsValid());
	if (!DialogueService.IsValid())
	{
		return false;
	}

	if (DialogueService->IsDialogueActive())
	{
		DialogueService->EndDialogue();
	}

	const FPrimaryAssetId DoorId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Door_GrandPa")));
	const FPrimaryAssetId GrandpaId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("GrandPa")));
	const FPrimaryAssetId WaterBottleBigId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("WaterBottleBig")));

	FText SpawnError;
	AActor* DoorActor = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		DoorId,
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Door_GrandPa should spawn"), DoorActor);
	if (!DoorActor)
	{
		AddError(FString::Printf(TEXT("Door spawn failed: %s"), *SpawnError.ToString()));
		return false;
	}

	FTransform GrandpaTransform = DoorActor->GetActorTransform();
	GrandpaTransform.SetLocation(DoorActor->GetActorLocation() + FVector(100.f, 0.f, 0.f));
	AActor* GrandpaActor = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		GrandpaId,
		GrandpaTransform,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("GrandPa should spawn"), GrandpaActor);
	if (!GrandpaActor)
	{
		AddError(FString::Printf(TEXT("GrandPa spawn failed: %s"), *SpawnError.ToString()));
		DoorActor->Destroy();
		return false;
	}

	UStaticMeshComponent* DoorMesh = DoorActor->FindComponentByClass<UStaticMeshComponent>();
	ULockableComponent* Lockable = DoorActor->FindComponentByClass<ULockableComponent>();
	USpringRotatorComponent* Hinged = DoorActor->FindComponentByClass<USpringRotatorComponent>();
	UProjectDialogueComponent* GrandpaDialogue = GrandpaActor->FindComponentByClass<UProjectDialogueComponent>();

	TestNotNull(TEXT("Door should have mesh"), DoorMesh);
	TestNotNull(TEXT("Door should have lockable capability"), Lockable);
	TestNotNull(TEXT("Door should have hinged capability"), Hinged);
	TestNotNull(TEXT("GrandPa should have dialogue capability"), GrandpaDialogue);

	APawn* InstigatorPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Instigator pawn should spawn"), InstigatorPawn);

	UProjectTestInventoryComponent* TestInventory = nullptr;
	if (InstigatorPawn)
	{
		TestInventory = NewObject<UProjectTestInventoryComponent>(InstigatorPawn, TEXT("TestInventory"));
		InstigatorPawn->AddInstanceComponent(TestInventory);
		TestInventory->RegisterComponent();
		// Scenario uses condition "inventory:WaterBottle*", while players can carry variants.
		// Use WaterBottleBig to validate family matching in condition + consume actions.
		TestInventory->SetItemQuantity(WaterBottleBigId, 1);
	}
	TestNotNull(TEXT("Instigator should have test inventory component"), TestInventory);

	if (DoorMesh && Lockable && Hinged && GrandpaDialogue && InstigatorPawn && TestInventory)
	{
		const bool bFirstInteractionHandled = IInteractableTargetInterface::Execute_OnInteract(
			DoorActor, InstigatorPawn, DoorMesh);
		TestFalse(TEXT("First interaction should be blocked by lock"), bFirstInteractionHandled);
		TestTrue(TEXT("Door should remain locked after deny"), Lockable->IsLocked());

		TestTrue(TEXT("Watcher deny should activate dialogue"), DialogueService->IsDialogueActive());
		TestEqual(
			TEXT("Scenario should start from expected dialogue tree"),
			DialogueService->GetCurrentTreeId(),
			FName(TEXT("DLG_GrandPa_Door")));
		const FName InitialNodeId = DialogueService->GetCurrentNodeId();
		TestEqual(TEXT("Scenario should start from greeting node"), InitialNodeId, FName(TEXT("greeting")));
		TestTrue(
			TEXT("Active dialogue owner should be GrandPa actor"),
			DialogueService->GetActiveDialogueOwner() == GrandpaActor);

		TArray<FDialogueOptionView> Options;
		DialogueService->GetCurrentOptions(Options);
		TestTrue(TEXT("Greeting should expose at least one option"), Options.Num() > 0);

		int32 GiveWaterOptionIndex = INDEX_NONE;
		for (const FDialogueOptionView& Option : Options)
		{
			const FString OptionText = Option.Text.ToString();
			if (OptionText.Contains(TEXT("take this water"), ESearchCase::IgnoreCase))
			{
				GiveWaterOptionIndex = Option.Index;
				break;
			}
		}

		TestTrue(TEXT("Give-water option should be visible when instigator has WaterBottle"), GiveWaterOptionIndex != INDEX_NONE);
		if (GiveWaterOptionIndex != INDEX_NONE)
		{
			DialogueService->SelectOption(GiveWaterOptionIndex);
			TestNotEqual(
				TEXT("Selecting give-water should advance dialogue node"),
				DialogueService->GetCurrentNodeId(),
				InitialNodeId);
		}

		TestFalse(TEXT("Selecting give-water should unlock door"), Lockable->IsLocked());
		TestEqual(TEXT("WaterBottleBig should be consumed via WaterBottle family action"), TestInventory->GetItemQuantity(WaterBottleBigId), 0);
		TestEqual(TEXT("inventory.consume should run exactly once"), TestInventory->ConsumedActionCount, 1);
		TestEqual(TEXT("Consumed item id should resolve to WaterBottleBig variant"), TestInventory->LastConsumedItemId.ToString(), WaterBottleBigId.ToString());
		TestTrue(
			TEXT("motion.open should arm door motion (tick enabled or animating)"),
			Hinged->IsComponentTickEnabled() || Hinged->bIsAnimating);

		// give_water -> thanks
		DialogueService->AdvanceOrEnd();
		// thanks -> end conversation (applies queued dialogue.set_tree)
		DialogueService->AdvanceOrEnd();

		TestFalse(TEXT("Conversation should end after thanks"), DialogueService->IsDialogueActive());
		TestEqual(TEXT("Dialogue tree id should reset after conversation end"), DialogueService->GetCurrentTreeId(), NAME_None);
		TestEqual(TEXT("Dialogue node id should reset after conversation end"), DialogueService->GetCurrentNodeId(), NAME_None);
		TestTrue(
			TEXT("Active dialogue owner should reset after conversation end"),
			DialogueService->GetActiveDialogueOwner() == nullptr);
		TestEqual(
			TEXT("GrandPa dialogue tree should switch to follow-up tree after conversation end"),
			GrandpaDialogue->DialogueTreeAsset.ToSoftObjectPath().ToString(),
			FString(TEXT("/ProjectObject/Human/GrandPa/DLG_GrandPa_Test.DLG_GrandPa_Test")));
	}

	if (DialogueService->IsDialogueActive())
	{
		DialogueService->EndDialogue();
	}

	if (InstigatorPawn)
	{
		InstigatorPawn->Destroy();
	}
	GrandpaActor->Destroy();
	DoorActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_DialogueSameFrameBroadcastGuardTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Pilot.DialogueSameFrameBroadcastGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_DialogueSameFrameBroadcastGuardTest::RunTest(const FString& Parameters)
{
	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	TSharedPtr<IDialogueService> DialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TestTrue(TEXT("Dialogue service should be registered"), DialogueService.IsValid());
	if (!DialogueService.IsValid())
	{
		return false;
	}

	TSharedPtr<IInteractionService> InteractionService = FProjectServiceLocator::Resolve<IInteractionService>();
	TestTrue(TEXT("Interaction service should be registered"), InteractionService.IsValid());
	if (!InteractionService.IsValid())
	{
		return false;
	}

	if (DialogueService->IsDialogueActive())
	{
		DialogueService->EndDialogue();
	}

	const FPrimaryAssetId GrandpaId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("GrandPa")));
	FText SpawnError;
	AActor* GrandpaActor = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		GrandpaId,
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("GrandPa should spawn"), GrandpaActor);
	if (!GrandpaActor)
	{
		AddError(FString::Printf(TEXT("GrandPa spawn failed: %s"), *SpawnError.ToString()));
		return false;
	}

	UProjectDialogueComponent* DialogueComp = GrandpaActor->FindComponentByClass<UProjectDialogueComponent>();
	TestNotNull(TEXT("GrandPa should have dialogue capability"), DialogueComp);

	APawn* InstigatorPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Instigator pawn should spawn"), InstigatorPawn);

	if (DialogueComp && InstigatorPawn)
	{
		const bool bPassThrough = IInteractableComponentTargetInterface::Execute_OnComponentInteract(DialogueComp, InstigatorPawn);
		TestFalse(TEXT("Dialogue interaction should block chain while conversation is active"), bPassThrough);
		TestTrue(TEXT("Dialogue should be active immediately after interaction"), DialogueService->IsDialogueActive());

		// Reproduce interaction service rebroadcast within the same frame as interaction start.
		InteractionService->OnInteraction().Broadcast(GrandpaActor, InstigatorPawn);
		TestTrue(
			TEXT("Same-frame interaction rebroadcast must not close newly started dialogue"),
			DialogueService->IsDialogueActive());
	}

	if (DialogueService->IsDialogueActive())
	{
		DialogueService->EndDialogue();
	}

	if (InstigatorPawn)
	{
		InstigatorPawn->Destroy();
	}
	GrandpaActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectParentGeneralization_DoorGrandPaAssetDefinitionIdTest,
	"ProjectIntegrationTests.ObjectParentGeneralization.Pilot.DoorGrandPaAssetDefinitionId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FObjectParentGeneralization_DoorGrandPaAssetDefinitionIdTest::RunTest(const FString& Parameters)
{
	UObjectDefinition* DoorAsset = LoadObject<UObjectDefinition>(
		nullptr,
		TEXT("/ProjectObject/HumanMade/Building/Fenestration/Door/Scenario/Door_GrandPa.Door_GrandPa"));
	TestNotNull(TEXT("Door_GrandPa asset should load"), DoorAsset);
	if (!DoorAsset)
	{
		return false;
	}

	TestEqual(TEXT("Door_GrandPa asset ObjectId should match JSON id"), DoorAsset->ObjectId.ToString(), FString(TEXT("Door_GrandPa")));
	const FPrimaryAssetId DefinitionId = DoorAsset->GetPrimaryAssetId();
	TestTrue(TEXT("Door_GrandPa asset should expose valid primary asset id"), DefinitionId.IsValid());
	if (DefinitionId.IsValid())
	{
		TestEqual(TEXT("Door_GrandPa primary asset id should match expected format"), DefinitionId.ToString(), FString(TEXT("ObjectDefinition:Door_GrandPa")));
	}

	UWorld* World = OPG_ResolveAutomationTestWorld();
	TestNotNull(TEXT("Automation world should be available"), World);
	if (!World)
	{
		return false;
	}

	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		DoorAsset,
		FTransform::Identity,
		FActorSpawnParameters(),
		&SpawnError);
	TestNotNull(TEXT("Door_GrandPa asset should spawn"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn failed: %s"), *SpawnError.ToString()));
		return false;
	}

	FPrimaryAssetId HostedDefinitionId;
	FString HostedStructureHash;
	FString HostedContentHash;
	const bool bHasHostState = ProjectWorldDefinitionHost::ReadHostState(
		Spawned, HostedDefinitionId, HostedStructureHash, HostedContentHash);
	TestTrue(TEXT("Spawned Door_GrandPa asset actor should expose host state"), bHasHostState);
	if (bHasHostState)
	{
		TestEqual(TEXT("Spawned host definition id should match asset definition id"), HostedDefinitionId.ToString(), DefinitionId.ToString());
	}

	Spawned->Destroy();
	return true;
}
