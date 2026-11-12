#include "ProjectWorldMeshTerrainStorageTransformer.h"
#include "MeshPartition.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionDependencyInterface.h"
#include "MeshPartitionMeshData.h"
#include "Tasks/Task.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldTerrainStorage, Log, All);

namespace
{
	using namespace UE::MeshPartition;
	struct FSectionFrame
	{
		ACompiledSection* Section = nullptr;
		FTransform ParentToWorld, PreviousToWorld, StoredToWorld;
		FVector Pivot = FVector::ZeroVector;
		TArray<int32> Units;
	};

	bool PrepareAndApply(FTransformerContext& Context)
	{
		check(IsInGameThread());
		if (Context.bWasCancelled) return false;
		const auto Refuse = [&Context](const TCHAR* Reason)
		{
			// Native pipeline tasks do not propagate Execute's return value.
			Context.bWasCancelled = true;
			UE_LOG(LogProjectWorldTerrainStorage, Error, TEXT("[SectionStorage] refused=%s"), Reason);
			return false;
		};
		if (Context.TransformerUnits.IsEmpty()) return Refuse(TEXT("missing_units"));
		TArray<FSectionFrame> Frames;
		TMap<ACompiledSection*, int32> Owners;
		for (int32 Index = 0; Index < Context.TransformerUnits.Num(); ++Index)
		{
			const auto& Unit = Context.TransformerUnits[Index];
			auto* Section = Cast<ACompiledSection>(Unit.Section.Get());
			const AMeshPartition* Parent = IsValid(Section) ? Section->GetParentMegaMesh() : nullptr;
			if (!IsValid(Parent) || !Section->GetRootComponent() || !Unit.MeshData || Unit.MeshData->VertexCount() == 0)
				return Refuse(TEXT("unknown_section_parent_or_mesh"));
			int32* Owner = Owners.Find(Section);
			if (!Owner)
			{
				const int32 Added = Frames.AddDefaulted();
				Owners.Add(Section, Added);
				auto& Frame = Frames[Added];
				Frame.Section = Section;
				Frame.ParentToWorld = Parent->GetActorTransform();
				Frame.PreviousToWorld = Section->GetActorTransform();
				Owner = Owners.Find(Section);
			}
			Frames[*Owner].Units.Add(Index);
		}

		TArray<TSharedPtr<const FMeshData>> Copies;
		Copies.SetNum(Context.TransformerUnits.Num());
		for (auto& Frame : Frames)
		{
			FBox Bounds(ForceInit);
			for (int32 Index : Frame.Units)
				Bounds += FBox(Context.TransformerUnits[Index].MeshData->GetBounds(false));
			if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() || Frame.ParentToWorld.ContainsNaN())
				return Refuse(TEXT("nonfinite_source_frame"));
			Frame.Pivot = Bounds.GetCenter();
			Frame.Pivot.Z = 0;
			Frame.StoredToWorld = FTransform(Frame.Pivot) * Frame.ParentToWorld;
			for (int32 Index : Frame.Units)
			{
				const auto& Source = *Context.TransformerUnits[Index].MeshData;
				auto Copy = MakeShared<FMeshData>();
				Copy->Copy(Source);
				for (int32 Vertex : Source.VertexIndicesItr())
				{
					const FVector Position = Source.GetVertex(Vertex);
					const FVector Stored = Position - Frame.Pivot;
					// Fresh native builds supply parent-local data, including on section reuse.
					if (Position.ContainsNaN()) return Refuse(TEXT("nonfinite_source_position"));
					if (!Frame.ParentToWorld.TransformPosition(Position).Equals(
						Frame.StoredToWorld.TransformPosition(Stored), 1.e-6))
						return Refuse(TEXT("source_world_position_changed"));
					Copy->SetVertex(Vertex, Stored);
				}
				Copies[Index] = MoveTemp(Copy);
			}
		}

		int32 Changed = 0;
		for (const auto& Frame : Frames)
		{
			if (!Frame.Section->SetActorTransform(Frame.StoredToWorld, false, nullptr, ETeleportType::TeleportPhysics) ||
				!Frame.Section->GetActorTransform().Equals(Frame.StoredToWorld, 1.e-6))
			{
				for (int32 Index = 0; Index <= Changed; ++Index)
					Frames[Index].Section->SetActorTransform(Frames[Index].PreviousToWorld, false, nullptr, ETeleportType::TeleportPhysics);
				return Refuse(TEXT("section_frame_update_failed"));
			}
			++Changed;
		}
		for (int32 Index = 0; Index < Copies.Num(); ++Index)
			Context.TransformerUnits[Index].MeshData = MoveTemp(Copies[Index]);
		for (const auto& Frame : Frames)
			UE_LOG(LogProjectWorldTerrainStorage, Display, TEXT("[SectionStorage] section=%s units=%d pivot_parent_cm=%s"),
				*Frame.Section->GetPathName(), Frame.Units.Num(), *Frame.Pivot.ToString());
		return true;
	}
}

bool FProjectWorldMeshTerrainStorageTransformer::Execute(UE::MeshPartition::FTransformerContext& Context) const
{
	if (IsInGameThread()) return PrepareAndApply(Context);
	bool Result = false;
	UE::Tasks::FTask Dummy;
	auto Task = UE::Tasks::Launch(TEXT("ProjectTerrainSectionStorage"), [&Context, &Result]
	{
		Result = PrepareAndApply(Context);
	}, UE::Tasks::Prerequisites(Dummy), UE::Tasks::ETaskPriority::Normal, UE::Tasks::EExtendedTaskPriority::GameThreadNormalPri);
	Task.Wait();
	return Result;
}

void FProjectWorldMeshTerrainStorageTransformer::GatherDependencies(UE::MeshPartition::IDependencyInterface& Dependencies) const
{
	Dependencies += FString(TEXT("project_terrain_parent_space_section_storage_v1"));
}
