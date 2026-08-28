#pragma once

#include "CoreMinimal.h"
#include "SinglePlayerGameMode.h"
#include "Interfaces/ILookInputModifier.h"
#include "CinematicGameMode.generated.h"

/**
 * Which cinematic session profile this GameMode instance represents.
 *
 * The Render and Record profiles need OPPOSITE handling of player input:
 *  - Render blocks input because Sequencer drives the camera and the take
 *    has already been recorded.
 *  - Record must keep input alive so the player can walk/look with mouse
 *    + keyboard and Take Recorder can capture the resulting motion.
 */
UENUM(BlueprintType)
enum class ECinematicSessionMode : uint8
{
	/** MRQ render-time: cinematic flag ON, movement/turning input BLOCKED. */
	Render UMETA(DisplayName = "Render"),

	/** PIE recording-time: cinematic flag OFF, input ALIVE for Take Recorder. */
	Record UMETA(DisplayName = "Record")
};

/**
 * GameMode used by Movie Render Queue (Render profile) and by PIE recording
 * sessions where Take Recorder captures slowed gameplay (Record profile).
 *
 * Inherits ASinglePlayerGameMode so the project's normal init runs (HUD spawn,
 * pawn spawn, feature init, vitals/death wiring).
 *
 * Profile auto-selection (no Blueprint children needed, no URL options needed):
 *
 *   - PIE / standalone: SessionMode defaults to Record (UPROPERTY default).
 *     Walk-slowdown (PawnTimeDilation=0.5) and look-smoothing
 *     (LookSensitivityScale=0.5) apply automatically.
 *
 *   - MRQ render: InitGame queries the Editor
 *     `UMoviePipelineQueueSubsystem::GetActiveExecutor()`; if non-null,
 *     SessionMode is force-flipped to Render. The phantom-pawn-hide in
 *     BeginPlay then fires, removing the gameplay-spawned default pawn's
 *     visibility + shadow from the rendered frame.
 *
 *   - URL escape hatch: `?CinematicSession=Record|Render` forces the profile
 *     explicitly (parsed after MRQ auto-detection, so URL wins).
 *
 *  Render profile:
 *    SetCinematicMode(true, false, true, true, true)
 *      - cinematic flag ON
 *      - gameplay pawn hidden and HUD suppressed for clean release footage
 *      - movement + turning input BLOCKED (Sequencer drives camera)
 *      - gameplay-spawned phantom pawn hidden (visibility only; collision
 *        intentionally kept on so PIE Render selection doesn't break walking).
 *
 *  Record profile:
 *    SetCinematicMode(false, false, false, false, false)
 *      - cinematic flag OFF (real gameplay loop runs)
 *      - pawn + HUD visible, input ALIVE for Take Recorder capture
 *    PawnTimeDilation = 0.5, LookSensitivityScale = 0.5 (defaults applied
 *    whenever the active profile is Record).
 *
 * Deployment - one C++ class, no Blueprint children, no per-session URL:
 *
 *   PIE recording:
 *     City17's WorldSettings.GameMode Override = ACinematicGameMode.
 *     Press Play. Record profile is automatic.
 *
 *   MRQ render:
 *     Preset's SoftGameModeOverride = ACinematicGameMode. MRQ
 *     auto-detection forces Render at InitGame.
 *
 * Module dep note: this class lives in the Editor-only ProjectCinematic
 * module. Project target allowlists keep this code and its capture
 * dependencies out of cooked game/client/server builds. ProjectSinglePlay
 * carries no cinematic dependency.
 *
 * Design rationale: docs/cinematics/render_setup.md
 */
UCLASS(MinimalAPI)
class ACinematicGameMode : public ASinglePlayerGameMode, public ILookInputModifier
{
	GENERATED_BODY()

