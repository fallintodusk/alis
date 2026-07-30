// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "CinematicHideMetadata.generated.h"

/**
 * Per-take metadata attached to ULevelSequence via `FindOrAddMetaData<>`.
 *
 * Carries the editor-world placement actors that the take's Spawnable
 * duplicates "stand in for" -- the bake-time stamper writes a soft ref
 * per recorded original; ACinematicGameMode reads the list at Render-mode
 * BeginPlay and calls SetActorHiddenInGame(true) on each.
 *
 * Why we don't use a Sequencer `bHidden` Possessable track for this:
 * WP external actors (the dresser etc. in ALIS) don't resolve cleanly
 * through Sequencer's binding pipeline at MRQ render. The property
 * track gets stamped fine but evaluates against a null binding, so the
 * editor placement keeps rendering alongside the take's Spawnable
 * duplicate -- the user sees TWO dressers ("double drawer" regression).
 *
 * Direct game-time hide via SetActorHiddenInGame avoids all binding-
 * resolution gymnastics and works regardless of how the actor was
 * placed (WP external, regular placed actor, runtime spawn).
 */
UCLASS()
class PROJECTCINEMATIC_API UCinematicHideMetadata : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Soft refs to editor-world placement actors that should be hidden
	 * while the take plays. Populated by the editor-time stamper. Read
	 * by ACinematicGameMode in Render BeginPlay; ignored in Record.
	 */
	UPROPERTY()
	TArray<FSoftObjectPath> ActorsToHide;
};
