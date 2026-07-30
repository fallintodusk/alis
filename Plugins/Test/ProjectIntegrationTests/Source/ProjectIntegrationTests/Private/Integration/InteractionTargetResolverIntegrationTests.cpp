// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Misc/ScopeExit.h"

#include "InteractionComponent.h"
#include "Support/ObjectParentGeneralizationTestDoubles.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace InteractionTargetResolverTests
{
	UWorld* ResolveWorld(FAutomationTestBase* Test)
	{
		if (UWorld* World = AutomationCommon::GetAnyGameWorld())
		{
			return World;
		}

		if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
		{
			if (Test)
			{
				Test->AddError(TEXT("Failed to open MainMenu_Persistent for interaction targeting tests"));
			}
			return nullptr;
		}

		return AutomationCommon::GetAnyGameWorld();
	}

	APawn* SpawnPawn(UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		APawn* Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Pawn)
		{
			return nullptr;
		}

		USceneComponent* Root = NewObject<USceneComponent>(Pawn, TEXT("InteractionResolverTestRoot"));
		Pawn->SetRootComponent(Root);
		Pawn->AddInstanceComponent(Root);
		Root->RegisterComponent();
		Pawn->SetActorLocation(Location);
		return Pawn;
	}

	struct FSpawnedInteractable
	{
		AActor* Actor = nullptr;
		UBoxComponent* Box = nullptr;
		UProjectInteractionCounterCapabilityComponent* Capability = nullptr;
	};

	FSpawnedInteractable SpawnInteractable(
		UWorld* World,
		const TCHAR* Name,
		const FVector& Location,
		const FVector& Extent,
		int32 Priority = 0)
	{
		FSpawnedInteractable Result;
		if (!World)
		{
			return Result;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Result.Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Result.Actor)
		{
			return Result;
		}

		Result.Box = NewObject<UBoxComponent>(Result.Actor, FName(*FString::Printf(TEXT("%s_Box"), Name)));
		Result.Actor->SetRootComponent(Result.Box);
		Result.Actor->AddInstanceComponent(Result.Box);
		Result.Box->SetBoxExtent(Extent);
		Result.Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Result.Box->SetCollisionObjectType(ECC_WorldDynamic);
		Result.Box->SetCollisionResponseToAllChannels(ECR_Block);
		Result.Box->RegisterComponent();
		Result.Actor->SetActorLocation(Location);

		Result.Capability = NewObject<UProjectInteractionCounterCapabilityComponent>(
			Result.Actor,
			FName(*FString::Printf(TEXT("%s_Capability"), Name)));
		Result.Actor->AddInstanceComponent(Result.Capability);
		Result.Capability->InteractPriority = Priority;
		Result.Capability->InteractionLabel = FText::FromString(Name);
		Result.Capability->RegisterComponent();
		IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(Result.Capability, Result.Box);
		return Result;
	}

	// Variant of SpawnInteractable that uses UProjectLooseBoundsBoxComponent so the
	// candidate's render/query bounds AABB is larger than its physics collision
	// shape. Drives the resolver's Bounds bucket without affecting the Collision
	// bucket - lets tests reproduce the backpack regression in isolation.
	FSpawnedInteractable SpawnLooseBoundsInteractable(
		UWorld* World,
		const TCHAR* Name,
		const FVector& Location,
		const FVector& CollisionExtent,
		const FVector& ExtraBoundsExtent,
		int32 Priority = 0)
	{
		FSpawnedInteractable Result;
		if (!World)
		{
			return Result;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Result.Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Result.Actor)
		{
			return Result;
		}

		UProjectLooseBoundsBoxComponent* LooseBox = NewObject<UProjectLooseBoundsBoxComponent>(
			Result.Actor,
			FName(*FString::Printf(TEXT("%s_LooseBox"), Name)));
		Result.Actor->SetRootComponent(LooseBox);
		Result.Actor->AddInstanceComponent(LooseBox);
		LooseBox->SetBoxExtent(CollisionExtent);
		LooseBox->ExtraBoundsExtent = ExtraBoundsExtent;
		LooseBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		LooseBox->SetCollisionObjectType(ECC_WorldDynamic);
		LooseBox->SetCollisionResponseToAllChannels(ECR_Block);
		LooseBox->RegisterComponent();
		Result.Actor->SetActorLocation(Location);
		Result.Box = LooseBox;

		Result.Capability = NewObject<UProjectInteractionCounterCapabilityComponent>(
			Result.Actor,
			FName(*FString::Printf(TEXT("%s_Capability"), Name)));
		Result.Actor->AddInstanceComponent(Result.Capability);
		Result.Capability->InteractPriority = Priority;
		Result.Capability->InteractionLabel = FText::FromString(Name);
		Result.Capability->RegisterComponent();
		IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(Result.Capability, Result.Box);
		return Result;
	}

	AActor* SpawnVisibilityBlocker(UWorld* World, const TCHAR* Name, const FVector& Location, const FVector& Extent)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Blocker = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Blocker)
		{
			return nullptr;
		}

		UBoxComponent* BlockerBox = NewObject<UBoxComponent>(Blocker, FName(*FString::Printf(TEXT("%s_Box"), Name)));
		Blocker->SetRootComponent(BlockerBox);
		Blocker->AddInstanceComponent(BlockerBox);
		BlockerBox->SetBoxExtent(Extent);
		BlockerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BlockerBox->SetCollisionObjectType(ECC_WorldStatic);
		BlockerBox->SetCollisionResponseToAllChannels(ECR_Block);
		BlockerBox->RegisterComponent();
		Blocker->SetActorLocation(Location);
		return Blocker;
	}

	UInteractionComponent* AttachInteractionComponent(APawn* Pawn)
	{
		if (!Pawn)
		{
			return nullptr;
		}

		UInteractionComponent* Interaction = NewObject<UInteractionComponent>(Pawn, TEXT("InteractionResolverComponent"));
		Pawn->AddInstanceComponent(Interaction);
		Interaction->RegisterComponent();
		Interaction->bDrawDebug = false;
		return Interaction;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_OverlapFindsOffAxisTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.OverlapFindsOffAxisInteractableWithoutDirectLineHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_OverlapFindsOffAxisTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector ViewOrigin(0.0f, 0.0f, 7000.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable Target = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("OffAxisInteractable"),
		ViewOrigin + FVector(150.0f, 80.0f, 0.0f),
		FVector(10.0f, 10.0f, 10.0f));

	ON_SCOPE_EXIT
	{
		if (Target.Actor) { Target.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Off-axis interactable should spawn"), Target.Actor);
	TestNotNull(TEXT("Off-axis interactable box should exist"), Target.Box);
	if (!Pawn || !Interaction || !Target.Actor || !Target.Box)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		FVector::ForwardVector,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should find an off-axis target gathered by overlap"), bFound);
	TestEqual(TEXT("Resolver should return the off-axis interactable actor"), ResolvedActor, Target.Actor);
	TestEqual(TEXT("Resolver should keep the overlapping primitive component"), ResolvedComponent, static_cast<UPrimitiveComponent*>(Target.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_WorldStaticInteractableCandidateCanResolveTest,
	"ProjectIntegrationTests.Interaction.Targeting.WorldStaticInteractableCandidateCanResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_WorldStaticInteractableCandidateCanResolveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector ViewOrigin(0.0f, 0.0f, 7200.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable Target = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("WorldStaticInteractable"),
		ViewOrigin + FVector(150.0f, 0.0f, 0.0f),
		FVector(12.0f, 40.0f, 80.0f));

	if (Target.Box)
	{
		Target.Box->SetCollisionObjectType(ECC_WorldStatic);
	}

	ON_SCOPE_EXIT
	{
		if (Target.Actor) { Target.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("WorldStatic interactable should spawn"), Target.Actor);
	TestNotNull(TEXT("WorldStatic interactable box should exist"), Target.Box);
	if (!Pawn || !Interaction || !Target.Actor || !Target.Box)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		FVector::ForwardVector,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should gather interactables using WorldStatic object type"), bFound);
	TestEqual(TEXT("Resolver should return the WorldStatic interactable actor"), ResolvedActor, Target.Actor);
	TestEqual(TEXT("Resolver should keep the WorldStatic primitive component"), ResolvedComponent, static_cast<UPrimitiveComponent*>(Target.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_RayHitOnLargeMeshPassesAimGateTest,
	"ProjectIntegrationTests.Interaction.Targeting.RayHitOnLargeMeshPassesAimGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_RayHitOnLargeMeshPassesAimGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector ViewOrigin(0.0f, 0.0f, 7600.0f);
	const FVector ViewForward = FVector(1.0f, 0.0f, -1.0f).GetSafeNormal();
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable Target = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("LargeDoorLikeInteractable"),
		ViewOrigin + FVector(80.0f, 0.0f, -70.0f),
		FVector(10.0f, 80.0f, 120.0f));

	ON_SCOPE_EXIT
	{
		if (Target.Actor) { Target.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Large interactable should spawn"), Target.Actor);
	TestNotNull(TEXT("Large interactable box should exist"), Target.Box);
	if (!Pawn || !Interaction || !Target.Actor || !Target.Box)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		ViewForward,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should accept a large mesh when the view ray hits it"), bFound);
	TestEqual(TEXT("Resolver should return the large interactable actor"), ResolvedActor, Target.Actor);
	TestEqual(TEXT("Resolver should keep the hit primitive component"), ResolvedComponent, static_cast<UPrimitiveComponent*>(Target.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_RayPiercedSmallTargetBeatsOffAxisHighPriorityTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.RayPiercedSmallTargetBeatsOffAxisHighPriorityTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_RayPiercedSmallTargetBeatsOffAxisHighPriorityTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector ViewOrigin(0.0f, 0.0f, 7600.0f);
	const FVector ViewForward = FVector::ForwardVector;
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);

	// Bottle-like: small, centered on the ray, low priority. Ray pierces this box.
	InteractionTargetResolverTests::FSpawnedInteractable Small = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("BottleLikeSmallCenteredInteractable"),
		ViewOrigin + FVector(150.0f, 0.0f, 0.0f),
		FVector(5.0f, 5.0f, 10.0f),
		0);

	// Door-like: large, off-axis (still inside the aim cone), high priority. Ray does NOT pierce this box.
	InteractionTargetResolverTests::FSpawnedInteractable Large = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("DoorLikeLargeOffAxisInteractable"),
		ViewOrigin + FVector(150.0f, 100.0f, 0.0f),
		FVector(10.0f, 30.0f, 120.0f),
		100);

	ON_SCOPE_EXIT
	{
		if (Small.Actor) { Small.Actor->Destroy(); }
		if (Large.Actor) { Large.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Small centered interactable should spawn"), Small.Actor);
	TestNotNull(TEXT("Large off-axis interactable should spawn"), Large.Actor);
	if (!Pawn || !Interaction || !Small.Actor || !Large.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		ViewForward,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should pick a winner when both candidates are inside the cone"), bFound);
	TestEqual(
		TEXT("Ray-pierced small target must beat a high-priority off-axis neighbor"),
		ResolvedActor,
		Small.Actor);
	TestEqual(
		TEXT("Resolver should keep the small primitive component"),
		ResolvedComponent,
		static_cast<UPrimitiveComponent*>(Small.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_RayPiercedFrontTargetBeatsRayPiercedBackTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.RayPiercedFrontTargetBeatsRayPiercedBackTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_RayPiercedFrontTargetBeatsRayPiercedBackTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Two interactables co-linear with the view ray, front and back. Both are
	// view-ray-hit candidates. Comparator must pick the front one because
	// ViewRayHitDistance breaks the tie. The back's priority must NOT outweigh
	// front-most intent. This is the windows-screenshot regression.
	//
	// LOS leak guard: SpawnInteractable blocks all channels by default, which would
	// make the front box reject the back candidate via the LOS gate before the
	// comparator ever runs - producing a false positive. Set the front box to
	// ignore the visibility channel so the LOS trace from view origin to the back
	// candidate's TargetPoint is unobstructed and BOTH candidates survive to
	// IsBetterCandidate. Only then does the comparator legitimately decide.
	const FVector ViewOrigin(0.0f, 0.0f, 7800.0f);
	const FVector ViewForward = FVector::ForwardVector;
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);

	InteractionTargetResolverTests::FSpawnedInteractable Front = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("FrontWindowLikeInteractable"),
		ViewOrigin + FVector(120.0f, 0.0f, 0.0f),
		FVector(8.0f, 80.0f, 80.0f),
		0);

	if (Front.Box)
	{
		// Window-glass-like: still queries (overlap/LineTraceComponent see it) but
		// does not block world LOS traces. This matches the real "window does not
		// block visibility" case in PIE.
		Front.Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}

	InteractionTargetResolverTests::FSpawnedInteractable Back = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("BackWindowLikeInteractable"),
		ViewOrigin + FVector(180.0f, 0.0f, 0.0f),
		FVector(8.0f, 80.0f, 80.0f),
		100);

	ON_SCOPE_EXIT
	{
		if (Back.Actor) { Back.Actor->Destroy(); }
		if (Front.Actor) { Front.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Front interactable should spawn"), Front.Actor);
	TestNotNull(TEXT("Back interactable should spawn"), Back.Actor);
	if (!Pawn || !Interaction || !Front.Actor || !Back.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		ViewForward,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should pick a winner with two pierced candidates"), bFound);
	TestEqual(
		TEXT("Front-most ray-pierced candidate must beat a higher-priority pierced candidate behind it"),
		ResolvedActor,
		Front.Actor);
	TestEqual(
		TEXT("Resolver should keep the front primitive component"),
		ResolvedComponent,
		static_cast<UPrimitiveComponent*>(Front.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_ConeFallbackMostCenteredWinsWhenRayMissesTest,
	"ProjectIntegrationTests.Interaction.Targeting.ConeFallbackMostCenteredWinsWhenRayMisses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_ConeFallbackMostCenteredWinsWhenRayMissesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Two non-pierced candidates inside the cone, on opposite sides of the ray so
	// LOS is clear for both. The candidate closer to the ray (higher AimDot) must win.
	const FVector ViewOrigin(0.0f, 0.0f, 8000.0f);
	const FVector ViewForward = FVector::ForwardVector;
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);

	InteractionTargetResolverTests::FSpawnedInteractable MoreCentered = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("ConeFallbackMoreCentered"),
		ViewOrigin + FVector(150.0f, 18.0f, 0.0f),
		FVector(6.0f, 6.0f, 6.0f),
		0);

	InteractionTargetResolverTests::FSpawnedInteractable LessCentered = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("ConeFallbackLessCentered"),
		ViewOrigin + FVector(150.0f, -35.0f, 0.0f),
		FVector(6.0f, 6.0f, 6.0f),
		0);

	ON_SCOPE_EXIT
	{
		if (LessCentered.Actor) { LessCentered.Actor->Destroy(); }
		if (MoreCentered.Actor) { MoreCentered.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("More-centered interactable should spawn"), MoreCentered.Actor);
	TestNotNull(TEXT("Less-centered interactable should spawn"), LessCentered.Actor);
	if (!Pawn || !Interaction || !MoreCentered.Actor || !LessCentered.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		ViewForward,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should pick a winner with two non-pierced cone candidates"), bFound);
	TestEqual(
		TEXT("Cone fallback: higher AimDot (more centered) candidate must win"),
		ResolvedActor,
		MoreCentered.Actor);
	TestEqual(
		TEXT("Resolver should keep the more-centered primitive component"),
		ResolvedComponent,
		static_cast<UPrimitiveComponent*>(MoreCentered.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_CollisionHitBeatsCloserBoundsHitTest,
	"ProjectIntegrationTests.Interaction.Targeting.CollisionHitBeatsCloserBoundsHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_CollisionHitBeatsCloserBoundsHitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Reproduces the backpack regression: a candidate whose AABB clips the view ray
	// (large rendered bounds, thin collision) gets a "Bounds" hit at a closer
	// distance than the actually-aimed-at candidate's "Collision" hit. Old
	// comparator collapsed both into one bucket and picked the closer Bounds hit.
	// The 3-bucket comparator must order Collision > Bounds even when Bounds is
	// closer.
	const FVector ViewOrigin(0.0f, 0.0f, 8400.0f);
	const FVector ViewForward = FVector::ForwardVector;
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);

	// LooseBoundsCandidate: small physics box raised 60cm above the ray (collision
	// misses the ray) but with 100cm extra bounds extent so the AABB clips the ray
	// at a near distance. This is the "backpack" - rendered volume under the
	// crosshair, but the actual physics geometry is not under the crosshair.
	InteractionTargetResolverTests::FSpawnedInteractable LooseBoundsCandidate =
		InteractionTargetResolverTests::SpawnLooseBoundsInteractable(
			World,
			TEXT("CollisionHit_BackpackLikeLooseBounds"),
			ViewOrigin + FVector(140.0f, 0.0f, 60.0f),
			FVector(5.0f, 5.0f, 5.0f),
			FVector(100.0f, 100.0f, 100.0f),
			0);

	// CollisionCandidate: small box centered on the ray, further out. The ray's
	// physics line trace pierces it. ViewRayHitDistance is larger than the
	// LooseBoundsCandidate's, so the only way for this to win is the bucket order.
	InteractionTargetResolverTests::FSpawnedInteractable CollisionCandidate =
		InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("CollisionHit_MetalPieceOnRay"),
			ViewOrigin + FVector(180.0f, 0.0f, 0.0f),
			FVector(5.0f, 5.0f, 5.0f),
			0);

	ON_SCOPE_EXIT
	{
		if (CollisionCandidate.Actor) { CollisionCandidate.Actor->Destroy(); }
		if (LooseBoundsCandidate.Actor) { LooseBoundsCandidate.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Loose-bounds candidate should spawn"), LooseBoundsCandidate.Actor);
	TestNotNull(TEXT("Collision-hit candidate should spawn"), CollisionCandidate.Actor);
	if (!Pawn || !Interaction || !LooseBoundsCandidate.Actor || !CollisionCandidate.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		ViewForward,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should pick a winner with a Bounds candidate vs a Collision candidate"), bFound);
	TestEqual(
		TEXT("Real ray collision must beat a closer bounds-only AABB clip (backpack regression)"),
		ResolvedActor,
		CollisionCandidate.Actor);
	TestEqual(
		TEXT("Resolver should keep the collision-hit primitive component"),
		ResolvedComponent,
		static_cast<UPrimitiveComponent*>(CollisionCandidate.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_BoundsHitFrontTargetBeatsFarFallbackTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.BoundsHitFrontTargetBeatsFarFallbackTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_BoundsHitFrontTargetBeatsFarFallbackTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Verifies the second bucket boundary: a Bounds-only ray hit must beat a
	// fallback (None / closest-point) candidate even when the fallback is more
	// centered or closer. Prevents a regression where shrinking Bounds back into
	// the fallback bucket would lose window-glass-style centeredness signals.
	const FVector ViewOrigin(0.0f, 0.0f, 8800.0f);
	const FVector ViewForward = FVector::ForwardVector;
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);

	// BoundsCandidate: physics shape 60cm above the ray (collision misses), but
	// AABB extends 100cm so the bounds clips the ray. Bounds hit, no Collision.
	InteractionTargetResolverTests::FSpawnedInteractable BoundsCandidate =
		InteractionTargetResolverTests::SpawnLooseBoundsInteractable(
			World,
			TEXT("BoundsHit_WindowGlassLikeLooseBounds"),
			ViewOrigin + FVector(140.0f, 0.0f, 60.0f),
			FVector(5.0f, 5.0f, 5.0f),
			FVector(100.0f, 100.0f, 100.0f),
			0);

	// FallbackCandidate: off-axis enough that neither collision nor bounds clip
	// the ray, but still inside the cone (AimDot > 0.85). Resolver falls through
	// to closest-point on collision; HitKind = None.
	InteractionTargetResolverTests::FSpawnedInteractable FallbackCandidate =
		InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("Fallback_OffAxisInsideCone"),
			ViewOrigin + FVector(160.0f, 80.0f, 0.0f),
			FVector(5.0f, 5.0f, 5.0f),
			0);

	ON_SCOPE_EXIT
	{
		if (FallbackCandidate.Actor) { FallbackCandidate.Actor->Destroy(); }
		if (BoundsCandidate.Actor) { BoundsCandidate.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Bounds candidate should spawn"), BoundsCandidate.Actor);
	TestNotNull(TEXT("Fallback candidate should spawn"), FallbackCandidate.Actor);
	if (!Pawn || !Interaction || !BoundsCandidate.Actor || !FallbackCandidate.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		ViewForward,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should pick a winner with a Bounds candidate vs a Fallback candidate"), bFound);
	TestEqual(
		TEXT("Bounds-only ray hit must beat a closest-point fallback candidate"),
		ResolvedActor,
		BoundsCandidate.Actor);
	TestEqual(
		TEXT("Resolver should keep the bounds-hit primitive component"),
		ResolvedComponent,
		static_cast<UPrimitiveComponent*>(BoundsCandidate.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_VisibleCandidateBeatsOccludedCenteredCandidateTest,
	"ProjectIntegrationTests.Interaction.Targeting.VisibleCandidateBeatsOccludedCenteredCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_VisibleCandidateBeatsOccludedCenteredCandidateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector ViewOrigin(0.0f, 0.0f, 7400.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable CenterTarget = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("CenteredOccludedTarget"),
		ViewOrigin + FVector(190.0f, 0.0f, 0.0f),
		FVector(14.0f, 14.0f, 14.0f));
	InteractionTargetResolverTests::FSpawnedInteractable VisibleTarget = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("VisibleSideTarget"),
		ViewOrigin + FVector(170.0f, 95.0f, 0.0f),
		FVector(14.0f, 14.0f, 14.0f));
	AActor* Blocker = InteractionTargetResolverTests::SpawnVisibilityBlocker(
		World,
		TEXT("CenteredTargetBlocker"),
		ViewOrigin + FVector(95.0f, 0.0f, 0.0f),
		FVector(18.0f, 30.0f, 30.0f));

	ON_SCOPE_EXIT
	{
		if (Blocker) { Blocker->Destroy(); }
		if (VisibleTarget.Actor) { VisibleTarget.Actor->Destroy(); }
		if (CenterTarget.Actor) { CenterTarget.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Centered interactable should spawn"), CenterTarget.Actor);
	TestNotNull(TEXT("Visible side interactable should spawn"), VisibleTarget.Actor);
	TestNotNull(TEXT("Visibility blocker should spawn"), Blocker);
	if (!Pawn || !Interaction || !CenterTarget.Actor || !VisibleTarget.Actor || !Blocker)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		FVector::ForwardVector,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should still find a visible candidate when the centered one is blocked"), bFound);
	TestEqual(TEXT("LOS gate should reject the centered occluded candidate"), ResolvedActor, VisibleTarget.Actor);
	TestEqual(TEXT("Resolver should preserve the visible target component"), ResolvedComponent, static_cast<UPrimitiveComponent*>(VisibleTarget.Box));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_BackCandidateRejectedByAimGateTest,
	"ProjectIntegrationTests.Interaction.Targeting.BackCandidateRejectedByAimGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_BackCandidateRejectedByAimGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector ViewOrigin(0.0f, 0.0f, 7800.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable BehindTarget = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("BehindAimRejectedTarget"),
		ViewOrigin - FVector(90.0f, 0.0f, 0.0f),
		FVector(10.0f, 10.0f, 10.0f));

	ON_SCOPE_EXIT
	{
		if (BehindTarget.Actor) { BehindTarget.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Behind interactable should spawn"), BehindTarget.Actor);
	if (!Pawn || !Interaction || !BehindTarget.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		FVector::ForwardVector,
		ResolvedActor,
		ResolvedComponent);

	TestFalse(TEXT("Resolver should reject a candidate behind the view direction"), bFound);
	TestNull(TEXT("No actor should resolve after aim-gate rejection"), ResolvedActor);
	TestNull(TEXT("No component should resolve after aim-gate rejection"), ResolvedComponent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_PeripheralCandidateRejectedByAimGateTest,
	"ProjectIntegrationTests.Interaction.Targeting.PeripheralCandidateRejectedByAimGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_PeripheralCandidateRejectedByAimGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector ViewOrigin(0.0f, 0.0f, 8000.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable PeripheralTarget = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("PeripheralAimRejectedTarget"),
		ViewOrigin + FVector(120.0f, 180.0f, 0.0f),
		FVector(8.0f, 8.0f, 8.0f));

	ON_SCOPE_EXIT
	{
		if (PeripheralTarget.Actor) { PeripheralTarget.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Peripheral interactable should spawn"), PeripheralTarget.Actor);
	if (!Pawn || !Interaction || !PeripheralTarget.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		ViewOrigin,
		FVector::ForwardVector,
		ResolvedActor,
		ResolvedComponent);

	TestFalse(TEXT("Resolver should reject a peripheral target inside the overlap sphere"), bFound);
	TestNull(TEXT("No actor should resolve after peripheral aim-gate rejection"), ResolvedActor);
	TestNull(TEXT("No component should resolve after peripheral aim-gate rejection"), ResolvedComponent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_ShortCircuitBypassesLosButNotAimTest,
	"ProjectIntegrationTests.Interaction.Targeting.ShortCircuitBypassesLosButNotAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_ShortCircuitBypassesLosButNotAimTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FVector VisibleByShortCircuitOrigin(0.0f, 0.0f, 8200.0f);
	const FVector RejectedByAimOrigin(0.0f, 0.0f, 8800.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, VisibleByShortCircuitOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable NearFrontTarget = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("NearFrontShortCircuitTarget"),
		VisibleByShortCircuitOrigin + FVector(45.0f, 0.0f, 0.0f),
		FVector(6.0f, 6.0f, 6.0f));
	AActor* FrontBlocker = InteractionTargetResolverTests::SpawnVisibilityBlocker(
		World,
		TEXT("NearFrontShortCircuitBlocker"),
		VisibleByShortCircuitOrigin + FVector(22.0f, 0.0f, 0.0f),
		FVector(5.0f, 20.0f, 20.0f));
	InteractionTargetResolverTests::FSpawnedInteractable NearBehindTarget = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("NearBehindShortCircuitTarget"),
		RejectedByAimOrigin - FVector(45.0f, 0.0f, 0.0f),
		FVector(6.0f, 6.0f, 6.0f));

	ON_SCOPE_EXIT
	{
		if (NearBehindTarget.Actor) { NearBehindTarget.Actor->Destroy(); }
		if (FrontBlocker) { FrontBlocker->Destroy(); }
		if (NearFrontTarget.Actor) { NearFrontTarget.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Near front interactable should spawn"), NearFrontTarget.Actor);
	TestNotNull(TEXT("Near front blocker should spawn"), FrontBlocker);
	TestNotNull(TEXT("Near behind interactable should spawn"), NearBehindTarget.Actor);
	if (!Pawn || !Interaction || !NearFrontTarget.Actor || !FrontBlocker || !NearBehindTarget.Actor)
	{
		return false;
	}

	AActor* ResolvedActor = nullptr;
	UPrimitiveComponent* ResolvedComponent = nullptr;
	bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		VisibleByShortCircuitOrigin,
		FVector::ForwardVector,
		ResolvedActor,
		ResolvedComponent);

	TestTrue(TEXT("Resolver should short-circuit LOS for a directly touched forward target"), bFound);
	TestEqual(TEXT("Short-circuit should keep the forward target despite a blocker"), ResolvedActor, NearFrontTarget.Actor);
	TestEqual(TEXT("Short-circuit should preserve the forward target component"), ResolvedComponent, static_cast<UPrimitiveComponent*>(NearFrontTarget.Box));

	ResolvedActor = nullptr;
	ResolvedComponent = nullptr;
	bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
		RejectedByAimOrigin,
		FVector::ForwardVector,
		ResolvedActor,
		ResolvedComponent);

	TestFalse(TEXT("Short-circuit should not bypass the aim gate"), bFound);
	TestNull(TEXT("No actor should resolve when the near target is behind the view"), ResolvedActor);
	TestNull(TEXT("No component should resolve when the near target is behind the view"), ResolvedComponent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_PriorityBreaksOnlyAfterCenterednessAndDistanceTieTest,
	"ProjectIntegrationTests.Interaction.Targeting.PriorityBreaksOnlyAfterCenterednessAndDistanceTie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_PriorityBreaksOnlyAfterCenterednessAndDistanceTieTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Case A: low-priority centered (ray-pierced) candidate must beat high-priority off-axis candidate.
	{
		const FVector ViewOrigin(0.0f, 0.0f, 9200.0f);
		APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
		UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
		InteractionTargetResolverTests::FSpawnedInteractable CenterLowPriority = InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("CaseA_CenterLowPriority"),
			ViewOrigin + FVector(160.0f, 0.0f, 0.0f),
			FVector(8.0f, 8.0f, 8.0f),
			0);
		InteractionTargetResolverTests::FSpawnedInteractable SideHighPriority = InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("CaseA_SideHighPriority"),
			ViewOrigin + FVector(160.0f, 45.0f, 0.0f),
			FVector(8.0f, 8.0f, 8.0f),
			100);

		ON_SCOPE_EXIT
		{
			if (SideHighPriority.Actor) { SideHighPriority.Actor->Destroy(); }
			if (CenterLowPriority.Actor) { CenterLowPriority.Actor->Destroy(); }
			if (Interaction) { Interaction->DestroyComponent(); }
			if (Pawn) { Pawn->Destroy(); }
		};

		TestNotNull(TEXT("Case A pawn should spawn"), Pawn);
		TestNotNull(TEXT("Case A interaction component should attach"), Interaction);
		TestNotNull(TEXT("Case A centered low-priority interactable should spawn"), CenterLowPriority.Actor);
		TestNotNull(TEXT("Case A side high-priority interactable should spawn"), SideHighPriority.Actor);
		if (!Pawn || !Interaction || !CenterLowPriority.Actor || !SideHighPriority.Actor)
		{
			return false;
		}

		AActor* ResolvedActor = nullptr;
		UPrimitiveComponent* ResolvedComponent = nullptr;
		const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
			ViewOrigin,
			FVector::ForwardVector,
			ResolvedActor,
			ResolvedComponent);

		TestTrue(TEXT("Case A resolver should find a candidate"), bFound);
		TestEqual(
			TEXT("Priority must NOT outweigh centeredness: ray-pierced low-priority candidate must win"),
			ResolvedActor,
			CenterLowPriority.Actor);
	}

	// Case B: same bucket + same AimDot + same Distance, only Priority differs - higher priority wins.
	{
		const FVector ViewOrigin(0.0f, 0.0f, 9300.0f);
		APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
		UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
		// Both off-axis at the same Y offset but on opposite sides of the ray -> both non-pierced
		// with equal AimDot and equal Distance. Priority is the only discriminator left.
		InteractionTargetResolverTests::FSpawnedInteractable LowPriority = InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("CaseB_LowPriorityMirror"),
			ViewOrigin + FVector(160.0f, 30.0f, 0.0f),
			FVector(6.0f, 6.0f, 6.0f),
			0);
		InteractionTargetResolverTests::FSpawnedInteractable HighPriority = InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("CaseB_HighPriorityMirror"),
			ViewOrigin + FVector(160.0f, -30.0f, 0.0f),
			FVector(6.0f, 6.0f, 6.0f),
			100);

		ON_SCOPE_EXIT
		{
			if (HighPriority.Actor) { HighPriority.Actor->Destroy(); }
			if (LowPriority.Actor) { LowPriority.Actor->Destroy(); }
			if (Interaction) { Interaction->DestroyComponent(); }
			if (Pawn) { Pawn->Destroy(); }
		};

		TestNotNull(TEXT("Case B pawn should spawn"), Pawn);
		TestNotNull(TEXT("Case B interaction component should attach"), Interaction);
		TestNotNull(TEXT("Case B low-priority mirror should spawn"), LowPriority.Actor);
		TestNotNull(TEXT("Case B high-priority mirror should spawn"), HighPriority.Actor);
		if (!Pawn || !Interaction || !LowPriority.Actor || !HighPriority.Actor)
		{
			return false;
		}

		AActor* ResolvedActor = nullptr;
		UPrimitiveComponent* ResolvedComponent = nullptr;
		const bool bFound = Interaction->TestOnly_ResolveBestInteractionTarget(
			ViewOrigin,
			FVector::ForwardVector,
			ResolvedActor,
			ResolvedComponent);

		TestTrue(TEXT("Case B resolver should find a candidate"), bFound);
		TestEqual(
			TEXT("Priority must break ties when bucket, AimDot, and Distance are equal"),
			ResolvedActor,
			HighPriority.Actor);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_HysteresisKeepsCurrentFocusOnNearTieTest,
	"ProjectIntegrationTests.Interaction.Targeting.HysteresisKeepsCurrentFocusOnNearTie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_HysteresisKeepsCurrentFocusOnNearTieTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Both candidates off-axis enough that the view ray pierces neither (same "non-pierced"
	// bucket). They sit on opposite sides of the ray so neither occludes the other for LOS.
	// NewSlightWinner has a slightly higher AimDot. With hysteresis, incumbent keeps focus;
	// without hysteresis, the slight winner takes focus. Cross-bucket cases are intentionally
	// out of scope for this test - they always switch under the new comparator.
	const FVector ViewOrigin(0.0f, 0.0f, 9600.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable CurrentFocus = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("CurrentNearTieTarget"),
		ViewOrigin + FVector(150.0f, 25.0f, 0.0f),
		FVector(8.0f, 8.0f, 8.0f));
	InteractionTargetResolverTests::FSpawnedInteractable NewSlightWinner = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("NewSlightWinnerTarget"),
		ViewOrigin + FVector(150.0f, -20.0f, 0.0f),
		FVector(8.0f, 8.0f, 8.0f));

	ON_SCOPE_EXIT
	{
		if (NewSlightWinner.Actor) { NewSlightWinner.Actor->Destroy(); }
		if (CurrentFocus.Actor) { CurrentFocus.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Current-focus interactable should spawn"), CurrentFocus.Actor);
	TestNotNull(TEXT("New slight-winner interactable should spawn"), NewSlightWinner.Actor);
	if (!Pawn || !Interaction || !CurrentFocus.Actor || !NewSlightWinner.Actor)
	{
		return false;
	}

	Interaction->TestOnly_SetFocusedActor(CurrentFocus.Actor, CurrentFocus.Box);
	TestEqual(TEXT("Current focus should be primed"), IInteractionComponentInterface::Execute_GetFocusedActor(Interaction), CurrentFocus.Actor);

	const bool bFoundWithHysteresis = Interaction->TestOnly_UpdateFocusFromView(ViewOrigin, FVector::ForwardVector);
	TestTrue(TEXT("Resolver should find candidates while hysteresis is enabled"), bFoundWithHysteresis);
	TestEqual(TEXT("Hysteresis should keep the current focus on a near tie"), IInteractionComponentInterface::Execute_GetFocusedActor(Interaction), CurrentFocus.Actor);

	Interaction->FocusSwitchHysteresis = 0.0f;
	const bool bFoundWithoutHysteresis = Interaction->TestOnly_UpdateFocusFromView(ViewOrigin, FVector::ForwardVector);
	TestTrue(TEXT("Resolver should find candidates after hysteresis is disabled"), bFoundWithoutHysteresis);
	TestEqual(TEXT("Disabling hysteresis should allow the slight raw winner to take focus"), IInteractionComponentInterface::Execute_GetFocusedActor(Interaction), NewSlightWinner.Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_HysteresisUsesAngularConeLeewayTest,
	"ProjectIntegrationTests.Interaction.Targeting.HysteresisUsesAngularConeLeeway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_HysteresisUsesAngularConeLeewayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Reproduces the cigarette-vs-water-bottle PIE bug: incumbent focus at ~8 deg
	// from the crosshair, new winner at ~2.5 deg. Both inside the cone so both
	// land in the None bucket. The angular diff (~5.5 deg) exceeds the leeway at
	// FocusSwitchHysteresis=0.10 (3.18 deg of a 31.8 deg cone), so focus MUST
	// switch despite the broken old AimDot-ratio threshold (~0.8991, which would
	// have kept the incumbent because anything in the cone clears it).
	const FVector ViewOrigin(0.0f, 0.0f, 9700.0f);
	const FVector ViewForward = FVector::ForwardVector;
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);

	// Current focus at angle ~8 deg from forward: 150 cm forward, ~21 cm sideways.
	// AimDot = 150/sqrt(150^2 + 21^2) ~= 0.9904. acos(0.9904) ~= 7.95 deg.
	InteractionTargetResolverTests::FSpawnedInteractable Incumbent =
		InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("AngularLeeway_IncumbentAt8Deg"),
			ViewOrigin + FVector(150.0f, 21.0f, 0.0f),
			FVector(6.0f, 6.0f, 6.0f));

	// New winner at angle ~2.5 deg: 150 cm forward, ~7 cm sideways (opposite side
	// to keep LOS clean for both candidates).
	// AimDot = 150/sqrt(150^2 + 7^2) ~= 0.9989. acos(0.9989) ~= 2.67 deg.
	InteractionTargetResolverTests::FSpawnedInteractable Challenger =
		InteractionTargetResolverTests::SpawnInteractable(
			World,
			TEXT("AngularLeeway_ChallengerAt2pt5Deg"),
			ViewOrigin + FVector(150.0f, -7.0f, 0.0f),
			FVector(6.0f, 6.0f, 6.0f));

	ON_SCOPE_EXIT
	{
		if (Challenger.Actor) { Challenger.Actor->Destroy(); }
		if (Incumbent.Actor) { Incumbent.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Incumbent interactable should spawn"), Incumbent.Actor);
	TestNotNull(TEXT("Challenger interactable should spawn"), Challenger.Actor);
	if (!Pawn || !Interaction || !Incumbent.Actor || !Challenger.Actor)
	{
		return false;
	}

	Interaction->TestOnly_SetFocusedActor(Incumbent.Actor, Incumbent.Box);
	TestEqual(
		TEXT("Incumbent focus should be primed"),
		IInteractionComponentInterface::Execute_GetFocusedActor(Interaction),
		Incumbent.Actor);

	const bool bFound = Interaction->TestOnly_UpdateFocusFromView(ViewOrigin, ViewForward);
	TestTrue(TEXT("Resolver should find candidates"), bFound);
	TestEqual(
		TEXT("Angular hysteresis must let a clearly-aimed candidate take focus"),
		IInteractionComponentInterface::Execute_GetFocusedActor(Interaction),
		Challenger.Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_AuthorityInteractRoutesFocusedTargetToCapabilityTest,
	"ProjectIntegrationTests.Interaction.Targeting.AuthorityInteractRoutesFocusedTargetToCapability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_AuthorityInteractRoutesFocusedTargetToCapabilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// SOT: client's focused (Actor, Component) is what gets interacted. The
	// authority path validates and dispatches without re-resolving from a
	// different view (which is what produced the dresser regression: server
	// re-resolved with eye view, picked a sibling drawer slot or rejected).
	const FVector ViewOrigin(0.0f, 0.0f, 10000.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable Target = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("SOT_AuthorityFocusedTarget"),
		ViewOrigin + FVector(150.0f, 0.0f, 0.0f),
		FVector(8.0f, 8.0f, 8.0f));

	ON_SCOPE_EXIT
	{
		if (Target.Actor) { Target.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Authority target should spawn"), Target.Actor);
	TestNotNull(TEXT("Authority target capability should exist"), Target.Capability);
	if (!Pawn || !Interaction || !Target.Actor || !Target.Capability)
	{
		return false;
	}

	Pawn->SetActorLocation(ViewOrigin);

	// Prime the client-side SOT directly - the authority path reads
	// FocusedActor/FocusedComponent and dispatches that exact target.
	Interaction->TestOnly_SetFocusedActor(Target.Actor, Target.Box);

	TestTrue(
		TEXT("Authority TryInteract should dispatch the focused target"),
		IInteractionComponentInterface::Execute_TryInteract(Interaction));
	TestEqual(
		TEXT("Authority dispatch should call the focused target's capability exactly once"),
		Target.Capability->InteractionCallCount,
		1);
	TestEqual(
		TEXT("Authority dispatch should pass the pawn as instigator"),
		Target.Capability->LastInstigator.Get(),
		static_cast<AActor*>(Pawn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_AuthorityInteractRoutesFocusedComponentToMatchingCapabilityTest,
	"ProjectIntegrationTests.Interaction.Targeting.AuthorityInteractRoutesFocusedComponentToMatchingCapability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_AuthorityInteractRoutesFocusedComponentToMatchingCapabilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Dresser regression in miniature: one actor with two sibling primitives
	// (each tied to its own capability via SetInteractTargetMesh). Whichever
	// primitive the client highlighted - sent through FocusedComponent - must
	// be the one whose capability fires. Under the old model the server
	// re-resolved from a different view, picked the sibling primitive, and
	// invoked the wrong capability.
	const FVector ViewOrigin(0.0f, 0.0f, 10400.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* DresserActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);

	UBoxComponent* SlotA = nullptr;
	UBoxComponent* SlotB = nullptr;
	UProjectInteractionCounterCapabilityComponent* CapabilityA = nullptr;
	UProjectInteractionCounterCapabilityComponent* CapabilityB = nullptr;

	if (DresserActor)
	{
		USceneComponent* Root = NewObject<USceneComponent>(DresserActor, TEXT("DresserRoot"));
		DresserActor->SetRootComponent(Root);
		DresserActor->AddInstanceComponent(Root);
		Root->RegisterComponent();

		SlotA = NewObject<UBoxComponent>(DresserActor, TEXT("SlotA"));
		SlotA->SetupAttachment(Root);
		SlotA->SetRelativeLocation(FVector(150.0f, 0.0f, 10.0f));
		SlotA->SetBoxExtent(FVector(8.0f, 8.0f, 8.0f));
		SlotA->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SlotA->SetCollisionObjectType(ECC_WorldDynamic);
		SlotA->SetCollisionResponseToAllChannels(ECR_Block);
		DresserActor->AddInstanceComponent(SlotA);
		SlotA->RegisterComponent();

		SlotB = NewObject<UBoxComponent>(DresserActor, TEXT("SlotB"));
		SlotB->SetupAttachment(Root);
		SlotB->SetRelativeLocation(FVector(150.0f, 0.0f, -10.0f));
		SlotB->SetBoxExtent(FVector(8.0f, 8.0f, 8.0f));
		SlotB->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SlotB->SetCollisionObjectType(ECC_WorldDynamic);
		SlotB->SetCollisionResponseToAllChannels(ECR_Block);
		DresserActor->AddInstanceComponent(SlotB);
		SlotB->RegisterComponent();

		CapabilityA = NewObject<UProjectInteractionCounterCapabilityComponent>(DresserActor, TEXT("CapabilityA"));
		DresserActor->AddInstanceComponent(CapabilityA);
		CapabilityA->InteractionLabel = FText::FromString(TEXT("Open Slot A"));
		CapabilityA->RegisterComponent();
		IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(CapabilityA, SlotA);

		CapabilityB = NewObject<UProjectInteractionCounterCapabilityComponent>(DresserActor, TEXT("CapabilityB"));
		DresserActor->AddInstanceComponent(CapabilityB);
		CapabilityB->InteractionLabel = FText::FromString(TEXT("Open Slot B"));
		CapabilityB->RegisterComponent();
		IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(CapabilityB, SlotB);
	}

	ON_SCOPE_EXIT
	{
		if (DresserActor) { DresserActor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Dresser actor should spawn"), DresserActor);
	TestNotNull(TEXT("Slot A box should attach"), SlotA);
	TestNotNull(TEXT("Slot B box should attach"), SlotB);
	TestNotNull(TEXT("Capability A should attach"), CapabilityA);
	TestNotNull(TEXT("Capability B should attach"), CapabilityB);
	if (!Pawn || !Interaction || !DresserActor || !SlotA || !SlotB || !CapabilityA || !CapabilityB)
	{
		return false;
	}

	Pawn->SetActorLocation(ViewOrigin);
	// The Dresser spawns at the origin and only its child boxes get relative offsets, so the
	// server range gate (InteractionRadius*1.5) rejects it. Co-locate it with the pawn view origin.
	DresserActor->SetActorLocation(ViewOrigin);

	// Press 1: focus on Slot A. Only Capability A must fire.
	Interaction->TestOnly_SetFocusedActor(DresserActor, SlotA);
	IInteractionComponentInterface::Execute_TryInteract(Interaction);

	TestEqual(
		TEXT("Focused on Slot A: Capability A must fire"),
		CapabilityA->InteractionCallCount,
		1);
	TestEqual(
		TEXT("Focused on Slot A: Capability B must NOT fire (sibling primitive)"),
		CapabilityB->InteractionCallCount,
		0);

	// Press 2: refocus on Slot B. Only Capability B must fire.
	Interaction->TestOnly_SetFocusedActor(DresserActor, SlotB);
	IInteractionComponentInterface::Execute_TryInteract(Interaction);

	TestEqual(
		TEXT("Focused on Slot B: Capability A must NOT fire on the second press"),
		CapabilityA->InteractionCallCount,
		1);
	TestEqual(
		TEXT("Focused on Slot B: Capability B must fire exactly once"),
		CapabilityB->InteractionCallCount,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetResolver_AuthorityInteractRejectsOutOfRangeTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.AuthorityInteractRejectsOutOfRangeTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionTargetResolver_AuthorityInteractRejectsOutOfRangeTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = InteractionTargetResolverTests::ResolveWorld(this);
	TestNotNull(TEXT("Interaction targeting test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	// Anti-cheat: server validation must reject a focused target that the pawn
	// cannot plausibly reach. Range gate is InteractionRadius * 1.5 (default
	// 200 cm * 1.5 = 300 cm); place the target well beyond that.
	const FVector ViewOrigin(0.0f, 0.0f, 10200.0f);
	APawn* Pawn = InteractionTargetResolverTests::SpawnPawn(World, ViewOrigin);
	UInteractionComponent* Interaction = InteractionTargetResolverTests::AttachInteractionComponent(Pawn);
	InteractionTargetResolverTests::FSpawnedInteractable FarTarget = InteractionTargetResolverTests::SpawnInteractable(
		World,
		TEXT("SOT_FarBeyondRangeTarget"),
		ViewOrigin + FVector(800.0f, 0.0f, 0.0f),
		FVector(8.0f, 8.0f, 8.0f));

	ON_SCOPE_EXIT
	{
		if (FarTarget.Actor) { FarTarget.Actor->Destroy(); }
		if (Interaction) { Interaction->DestroyComponent(); }
		if (Pawn) { Pawn->Destroy(); }
	};

	TestNotNull(TEXT("Resolver pawn should spawn"), Pawn);
	TestNotNull(TEXT("Resolver component should attach"), Interaction);
	TestNotNull(TEXT("Far target should spawn"), FarTarget.Actor);
	TestNotNull(TEXT("Far target capability should exist"), FarTarget.Capability);
	if (!Pawn || !Interaction || !FarTarget.Actor || !FarTarget.Capability)
	{
		return false;
	}

	Pawn->SetActorLocation(ViewOrigin);
	Interaction->TestOnly_SetFocusedActor(FarTarget.Actor, FarTarget.Box);

	IInteractionComponentInterface::Execute_TryInteract(Interaction);
	TestEqual(
		TEXT("Server validation must reject a target beyond InteractionRadius * 1.5"),
		FarTarget.Capability->InteractionCallCount,
		0);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_OverlapFindsOffAxisTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.OverlapFindsOffAxisInteractableWithoutDirectLineHit",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_WorldStaticInteractableCandidateCanResolveTest,
	"ProjectIntegrationTests.Interaction.Targeting.WorldStaticInteractableCandidateCanResolve",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_RayHitOnLargeMeshPassesAimGateTest,
	"ProjectIntegrationTests.Interaction.Targeting.RayHitOnLargeMeshPassesAimGate",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_RayPiercedSmallTargetBeatsOffAxisHighPriorityTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.RayPiercedSmallTargetBeatsOffAxisHighPriorityTarget",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_RayPiercedFrontTargetBeatsRayPiercedBackTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.RayPiercedFrontTargetBeatsRayPiercedBackTarget",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_ConeFallbackMostCenteredWinsWhenRayMissesTest,
	"ProjectIntegrationTests.Interaction.Targeting.ConeFallbackMostCenteredWinsWhenRayMisses",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_CollisionHitBeatsCloserBoundsHitTest,
	"ProjectIntegrationTests.Interaction.Targeting.CollisionHitBeatsCloserBoundsHit",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_BoundsHitFrontTargetBeatsFarFallbackTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.BoundsHitFrontTargetBeatsFarFallbackTarget",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_VisibleCandidateBeatsOccludedCenteredCandidateTest,
	"ProjectIntegrationTests.Interaction.Targeting.VisibleCandidateBeatsOccludedCenteredCandidate",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_BackCandidateRejectedByAimGateTest,
	"ProjectIntegrationTests.Interaction.Targeting.BackCandidateRejectedByAimGate",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_PeripheralCandidateRejectedByAimGateTest,
	"ProjectIntegrationTests.Interaction.Targeting.PeripheralCandidateRejectedByAimGate",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_ShortCircuitBypassesLosButNotAimTest,
	"ProjectIntegrationTests.Interaction.Targeting.ShortCircuitBypassesLosButNotAim",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_PriorityBreaksOnlyAfterCenterednessAndDistanceTieTest,
	"ProjectIntegrationTests.Interaction.Targeting.PriorityBreaksOnlyAfterCenterednessAndDistanceTie",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_HysteresisKeepsCurrentFocusOnNearTieTest,
	"ProjectIntegrationTests.Interaction.Targeting.HysteresisKeepsCurrentFocusOnNearTie",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_HysteresisUsesAngularConeLeewayTest,
	"ProjectIntegrationTests.Interaction.Targeting.HysteresisUsesAngularConeLeeway",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_AuthorityInteractRoutesFocusedTargetToCapabilityTest,
	"ProjectIntegrationTests.Interaction.Targeting.AuthorityInteractRoutesFocusedTargetToCapability",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_AuthorityInteractRoutesFocusedComponentToMatchingCapabilityTest,
	"ProjectIntegrationTests.Interaction.Targeting.AuthorityInteractRoutesFocusedComponentToMatchingCapability",
	"[Fast][Integration][Interaction]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInteractionTargetResolver_AuthorityInteractRejectsOutOfRangeTargetTest,
	"ProjectIntegrationTests.Interaction.Targeting.AuthorityInteractRejectsOutOfRangeTarget",
	"[Fast][Integration][Interaction]")

#endif // WITH_DEV_AUTOMATION_TESTS
