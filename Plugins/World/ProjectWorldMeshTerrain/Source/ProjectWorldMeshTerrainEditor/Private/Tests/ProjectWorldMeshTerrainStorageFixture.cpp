#include "ProjectWorldMeshTerrainStorageFixture.h"
#include "ProjectWorldMeshTerrainStorageTransformer.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionTransformerPipeline.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionEditorComponent.h"
#include "MeshPartitionGridSettings.h"
#include "MeshPartitionMeshBuilder.h"
#include "MeshPartitionMeshData.h"
#include "MeshPartition.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainStorageFixture
{
	bool PrepareDefinition(FAutomationTestBase& Test, UE::MeshPartition::UMeshPartitionDefinition& Definition,
		const UE::MeshPartition::UMeshPartitionDefinition& Shared)
	{
		using namespace UE::MeshPartition;
		if (!Test.TestTrue(TEXT("Storage experiments modify a separate definition and package."),
			&Definition != &Shared && Definition.GetPackage() != Shared.GetPackage())) return false;
		for (const auto& ReadOnly : Definition.GetCompiledSectionBuildVariants())
		{
			if (!Test.TestNotNull(TEXT("The cloned variant has its production pipeline."), ReadOnly.TransformerPipeline.Get())) return false;
			auto* Pipeline = DuplicateObject<UTransformerPipeline>(ReadOnly.TransformerPipeline, &Definition,
				MakeUniqueObjectName(&Definition, UTransformerPipeline::StaticClass(), TEXT("StoragePipeline")));
			if (!Test.TestTrue(TEXT("Only the separately duplicated pipeline can receive the storage transform."),
				Pipeline && Pipeline != ReadOnly.TransformerPipeline && Pipeline->IsIn(&Definition))) return false;
			Pipeline->ClearFlags(RF_Standalone);
			Pipeline->SetFlags(RF_Public);
			// Native assets expose read-only views; the mutable objects here are private duplicates.
			auto& Variant = const_cast<FCompiledSectionBuildVariant&>(ReadOnly);
			auto& Steps = const_cast<TArray<TInstancedStruct<FTransformer>>&>(Pipeline->GetTransformers());
			Steps.Insert(TInstancedStruct<FTransformer>::Make<FProjectWorldMeshTerrainStorageTransformer>(), 0);
			Variant.TransformerPipeline = Pipeline;
		}
		return true;
	}

	bool PrepareSplit(FAutomationTestBase& Test, UE::MeshPartition::UMeshPartitionEditorComponent& Editor,
		const UE::MeshPartition::FCompiledSectionBuildInfo& BaseInfo, const UE::MeshPartition::FCompiledSectionBuildVariant& Variant,
		const UE::MeshPartition::FMeshData& Native, const FTransform& ParentToWorld, const FVector& GridOrigin,
		FSplitSections& Out)
	{
		using namespace UE::MeshPartition;
		Out = FSplitSections();
		FGridSettings Grid;
		Grid.CellSize = 3000;
		Grid.bIs2D = true;
		Grid.WorldOriginOffset = GridOrigin;
		auto Cells = GridHelpers::BuildGridCellMeshes(Native, Grid, ParentToWorld);
		TArray<FIntVector> Keys; Cells.GetKeys(Keys);
		Keys.Sort([](const FIntVector& A, const FIntVector& B)
		{ return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z; });
		if (!Test.TestTrue(TEXT("The diagnostic native 30m grid creates multiple bounded source parts."), Keys.Num() >= 2 && Keys.Num() <= 31 * 31)) return false;
		for (const auto& Key : Keys)
		{
			auto Data = MakeShared<FMeshData>(MoveTemp(Cells.FindChecked(Key)));
			FCompiledSectionBuildInfo Info = BaseInfo;
			Info.BuildKey = FGuid::NewGuid(); Info.GridCellCoord = Key; Info.GridSettings = Grid;
			FPrepareCompiledSectionsParams Params{*Data};
			auto Sections = Editor.PrepareCompiledSections(Info, Variant, Params);
			if (!Test.TestEqual(TEXT("Each real native grid part supplies one private compiled section."), Sections.Num(), 1)) return false;
			Out.Sections.Add(Sections[0]); Out.Parts.Add(MoveTemp(Data));
		}
		return true;
	}

	bool CheckShared(FAutomationTestBase& Test, const FProjectWorldCanonicalBundle& Bundle, const FSplitSections& Split,
		const TArray<TArray<FProjectWorldSurfaceTriangle>>& Actual, FJsonObject& Receipt)
	{
		if (!Test.TestTrue(TEXT("Each split source owns an observed decoded field."),
			Split.Parts.Num() == Split.Sections.Num() && Actual.Num() == Split.Parts.Num())) return false;
		TMap<FVector, TArray<int32>> Owners;
		for (int32 Index = 0; Index < Split.Parts.Num(); ++Index)
			for (int32 Vertex : Split.Parts[Index]->VertexIndicesItr())
				Owners.FindOrAdd(Split.Parts[Index]->GetVertex(Vertex)).AddUnique(Index);
		int32 RequiredShared = 0;
		for (const auto& Entry : Owners) RequiredShared += Entry.Value.Num() > 1 ? 1 : 0;
		int32 Shared = 0, ComparedPairs = 0; double MaxGap = 0;
		FVector FaultPoint = FVector::ZeroVector; int32 FaultSection = INDEX_NONE;
		const auto Check = [&](const TArray<TArray<FProjectWorldSurfaceTriangle>>& Fields, bool Capture)
		{
			TArray<TSet<FVector>> Decoded; Decoded.SetNum(Fields.Num());
			for (int32 Index = 0; Index < Fields.Num(); ++Index)
				for (const auto& Face : Fields[Index]) for (const auto& Point : Face.Points) Decoded[Index].Add(Point);
			for (const auto& Entry : Owners)
			{
				if (Entry.Value.Num() < 2) continue;
				FVector First; bool HasFirst = false;
				for (int32 Index : Entry.Value)
				{
					const auto* Section = Split.Sections[Index];
					const auto* Parent = Section->GetParentMegaMesh();
					if (!Parent) return false;
					const FVector World = Parent->GetActorTransform().TransformPosition(Entry.Key);
					const auto& Transform = Section->GetActorTransform();
					const FVector Encoded = Transform.TransformPosition(FVector(FVector3f(Transform.InverseTransformPosition(World))));
					const FVector Canonical = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, Encoded);
					if (!Decoded[Index].Contains(Canonical)) return false;
					if (HasFirst)
					{
						if (Capture) { ++ComparedPairs; MaxGap = FMath::Max(MaxGap, FVector::Distance(First, Canonical)); }
						if (Canonical != First) return false;
						if (Capture && FaultSection == INDEX_NONE) { FaultSection = Index; FaultPoint = Canonical; }
					}
					else { First = Canonical; HasFirst = true; }
				}
				if (Capture) ++Shared;
			}
			return true;
		};
		const bool Consistent = Check(Actual, true);
		Receipt.SetNumberField(TEXT("native_section_count"), Split.Sections.Num());
		Receipt.SetNumberField(TEXT("native_diagnostic_cell_size_cm"), 3000);
		Receipt.SetNumberField(TEXT("shared_source_stations"), RequiredShared);
		Receipt.SetNumberField(TEXT("complete_decoded_shared_stations"), Shared);
		Receipt.SetNumberField(TEXT("compared_shared_owner_pairs"), ComparedPairs);
		if (ComparedPairs > 0) Receipt.SetNumberField(TEXT("maximum_decoded_shared_gap_m"), MaxGap);
		else Receipt.SetField(TEXT("maximum_decoded_shared_gap_m"), MakeShared<FJsonValueNull>());
		if (!Test.TestTrue(TEXT("Every shared source station occurs with the exact same actual decoded XYZ in all owning sections."),
			Consistent && RequiredShared > 0 && Shared == RequiredShared && FaultSection != INDEX_NONE)) return false;
		auto Fault = Actual; int32 Changed = 0;
		for (auto& Face : Fault[FaultSection]) for (auto& Point : Face.Points)
			if (Point == FaultPoint) { Point.Z += .001; ++Changed; }
		return Test.TestTrue(TEXT("A genuine 1mm gap in one actual decoded field refuses shared continuity."), Changed > 0 && !Check(Fault, false));
	}
}
#endif
