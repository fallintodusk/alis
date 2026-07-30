// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CinematicGameMode.h"

#include "ProjectCinematic.h"          // LogProjectCinematic
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "Components/PrimitiveComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"                                       // TActorIterator
#include "TimerManager.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"

#if WITH_EDITOR
#include "Editor.h"
#include "MoviePipelineQueueSubsystem.h"
#endif

void ACinematicGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][InitGame] BEGIN | map='%s' options='%s' default SessionMode=%s"),
		*MapName, *Options,
		SessionMode == ECinematicSessionMode::Render ? TEXT("Render") : TEXT("Record"));

	Super::InitGame(MapName, Options, ErrorMessage);

	// MRQ auto-detection (editor-only). C++ default SessionMode is Record
	// (suits PIE recording); when Movie Render Queue is actively rendering,
	// force Render so the phantom-pawn-hide in BeginPlay fires correctly.
	//
	// IMPORTANT: query the EDITOR subsystem `UMoviePipelineQueueSubsystem`,
	// not the engine subsystem `UMoviePipelineQueueEngineSubsystem`. The
	// MRQ UI's Render button uses the editor subsystem; the engine
	// subsystem stays null in that flow. Confirmed empirically via
	// Saved/Logs/Alis.log on 2026-05-19 -- the engine subsystem read 0
	// even mid-render. The editor subsystem holds the ActiveExecutor.
	bool bIsMRQRendering = false;
#if WITH_EDITOR
	if (GEditor)
	{
		if (UMoviePipelineQueueSubsystem* MRQ =
				GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>())
		{
			bIsMRQRendering = (MRQ->GetActiveExecutor() != nullptr);
			UE_LOG(LogProjectCinematic, Log,
				TEXT("[CinematicGM][InitGame] MRQ probe | subsystem=ok activeExecutor=%s -> bIsMRQRendering=%d"),
				MRQ->GetActiveExecutor() ? *MRQ->GetActiveExecutor()->GetName() : TEXT("<null>"),
				bIsMRQRendering ? 1 : 0);
		}
		else
		{
			UE_LOG(LogProjectCinematic, Log,
				TEXT("[CinematicGM][InitGame] MRQ probe | UMoviePipelineQueueSubsystem unavailable from GEditor."));
		}
	}
	else
	{
		UE_LOG(LogProjectCinematic, Log,
			TEXT("[CinematicGM][InitGame] MRQ probe skipped | GEditor null (not in editor context)."));
	}
#endif
	if (bIsMRQRendering)
	{
		SessionMode = ECinematicSessionMode::Render;
	}

	// CinematicSession URL option: "Record" or "Render" force the profile
	// explicitly (URL parsing runs after MRQ auto-detection so URL still
	// wins if both are set).
	const FString SessionStr = UGameplayStatics::ParseOption(Options, TEXT("CinematicSession"));
	if (SessionStr.Equals(TEXT("Render"), ESearchCase::IgnoreCase))
	{
		SessionMode = ECinematicSessionMode::Render;
	}
	else if (SessionStr.Equals(TEXT("Record"), ESearchCase::IgnoreCase))
	{
		SessionMode = ECinematicSessionMode::Record;
	}

	// Record-profile defaults. Fire whenever the active profile is Record,
	// regardless of how it was set (UPROPERTY default OR URL override).
	// Each default applies only if the value is still 1.0 (caller hasn't
	// overridden via Blueprint, instance, or URL below).
	if (SessionMode == ECinematicSessionMode::Record)
	{
		if (FMath::IsNearlyEqual(PawnTimeDilation, 1.0f))
		{
			PawnTimeDilation = 0.5f;
		}
	}

	// Optional explicit dilation override: ?CinematicDilation=0.4
	const FString DilationStr = UGameplayStatics::ParseOption(Options, TEXT("CinematicDilation"));
	if (!DilationStr.IsEmpty())
	{
		const float Parsed = FCString::Atof(*DilationStr);
		if (Parsed > 0.f && Parsed <= 1.0f)
		{
			PawnTimeDilation = Parsed;
		}
	}

	// Look-input scale.
	// Record profile defaults to 0.5 if caller hasn't overridden, mirroring
	// the PawnTimeDilation walk-speed reduction. Smoother captured camera
	// transforms = smoother render playback at no post-process cost.
	if (SessionMode == ECinematicSessionMode::Record &&
		FMath::IsNearlyEqual(LookSensitivityScale, 1.0f))
	{
		LookSensitivityScale = 0.5f;
	}
	// Optional explicit look-scale override: ?CinematicLookScale=0.4
	const FString LookScaleStr = UGameplayStatics::ParseOption(Options, TEXT("CinematicLookScale"));
	if (!LookScaleStr.IsEmpty())
	{
		const float Parsed = FCString::Atof(*LookScaleStr);
		if (Parsed > 0.f && Parsed <= 1.0f)
		{
			LookSensitivityScale = Parsed;
		}
	}
	// Look-input modification is exposed via the ILookInputModifier interface
	// implemented on this game mode. The character calls ModifyLook() in its
	// look handler; no CVar or service locator needed.

	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][InitGame] END | session=%s dilation=%.2f look_scale=%.2f"
		     " (mrq_detected=%d, parsed CinematicSession='%s' CinematicDilation='%s' CinematicLookScale='%s') | world='%s' worldType=%d"),
		SessionMode == ECinematicSessionMode::Render ? TEXT("Render") : TEXT("Record"),
		PawnTimeDilation,
		LookSensitivityScale,
		bIsMRQRendering ? 1 : 0,
		*SessionStr,
		*DilationStr,
		*LookScaleStr,
		GetWorld() ? *GetWorld()->GetName() : TEXT("<null>"),
		GetWorld() ? static_cast<int32>(GetWorld()->WorldType) : -1);
}

