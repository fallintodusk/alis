#include "ProjectWorldActor.h"
#include "IDefinitionApplicable.h"
#include "Components/SceneComponent.h"

#if WITH_EDITOR
#include "Engine/AssetManager.h"
#include "Logging/MessageLog.h"
#include "UObject/ObjectSaveContext.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldActor, Log, All);

AProjectWorldActor::AProjectWorldActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create default root component for proper transform handling during placement
	USceneComponent* DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);
}

#if WITH_EDITOR
void AProjectWorldActor::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	// Mode 2: Batch actor updates during commandlet saves
	// Runs during WorldPartitionResaveActorsBuilder or manual saves
	// Silent updates - no UI interaction (commandlet context)

	// Only update if:
	// 1. Actor has a definition link (ObjectDefinitionId is valid)
	// 2. Actor implements IDefinitionApplicable interface
	// 3. Not in editor transaction (avoid updating during manual edits)
	if (!ObjectDefinitionId.IsValid() || SaveContext.IsProceduralSave())
	{
		return;
	}

	if (!this->Implements<UDefinitionApplicable>())
	{
		return;
	}

	// Load definition via AssetManager
	UAssetManager& AssetManager = UAssetManager::Get();
	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ObjectDefinitionId);
	if (AssetPath.IsNull())
	{
		return;
	}

	UObject* LoadedAsset = AssetPath.TryLoad();
	UPrimaryDataAsset* Def = Cast<UPrimaryDataAsset>(LoadedAsset);
	if (!Def)
	{
		return;
	}

	// Check if update needed via interface's property reflection
	// We can't directly access DefinitionContentHash without knowing the definition type,
	// but ApplyDefinition will check structure hash internally
	// So we just try to apply - actor will validate structure hash and return false if mismatch

	// Try to apply definition (actor validates structure hash internally)
	const bool bSuccess = IDefinitionApplicable::Execute_ApplyDefinition(this, Def);

	if (bSuccess)
	{
		UE_LOG(LogProjectWorldActor, Log, TEXT("[Mode2] Applied definition to: %s"), *GetActorLabel());
	}
	else
	{
		// Structure mismatch: definition has structural changes (meshes/capabilities changed).
		// Cannot apply in-place - ActorSync will auto-replace this actor when definition is regenerated.
		UE_LOG(LogProjectWorldActor, Log, TEXT("[Mode2] '%s': structural changes detected, cannot update in-place. ActorSync should auto-replace when definition regenerated."), *GetActorLabel());
	}
}

void AProjectWorldActor::GenerateNewDataId()
{
	DataId = FGuid::NewGuid();
}
#endif
