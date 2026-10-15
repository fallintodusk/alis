#include "ProjectWorldMeshTerrainPartition.h"

#if WITH_EDITOR
#include "ProjectWorldMeshTerrainAuthoringRole.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MeshPartitionComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldMeshTerrainVisibility, Log, All);

void AProjectWorldMeshTerrainPartition::PostLoad()
{
	Super::PostLoad();
	RestoreEditorComponent();
}

void AProjectWorldMeshTerrainPartition::PostActorCreated()
{
	Super::PostActorCreated();
	RestoreEditorComponent();
}

void AProjectWorldMeshTerrainPartition::RestoreEditorComponent()
{
	if (GetWorld() == nullptr || GetWorld()->IsGameWorld() || GetMeshPartitionComponent() != nullptr)
	{
		return;
	}
	UClass* ComponentClass = LoadClass<UE::MeshPartition::UMeshPartitionComponent>(
		nullptr, TEXT("/Script/MeshPartitionEditor.MeshPartitionEditorComponent"));
	if (ComponentClass == nullptr)
	{
		return;
	}
	auto* Component = NewObject<UE::MeshPartition::UMeshPartitionComponent>(
		this, ComponentClass, TEXT("MeshPartitionEditorComponent"), RF_Transactional);
	SetMeshPartitionComponent(Component);
}

void AProjectWorldMeshTerrainPartition::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	AuthoringRegistrationHandle = World->AddOnPostRegisterAllActorComponentsHandler(
		FOnPostRegisterAllActorComponents::FDelegate::CreateUObject(this,
			&AProjectWorldMeshTerrainPartition::SuppressGameAuthoring));
	for (TActorIterator<AActor> It(World); It; ++It) SuppressGameAuthoring(*It);
}

void AProjectWorldMeshTerrainPartition::SuppressGameAuthoring(AActor* Actor)
{
	if (!Actor || Actor->GetWorld() != GetWorld() || !GetWorld()->IsGameWorld() ||
		!Actor->ActorHasTag(ProjectWorldMeshTerrainAuthoringRole::Tag())) return;
	const FString& Prefix = ProjectWorldMeshTerrainAuthoringRole::CellPrefix();
	if (!Actor->Tags.ContainsByPredicate([&Prefix](FName Tag)
		{ const FString Text = Tag.ToString(); return Text.StartsWith(Prefix) && Text.Len() > Prefix.Len(); })) return;
	int32 Hidden = 0;
	TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
	for (UPrimitiveComponent* Component : Components)
	{
		// Editor-only data survives PIE duplication; it must not compete with
		// the compiled terrain that owns gameplay rendering.
		if (Component->IsEditorOnly() && !Component->bHiddenInGame)
		{
			Component->SetHiddenInGame(true);
			++Hidden;
		}
	}
	if (Hidden > 0) UE_LOG(LogProjectWorldMeshTerrainVisibility, Display,
		TEXT("[AuthoringVisibility] World=%s Actor=%s hidden_editor_primitives=%d"),
		*GetWorld()->GetName(), *Actor->GetName(), Hidden);
}

void AProjectWorldMeshTerrainPartition::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld(); World && AuthoringRegistrationHandle.IsValid())
	{
		World->RemoveOnPostRegisterAllActorComponentsHandler(AuthoringRegistrationHandle);
		AuthoringRegistrationHandle.Reset();
	}
	Super::EndPlay(Reason);
}
#endif