void ACinematicGameMode::BeginPlay()
{
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][BeginPlay] BEGIN | session=%s world='%s' worldType=%d netMode=%d isGameWorld=%d"),
		SessionMode == ECinematicSessionMode::Render ? TEXT("Render") : TEXT("Record"),
		GetWorld() ? *GetWorld()->GetName() : TEXT("<null>"),
		GetWorld() ? static_cast<int32>(GetWorld()->WorldType) : -1,
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		(GetWorld() && GetWorld()->IsGameWorld()) ? 1 : 0);

	Super::BeginPlay(); // ASinglePlayerGameMode: HUD/pawn/feature init

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		UE_LOG(LogProjectCinematic, Warning,
			TEXT("[CinematicGM][BeginPlay] No PlayerController found via GetPlayerController(this, 0); cinematic setup skipped."));
		return;
	}
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][BeginPlay] Resolved PlayerController='%s' (class=%s) | startPawn='%s' (class=%s)"),
		*PC->GetName(), *PC->GetClass()->GetName(),
		PC->GetPawn() ? *PC->GetPawn()->GetName() : TEXT("<null>"),
		(PC->GetPawn() && PC->GetPawn()->GetClass()) ? *PC->GetPawn()->GetClass()->GetName() : TEXT("<null>"));

	// Re-check MRQ at BeginPlay timing. The editor subsystem holds the
	// ActiveExecutor BEFORE Execute() is called (see
	// MoviePipelineQueueSubsystem.cpp:138), so the executor IS set during
	// InitGame -- but a BeginPlay re-check is cheap insurance against any
	// MRQ entry point that defers the assignment.
	bool bMRQActiveNow = false;
#if WITH_EDITOR
	if (GEditor)
	{
		if (UMoviePipelineQueueSubsystem* MRQ =
				GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>())
		{
			bMRQActiveNow = (MRQ->GetActiveExecutor() != nullptr);
		}
	}
