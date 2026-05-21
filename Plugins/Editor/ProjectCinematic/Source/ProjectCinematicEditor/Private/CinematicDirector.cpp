// Copyright ALIS. All Rights Reserved.

#include "CinematicDirector.h"

#include "CinematicHideMetadata.h"   // recorded-original soft-ref carrier
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "ProjectCinematicEditor.h"   // LogProjectCinematicEditor
#include "SinglePlayController.h"

void UCinematicDirector::SetPanelVisible(FName Panel, bool bVisible)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicDirector] SetPanelVisible(%s, %d) -- no world; aborting."),
			*Panel.ToString(), bVisible ? 1 : 0);
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	ASinglePlayController* SPC = Cast<ASinglePlayController>(PC);
	if (!SPC)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicDirector] SetPanelVisible(%s, %d) -- no ASinglePlayController in world '%s' (type=%d)."),
			*Panel.ToString(), bVisible ? 1 : 0,
			*World->GetName(), static_cast<int32>(World->WorldType));
		return;
	}

	if      (Panel == TEXT("Inventory"))   { SPC->SetPanel_InventoryVisible(bVisible); }
	else if (Panel == TEXT("Vitals"))      { SPC->SetPanel_VitalsVisible(bVisible); }
	else if (Panel == TEXT("MindJournal")) { SPC->SetPanel_MindJournalVisible(bVisible); }
	else
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicDirector] Unknown panel name '%s' (bVisible=%d). Add an `else if` branch + a new thunk to extend."),
			*Panel.ToString(), bVisible ? 1 : 0);
	}
}

void UCinematicDirector::Cinematic_OpenInventory()    { SetPanelVisible(TEXT("Inventory"),   true);  }
void UCinematicDirector::Cinematic_CloseInventory()   { SetPanelVisible(TEXT("Inventory"),   false); }
void UCinematicDirector::Cinematic_OpenVitals()       { SetPanelVisible(TEXT("Vitals"),      true);  }
void UCinematicDirector::Cinematic_CloseVitals()      { SetPanelVisible(TEXT("Vitals"),      false); }
void UCinematicDirector::Cinematic_OpenMindJournal()  { SetPanelVisible(TEXT("MindJournal"), true);  }
void UCinematicDirector::Cinematic_CloseMindJournal() { SetPanelVisible(TEXT("MindJournal"), false); }

void UCinematicDirector::Cinematic_HideRecordedOriginals()
{
	// Director's own sequence -> UCinematicHideMetadata -> soft ref list.
	// Sequencer's Event Track binds this method to a frame at playback start
	// during the editor-time stamp pass (StampHideOriginals). At render time
	// the Event evaluator fires this thunk, which directly toggles
	// SetActorHiddenInGame on each editor placement -- no Sequencer binding
	// resolution involved (works for WP external actors that fail Possessable
	// binding resolution at MRQ).
	UMovieSceneSequence* OwnSeq = GetSequence();
	ULevelSequence* LevelSeq = Cast<ULevelSequence>(OwnSeq);
	if (!LevelSeq)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicDirector] Cinematic_HideRecordedOriginals: GetSequence() returned non-LevelSequence (%s); cannot read hide metadata."),
			OwnSeq ? *OwnSeq->GetClass()->GetName() : TEXT("<null>"));
		return;
	}

	UCinematicHideMetadata* Metadata = LevelSeq->FindMetaData<UCinematicHideMetadata>();
	if (!Metadata)
	{
		UE_LOG(LogProjectCinematicEditor, Log,
			TEXT("[CinematicDirector] Cinematic_HideRecordedOriginals: no UCinematicHideMetadata on '%s' (sequence has no recorded originals to hide)."),
			*LevelSeq->GetPathName());
		return;
	}

	int32 Hidden = 0;
	int32 Failed = 0;
	for (const FSoftObjectPath& Path : Metadata->ActorsToHide)
	{
		UObject* Resolved = Path.ResolveObject();
		AActor*  Actor    = Cast<AActor>(Resolved);
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
			++Hidden;
		}
		else
		{
			++Failed;
			UE_LOG(LogProjectCinematicEditor, Warning,
				TEXT("[CinematicDirector] Cinematic_HideRecordedOriginals: cannot resolve '%s' to AActor (resolved=%s); skipping."),
				*Path.ToString(),
				Resolved ? *Resolved->GetClass()->GetName() : TEXT("<null>"));
		}
	}

	UE_LOG(LogProjectCinematicEditor, Log,
		TEXT("[CinematicDirector] Cinematic_HideRecordedOriginals: hidden=%d failed=%d (sequence='%s')"),
		Hidden, Failed, *LevelSeq->GetPathName());
}
