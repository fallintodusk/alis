// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInteractionVisualSuppressor.generated.h"

/**
 * Contract: "I produce gameplay interaction UX (focus highlights, prompt
 * widgets, custom-depth stencils, etc.) and can be told to suppress that
 * UX for a non-gameplay rendering context (cinematic capture / MRQ render)."
 *
 * This interface is the decoupling seam between gameplay interaction
 * (ProjectInteraction) and cinematic capture (ProjectCinematic). Cinematic
 * code MUST NOT depend on ProjectInteraction's concrete InteractionComponent;
 * it discovers implementers at runtime via `UClass::ImplementsInterface()`
 * and calls `SuppressInteractionVisuals()` on whatever satisfies the
 * contract. New visual-producers (future interaction-like systems --
 * dialogue prompts, examine cursors, tutorial highlights) implement the
 * same interface and inherit cinematic suppression for free.
 *
 * Semantics of SuppressInteractionVisuals():
 *  - Clear any currently-displayed focus / highlight on visual primitives
 *  - Stop producing new highlights for the remainder of this play session
 *    (cinematic render is one-shot per PIE/MRQ session; this is not a
 *    toggle that needs to be paired with a "restore" call -- the next
 *    play session re-initialises everything)
 *  - Hide any HUD/UMG prompt widgets the implementer drives
 *  - Disable trace/tick paths that would re-resolve focus after the call
 *
 * Non-goals:
 *  - This is NOT for pausing gameplay rules. Gameplay logic (e.g. the
 *    ability to interact via input) is the implementer's choice; cinematic
 *    Render mode separately blocks input via PlayerController::SetCinematicMode.
 *  - This is NOT a "is interaction allowed" gate. It is a visual-only switch.
 *
 * Default implementation is a no-op so non-suppressible components are
 * silently skipped by callers iterating components on a pawn.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UInteractionVisualSuppressor : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IInteractionVisualSuppressor
{
	GENERATED_BODY()

public:
	/**
	 * Cinematic rendering is about to begin (or is active). Clear all
	 * currently-displayed interaction visuals and stop producing new ones
	 * for the rest of this play session.
	 *
	 * Implementers must be safe to call this multiple times (idempotent --
	 * cinematic code may sweep on a timer to catch late-spawning visuals).
	 */
	virtual void SuppressInteractionVisuals() {}
};