	// ILookInputModifier
public:
	virtual FVector2D ModifyLook(const FVector2D& Input) const override
	{
		return Input * LookSensitivityScale;
	}

public:
	/**
	 * Which cinematic profile this instance represents. Drives input blocking,
	 * cinematic flag, and (combined with PawnTimeDilation) timing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic")
	ECinematicSessionMode SessionMode = ECinematicSessionMode::Record;

	/**
	 * Per-actor time-dilation applied to the player pawn. 1.0 = no slowdown.
	 * Typical values:
	 *   Render profile: 1.0 (sequence keyframes already encode timing).
	 *   Record profile: 0.5 - slower walking, Take Recorder captures the
	 *                          slowed motion directly into the LevelSequence.
	 *
	 * Note: this scales the pawn's tick dt; physics outside the pawn may not
	 * follow this scale uniformly. Use the Record profile to capture slower
	 * walking; use the Render profile to render already-recorded sequences.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic",
		meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float PawnTimeDilation = 1.0f;

	/**
	 * Look-input multiplier applied during Record profile. Lower values
	 * produce smoother captured camera transforms because the player's
	 * natural hand-tremor maps to fewer recorded degrees of rotation.
	 *
	 * Render profile ignores this value (input is already blocked there).
	 * Record profile auto-sets this to 0.5 unless the caller overrides
	 * via `?CinematicLookScale=` URL option.
	 *
	 * Applied via the `ILookInputModifier` interface (declared in
	 * ProjectCore) that this game mode implements. The character's Look()
	 * handler queries the active game mode's interface and runs raw input
	 * through `ModifyLook()` -- this keeps the character module decoupled
	 * from the cinematic game mode at compile time.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic",
		meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float LookSensitivityScale = 1.0f;

	/** True only inside an MRQ render session (or an explicit
	 * `?CinematicSession=Render` URL launch). Use this to gate
	 * cinematic-only bypass paths (e.g. interaction distance-gate skip)
	 * so they can't be invoked during PIE / standalone live play. */
	UFUNCTION(BlueprintPure, Category = "Cinematic")
	bool IsRenderSession() const
	{
		return SessionMode == ECinematicSessionMode::Render;
	}

	/** Centroid of the World Partition streaming source spawned on BeginPlay.
	 *  Set per cinematic sublevel to the rough middle of the camera's trajectory.
	 *  Default (zero) is fine if the cinematic action happens near world origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Streaming")
	FVector CinematicTrajectoryCenter = FVector::ZeroVector;

	/** Radius of the WP streaming source, in cm. Default 1km - generous for
	 *  indoor trailer scenes; bump for outdoor wide shots. Wider = more
	 *  cells loaded = more memory; tune per shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Streaming",
		meta = (ClampMin = "100.0", UIMin = "100.0"))
	float CinematicStreamingRadiusCm = 100000.0f;

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Hidden helper actor that hosts UWorldPartitionStreamingSourceComponent.
	 *  Spawned in BeginPlay, destroyed in EndPlay. Owns the streaming source
	 *  so cells stay loaded around the recorded camera area during both PIE
	 *  recording and MRQ render. */
	UPROPERTY()
	TObjectPtr<AActor> CinematicStreamingHost;

	FTimerHandle CinematicStreamingFollowTimer;

	/** Keep the streaming source on the active Sequencer camera. */
	void UpdateCinematicStreamingSourceLocation();

	// --- Render-mode setup ---
	//
	// One ONE-SHOT cleanup at BeginPlay: stale baked CustomDepth from Take
	// Recorder Spawnable component templates (which can carry dirty
	// bRenderCustomDepth=true from rapid focus changes during the take).
	// Cleared once so the render starts from a clean stencil state.
	//
	// We intentionally do NOT periodically scrub stencils -- the cinematic
	// highlight that SHOULD appear in the render comes from Sequencer bool
	// property tracks written by
	// UProjectCinematicSubsystem::StampFocusHighlights during the take's
	// finish-stamp pass. Those tracks mirror gameplay focus state exactly;
	// a periodic scrub would fight them.
	//
	// We intentionally leave interaction focus/highlight state alone so
	// authored property tracks still render. Render mode removes viewport
	// widgets separately, so gameplay prompts do not leak into clean shots.
	// Record mode retains the ordinary interactive HUD.
	void ClearStaleCustomDepthAcrossWorld();
};
