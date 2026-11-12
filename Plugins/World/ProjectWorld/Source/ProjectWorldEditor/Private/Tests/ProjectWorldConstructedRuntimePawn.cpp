// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldConstructedRuntimePawn.h"
#include "ObjectDefinitionHostHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	bool ConfiguredPawnRequested()
	{
		return !FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_GAME_MODE")).IsEmpty() ||
			!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_PAWN_CLASS")).IsEmpty() ||
			!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_PAWN_DEFINITION")).IsEmpty();
	}
	bool ReadPawnSelection(FAutomationTestBase& Test,FPawnSelection& Out)
	{
		Out=FPawnSelection();
		const FString Mode=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_GAME_MODE"));
		const FString Pawn=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_PAWN_CLASS"));
		const FString Definition=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_PAWN_DEFINITION"));
		if (Mode.IsEmpty() && Pawn.IsEmpty() && Definition.IsEmpty())
		{ return true; }
		FPawnSelection Candidate;
		Candidate.GameModeClass=Mode.IsEmpty() ? nullptr : LoadClass<AGameModeBase>(nullptr,*Mode);
		Candidate.PawnClass=Pawn.IsEmpty() ? nullptr : LoadClass<ACharacter>(nullptr,*Pawn);
		Candidate.Definition=FPrimaryAssetId::FromString(Definition);
		if (!Test.TestTrue(TEXT("A configured diagnostic pawn requires known GameMode, expected character class and explicit definition identity."),
			Candidate.GameModeClass && Candidate.PawnClass && Candidate.Definition.IsValid())) return false;
		Out=Candidate; return true;
	}
	bool ConfigurePawn(UWorld* World,const FVector& Start,FAutomationTestBase& Test,FPawnSelection& Out)
	{
		if (!ReadPawnSelection(Test,Out)) return false;
		if (!Out.GameModeClass) {World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();return true;}
		if (World->WorldType==EWorldType::Editor)
		{
			World->GetWorldSettings()->DefaultGameMode=Out.GameModeClass;
			if (!Test.TestNotNull(TEXT("The project GameMode selects an actual PlayerStart on the modeled surface."),
				World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(),Start+FVector(0,0,150),FRotator::ZeroRotator))) return false;
		}
		return true;
	}
	bool SpawnPawn(UWorld* World,const FVector& Start,const FPawnSelection& Selection,FAutomationTestBase& Test,ACharacter*& Out,APlayerController*& Controller)
	{
		Out=nullptr; Controller=nullptr; auto* Mode=World->GetAuthGameMode();
		if (Selection.GameModeClass)
		{
			if (!Test.TestTrue(TEXT("The isolated world initialized the exact configured project GameMode."),Mode && Mode->GetClass()==Selection.GameModeClass)) return false;
			Controller=World->GetFirstPlayerController();
			if (!Test.TestTrue(TEXT("Native PIE supplies the configured controller and actual local player/viewport."),Controller &&
				Controller->GetClass()==Mode->PlayerControllerClass && Controller->GetLocalPlayer() && Controller->GetLocalPlayer()->ViewportClient &&
				Controller->GetLocalPlayer()->ViewportClient->GetWorld()==World && Controller->GetLocalPlayer()->ViewportClient->Viewport)) return false;
			Out=Cast<ACharacter>(Controller->GetPawn());
			if (!Test.TestTrue(TEXT("Normal GameMode restart/feature initialization possesses the expected definition-driven character without fallback."),
				Out && Out->GetClass()==Selection.PawnClass && Out->GetController()==Controller &&
				ProjectWorldDefinitionHost::GetDefinitionId(Out)==Selection.Definition)) return false;
			FPrimaryAssetId Id; FString StructureHash,ContentHash;
			if (!Test.TestTrue(TEXT("The normal spawn retains its applied definition identities."),
				ProjectWorldDefinitionHost::ReadHostState(Out,Id,StructureHash,ContentHash) && !StructureHash.IsEmpty() && !ContentHash.IsEmpty())) return false;
			auto* Definition=UAssetManager::Get().GetPrimaryAssetObject(Id);
			const auto* SourceHash=Definition ? FindFProperty<FStrProperty>(Definition->GetClass(),TEXT("SourceJsonHash")) : nullptr;
			if (!Test.TestTrue(TEXT("The consumed generated definition exposes its authored-source identity."),
				SourceHash && !SourceHash->GetPropertyValue_InContainer(Definition).IsEmpty())) return false;
			Test.AddInfo(FString::Printf(TEXT("ConfiguredPawnDefinition asset=%s authored_source_hash=%s; compare with the source normalization owner before claiming authored-data freshness"),
				*Definition->GetPathName(),*SourceHash->GetPropertyValue_InContainer(Definition)));
			TArray<USkeletalMeshComponent*> Meshes; Out->GetComponents(Meshes); int32 LoadedMeshes=0;
			for (auto* Mesh : Meshes) if (Mesh->IsRegistered() && Mesh->GetSkinnedAsset()) ++LoadedMeshes;
			if (!Test.TestTrue(TEXT("The definition-driven pawn includes realized skeletal components."),LoadedMeshes>0)) return false;
			Out->GetCharacterMovement()->StopMovementImmediately();
			Out->SetActorLocation(Start+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics); Out->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Test.AddInfo(FString::Printf(TEXT("ConfiguredPawn mode=%s controller=%s pawn=%s definition=%s structure=%s content=%s skeletal_meshes=%d radius_cm=%.17g halfheight_cm=%.17g max_walk_cm_s=%.17g; native PIE local-player direct-CMC diagnostic, not menu/input/render acceptance"),
				*Mode->GetClass()->GetPathName(),*Controller->GetClass()->GetPathName(),*Out->GetClass()->GetPathName(),*Id.ToString(),*StructureHash,*ContentHash,LoadedMeshes,
				Out->GetCapsuleComponent()->GetScaledCapsuleRadius(),Out->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Out->GetCharacterMovement()->MaxWalkSpeed));
		}
		else
		{
			Controller=World->SpawnActor<APlayerController>();
			Out=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),Start+FVector(0,0,150),FRotator::ZeroRotator);
			if (!Test.TestNotNull(TEXT("A real player controller exists."),Controller) || !Test.TestNotNull(TEXT("A real walking character exists."),Out)) return false;
			Controller->SetAsLocalPlayerController(); Controller->bAutoManageActiveCameraTarget=false;
			Out->GetCapsuleComponent()->SetCapsuleSize(34,88); Controller->Possess(Out); Out->GetCharacterMovement()->MaxWalkSpeed=220;
		}
		return true;
	}
}
#endif
