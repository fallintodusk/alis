#include "ProjectWorldMeshTerrainEvidenceSubject.h"

#include "EngineUtils.h"
#include "MeshPartitionEditorComponent.h"
#include "MeshPartitionModifierComponent.h"
#include "MeshPartitionPreviewSection.h"
#include "ProjectWorldMeshTerrainPartition.h"

FProjectWorldMeshTerrainEvidenceSubject& FProjectWorldMeshTerrainEvidenceSubject::Get()
{
	static FProjectWorldMeshTerrainEvidenceSubject Subject;
	return Subject;
}

FProjectWorldEvidenceSubjectReport FProjectWorldMeshTerrainEvidenceSubject::Inspect(const UWorld& World) const
{
	using namespace UE::MeshPartition;

	FProjectWorldEvidenceSubjectReport Report;
	Report.SubjectName = TEXT("ProjectWorldMeshTerrain");
	for (TActorIterator<AProjectWorldMeshTerrainPartition> PartitionIt(&World); PartitionIt; ++PartitionIt)
	{
		Report.bPresent = true;
		const AMeshPartition* Partition = *PartitionIt;
		// Loaded base modifiers count before the editor component registers them, so a world whose
		// component is missing or late still expects its units and reads as undrawn.
		int32 PartitionUnits = 0;
		for (TActorIterator<AActor> ActorIt(&World); ActorIt; ++ActorIt)
		{
			const TInlineComponentArray<UModifierComponent*> Modifiers(*ActorIt);
			for (const UModifierComponent* Modifier : Modifiers)
			{
				if (Modifier->IsBase() && Modifier->GetAffectedMeshPartition() == Partition)
				{
					++PartitionUnits;
					const APreviewSection* PreviewSection = Modifier->GetPreviewSection();
					Report.DrawnUnits += PreviewSection != nullptr && !PreviewSection->IsHiddenEd() ? 1 : 0;
				}
			}
		}
		// A partition without any loaded base modifier can never draw; it counts as one undrawn
		// unit so another partition's drawn units cannot make the subject read as drawn.
		Report.ExpectedUnits += FMath::Max(PartitionUnits, 1);
		const UMeshPartitionEditorComponent* EditorComponent =
			Cast<UMeshPartitionEditorComponent>(Partition->GetMeshPartitionComponent());
		Report.PendingBuilds +=
			EditorComponent != nullptr && EditorComponent->IsAnyPreviewSectionBuildActive() ? 1 : 0;
	}
	return Report;
}
