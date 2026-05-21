// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceDirector.h"
#include "CinematicDirector.generated.h"

/**
 * Pure C++ ULevelSequenceDirector for ALIS cinematic capture.
 *
 * Cinematic UI panel events are stamped against this class by the
 * editor-only CinematicTakeStamperSubsystem after Take Recorder finishes
 * a recording. Each Event Track trigger key in the produced LevelSequence
 * points at one of the parameterless thunks below; the thunk forwards to
 * SetPanelVisible with baked literal arguments.
 *
 * Why parameterless thunks: at runtime UE's MovieSceneEventSystems
 * (Engine/Source/Runtime/MovieSceneTracks/Private/Systems/
 * MovieSceneEventSystems.cpp:237-286) memzeros the parameter buffer
 * before ProcessEvent and never reads FMovieSceneEvent::PayloadVariables
 * (PayloadVariables is #if WITH_EDITORONLY_DATA, consumed only by the
 * Blueprint compiler at compile time). Baking the panel name + bool
 * into the C++ thunk body at compile time is the runtime-correct way
 * to pass data through a Sequencer event when not using a Director BP.
 *
 * Visible motion (drawer slides etc.) is NOT routed through here --
 * Take Recorder captures the actor + child component transforms
 * natively into transform tracks. This Director is only for UI panel
 * toggles which have no spatial state to bake.
 */
UCLASS()
class PROJECTCINEMATICEDITOR_API UCinematicDirector : public ULevelSequenceDirector
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cinematic|UI") void Cinematic_OpenInventory();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cinematic|UI") void Cinematic_CloseInventory();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cinematic|UI") void Cinematic_OpenVitals();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cinematic|UI") void Cinematic_CloseVitals();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cinematic|UI") void Cinematic_OpenMindJournal();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cinematic|UI") void Cinematic_CloseMindJournal();

	/**
	 * Hide each editor-world actor listed in this sequence's
	 * UCinematicHideMetadata. Stamped as an Event Track key at the sequence's
	 * playback start. The recorded take's Spawnable duplicates the actor at
	 * the same world transform; hiding the editor placement leaves exactly
	 * one visible -- the take's animated duplicate. See
	 * UProjectCinematicSubsystem::StampHideOriginals for the writer side.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cinematic|Hide")
	void Cinematic_HideRecordedOriginals();

private:
	/** Shared dispatch. Resolves the live PC from the director's world and toggles
	 *  the named panel. World resolution path validated when the recorder/stamper
	 *  land; until then this is a stub. */
	void SetPanelVisible(FName Panel, bool bVisible);
};