#endif
	const ECinematicSessionMode PrevMode = SessionMode;
	if (bMRQActiveNow)
	{
		SessionMode = ECinematicSessionMode::Render;
	}
	if (PrevMode != SessionMode)
	{
		UE_LOG(LogProjectCinematic, Log,
			TEXT("[CinematicGM][BeginPlay] MRQ re-check switched SessionMode: %s -> %s"),
			PrevMode == ECinematicSessionMode::Render ? TEXT("Render") : TEXT("Record"),
			SessionMode == ECinematicSessionMode::Render ? TEXT("Render") : TEXT("Record"));
	}

	const bool bRenderMode = (SessionMode == ECinematicSessionMode::Render);

	// Render: cinematic flag ON, block input. Sequencer drives the camera and
	//         input movement/turning are not needed (and would fight the
	//         recorded sequence).
	// Record: cinematic flag OFF, input ALIVE. Take Recorder needs the player
	//         to walk/look with mouse + keyboard; the cinematic flag would
	//         affect gameplay code that gates on it during the take.
	// HUD and pawn stay visible in both profiles - MoviePipelineWidgetRenderer
	// composites the HUD into render frames; Take Recorder captures the pawn
	// during recording.
	PC->SetCinematicMode(
		/*bInCinematicMode*/  bRenderMode,
		/*bHidePlayer*/       false,
		/*bAffectsHUD*/       false,
		/*bAffectsMovement*/  bRenderMode,
		/*bAffectsTurning*/   bRenderMode);

	// Pawn time-dilation. Skip when 1.0 (no-op) to avoid touching pawn state
	// unnecessarily. Per-actor scaling: this pawn's tick dt is multiplied by
	// PawnTimeDilation. Camera, Sequencer time, and other actors unaffected.
	APawn* Pawn = PC->GetPawn();
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][BeginPlay] SetCinematicMode applied | bRenderMode=%d (=> bInCinematicMode=%d bAffectsMovement=%d bAffectsTurning=%d) | pawn='%s' loc=%s"),
		bRenderMode ? 1 : 0,
		bRenderMode ? 1 : 0, bRenderMode ? 1 : 0, bRenderMode ? 1 : 0,
		Pawn ? *Pawn->GetName() : TEXT("<null>"),
		Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("<null>"));

	const bool bDilationActive = Pawn && PawnTimeDilation > 0.f &&
		!FMath::IsNearlyEqual(PawnTimeDilation, 1.0f);
	if (bDilationActive)
	{
		Pawn->CustomTimeDilation = PawnTimeDilation;
		UE_LOG(LogProjectCinematic, Log,
			TEXT("[CinematicGM][BeginPlay] Pawn time dilation applied | pawn='%s' CustomTimeDilation=%.2f"),
			*Pawn->GetName(), PawnTimeDilation);
	}
	else
	{
		UE_LOG(LogProjectCinematic, Verbose,
			TEXT("[CinematicGM][BeginPlay] Pawn dilation skipped | hasPawn=%d dilation=%.2f"),
			Pawn ? 1 : 0, PawnTimeDilation);
	}

	// In Render profile, hide the gameplay-spawned default pawn. UE GameMode
	// spawns a pawn at PlayerStart on RestartPlayer; the recorded LevelSequence
	// independently creates its own Spawnable for the cinematic actor.
	// Without this hide step, the phantom static pawn at PlayerStart can
	// drift into the recorded camera's frustum and visibly contaminate the
	// rendered frame. Hiding (rather than suppressing the spawn) keeps
	// PC->GetPawn() valid so HUD, UI, and interaction systems that depend
	// on a pawn-owning PlayerController keep working.
	//
	// Shadow note: `SetActorHiddenInGame(true)` alone does NOT kill the
	// shadow if any primitive component has `bCastHiddenShadow = true`
	// (common on character meshes -- keeps the shadow during in-game
	// invisibility effects). Verified empirically 2026-05-19: phantom
	// pawn body was gone but shadow remained on the asphalt. We
	// additionally iterate the pawn's primitive components and call
	// `SetCastShadow(false)` on each, which forces the shadow off
	// regardless of the bCastHiddenShadow flag.
	//
	// Collision note: we deliberately do NOT disable collision here.
	// This gamemode can be selected for PIE too (e.g. World Settings
	// GameMode Override), and disabling the pawn's collision in PIE
	// Render would make the player fall through the floor. Collision
	// is harmless in MRQ (no live input).
	bool bPawnHidden = false;
	int32 ShadowsKilled = 0;
	if (bRenderMode && Pawn)
	{
		Pawn->SetActorHiddenInGame(true);

		TArray<UPrimitiveComponent*> Primitives;
		Pawn->GetComponents<UPrimitiveComponent>(Primitives);
		for (UPrimitiveComponent* Prim : Primitives)
		{
			if (Prim)
			{
				Prim->SetCastShadow(false);
				++ShadowsKilled;
			}
		}
		bPawnHidden = true;
		UE_LOG(LogProjectCinematic, Log,
			TEXT("[CinematicGM][BeginPlay] Phantom pawn hidden | pawn='%s' bHidden=true shadowsKilled=%d/%d"),
			*Pawn->GetName(), ShadowsKilled, Primitives.Num());
	}

	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][BeginPlay] Summary | mode=%s pawn_dilation=%.2f input_blocked=%d phantom_pawn_hidden=%d"),
		bRenderMode ? TEXT("Render") : TEXT("Record"),
		PawnTimeDilation,
		bRenderMode ? 1 : 0,
		bPawnHidden ? 1 : 0);

	// Render-time diagnostic: who/what is the active camera? MRQ uses the
	// PlayerController's view target as the rendering camera by default.
	// If the take spawnable pawn (DefinitionCharacter0) is NOT auto-set as
	// view target, MRQ will render from the gameplay phantom pawn's POV
	// (PlayerStart location) -> no recorded camera movement appears in the
	// final video.
	if (bRenderMode)
	{
		AActor* ViewTarget = PC->GetViewTarget();
		const FVector PawnLoc = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		const FRotator PawnRot = Pawn ? Pawn->GetActorRotation() : FRotator::ZeroRotator;
		UE_LOG(LogProjectCinematic, Log,
			TEXT("ACinematicGameMode[Render]: ViewTarget='%s' (class=%s) | gameplay pawn loc=%s rot=%s | controller=%s"),
			ViewTarget ? *ViewTarget->GetName() : TEXT("<null>"),
			(ViewTarget && ViewTarget->GetClass()) ? *ViewTarget->GetClass()->GetName() : TEXT("<null>"),
			*PawnLoc.ToCompactString(),
			*PawnRot.ToCompactString(),
			*PC->GetName());
	}

	// Spawn a hidden helper actor with UWorldPartitionStreamingSourceComponent.
	// Keeps WP cells loaded around the cinematic camera area during both PIE
	// recording and MRQ render. Without this, far-away cinematic actors can
	// unload mid-take and their Possessable bindings go stale.
	//
	// Verified UE 5.7 (research round 4):
	// - PinActors() is editor-only (WorldPartition.h:460, WITH_EDITOR-only)
	// - DisableStreamingIn() only blocks new cells (doesn't force-load)
	// - MRQ does NOT pre-stream camera trajectory; relies on existing
	//   streaming sources + BlockTillLevelStreamingCompleted per frame
	// - UWorldPartitionStreamingSourceComponent is the runtime-correct path
	//   (auto-registers via WorldPartitionStreamingSourceComponent.cpp:25-41)
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CinematicStreamingHost = World->SpawnActor<AActor>(
			AActor::StaticClass(), CinematicTrajectoryCenter, FRotator::ZeroRotator, SpawnParams);
		if (CinematicStreamingHost)
		{
			CinematicStreamingHost->SetActorHiddenInGame(true);

			UWorldPartitionStreamingSourceComponent* StreamingSrc =
				NewObject<UWorldPartitionStreamingSourceComponent>(CinematicStreamingHost);
			if (StreamingSrc)
			{
				FStreamingSourceShape Shape;
				Shape.bUseGridLoadingRange = false;
				Shape.Radius = CinematicStreamingRadiusCm;
				Shape.bIsSector = false;
				StreamingSrc->Shapes.Add(Shape);
				StreamingSrc->Priority = EStreamingSourcePriority::Highest;
				StreamingSrc->RegisterComponent();  // auto-registers with WP subsystem

				UE_LOG(LogProjectCinematic, Log,
					TEXT("[CinematicGM][BeginPlay] WP streaming source OK | hostActor='%s' center=%s radius=%.0fcm priority=Highest registered=%d"),
					*CinematicStreamingHost->GetName(),
					*CinematicTrajectoryCenter.ToString(), CinematicStreamingRadiusCm,
					StreamingSrc->IsRegistered() ? 1 : 0);
			}
			else
			{
				UE_LOG(LogProjectCinematic, Error,
					TEXT("[CinematicGM][BeginPlay] Failed to create UWorldPartitionStreamingSourceComponent on host actor '%s'."),
					*CinematicStreamingHost->GetName());
			}
		}
		else
		{
			UE_LOG(LogProjectCinematic, Error,
				TEXT("[CinematicGM][BeginPlay] Failed to spawn CinematicStreamingHost AActor at %s. WP cells around cinematic area may unload mid-take."),
				*CinematicTrajectoryCenter.ToString());
		}
	}
	else
	{
		UE_LOG(LogProjectCinematic, Warning,
			TEXT("[CinematicGM][BeginPlay] No game world or not a game world; WP streaming source NOT spawned | hasWorld=%d isGameWorld=%d"),
			World ? 1 : 0, (World && World->IsGameWorld()) ? 1 : 0);
	}

	// Render-mode setup: ONE-SHOT cleanup of stale baked CustomDepth on every
	// actor's primitive components. Reason: Take Recorder duplicates
	// component state at the moment of AddSource, possibly including dirty
	// bRenderCustomDepth=true on multiple components from rapid focus
	// changes during the take. Without this scrub, those bake into the
	// rendered frame as ghost outlines on uninvolved drawers.
	//
	// IMPORTANT: we do NOT call SuppressInteractionVisuals() here. The
	// InteractionComponent's live focus/prompt broadcast is GAMEPLAY UX
	// that we WANT in the cinematic render -- the "[E] Open" widget and
	// the focus outline are part of the trailer's gameplay-readable
	// narrative ("the player walked up and interacted"). Killing the
	// broadcast removes the prompt from the render frame.
	//
	// What remains under cinematic control:
	//   - Take's Spawnable mesh transforms (drawer motion) -- Take Recorder
	//   - Editor placement hidden -- bHidden Possessable track
	//   - Outline on the specific interacted component over a deterministic
	//     window around each event -- StampInteractionHighlights bool track
	//
	// What remains under gameplay control:
	//   - Live focus driven by InteractionComponent traces
	//   - "[E] Open" HUD widget broadcast through IInteractionService
	if (bRenderMode && World)
	{
		ClearStaleCustomDepthAcrossWorld();

		UE_LOG(LogProjectCinematic, Log,
			TEXT("[CinematicGM][BeginPlay] Render-mode cleanup complete | stale stencils cleared (one-shot); live InteractionComponent kept enabled for [E] prompt + focus broadcasts"));
	}

	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][BeginPlay] END | session=%s"),
		bRenderMode ? TEXT("Render") : TEXT("Record"));
}

// NOTE -- legacy SuppressCinematicInteractionVisuals() removed.
//
// The function used to iterate the pawn's components for any
// IInteractionVisualSuppressor implementers and call SuppressInteractionVisuals()
// on each (the InteractionComponent's implementation clears focus + disables
// live highlight production for the render session).
//
// Removed because: the cinematic Render mode now KEEPS the live
// InteractionComponent active. The [E] prompt and focus-driven outline are
// gameplay-readable trailer UX that should appear in the rendered frame --
// suppressing them produced sterile shots that didn't match the user's
// vision for ALIS cinematics.
//
// The interface (Plugins/Foundation/ProjectCore/.../IInteractionVisualSuppressor.h)
// and InteractionComponent's implementation remain in place as an architectural
// seam for any future "clean cinematic mode" toggle. Nothing currently
// invokes them.

void ACinematicGameMode::ClearStaleCustomDepthAcrossWorld()
{
	// One-shot stale-stencil scrub: walk every actor's primitive components
	// and clear bRenderCustomDepth + CustomDepthStencilValue exactly once,
	// during Render-mode BeginPlay. Removes the ghost-outline artifacts that
	// come from Take Recorder Spawnable component templates carrying
	// dirty stencil state inherited from the moment of AddSource.
	//
	// After this clear, the ONLY thing that should re-enable stencil for
	// the render is the Sequencer property tracks written by
	// UProjectCinematicSubsystem::StampInteractionHighlights -- those drive
	// bRenderCustomDepth=true only during recorded interaction windows on
	// the recorded component, exactly the cinematic highlight we want.
	UWorld* World = GetWorld();
	if (!World) { return; }

	int32 ActorsScrubbed = 0;
	int32 PrimsCleared   = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) { continue; }
		++ActorsScrubbed;

		TArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents<UPrimitiveComponent>(Primitives);
		for (UPrimitiveComponent* Prim : Primitives)
		{
			if (!Prim) { continue; }
			if (Prim->bRenderCustomDepth || Prim->CustomDepthStencilValue != 0)
			{
				Prim->SetRenderCustomDepth(false);
				Prim->SetCustomDepthStencilValue(0);
				++PrimsCleared;
			}
		}
	}

	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][StaleStencilClear] actors=%d primsCleared=%d (one-shot, render mode)"),
		ActorsScrubbed, PrimsCleared);
}

void ACinematicGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][EndPlay] BEGIN | reason=%d session=%s hostActor='%s'"),
		static_cast<int32>(EndPlayReason),
		SessionMode == ECinematicSessionMode::Render ? TEXT("Render") : TEXT("Record"),
		CinematicStreamingHost ? *CinematicStreamingHost->GetName() : TEXT("<null>"));

	// Render-mode suppression is now one-shot at BeginPlay; no timer/handler
	// teardown needed.
	if (CinematicStreamingHost)
	{
		const FString HostName = CinematicStreamingHost->GetName();
		CinematicStreamingHost->Destroy();
		CinematicStreamingHost = nullptr;
		UE_LOG(LogProjectCinematic, Log,
			TEXT("[CinematicGM][EndPlay] Destroyed streaming host actor='%s'"),
			*HostName);
	}
	Super::EndPlay(EndPlayReason);
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[CinematicGM][EndPlay] END"));
}
