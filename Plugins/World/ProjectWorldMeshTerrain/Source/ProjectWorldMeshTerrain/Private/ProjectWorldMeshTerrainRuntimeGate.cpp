#include "ProjectWorldMeshTerrainRuntimeGate.h"

#include "ProjectWorldTerrainRuntimeRole.h"

#include "ProjectWorldMeshTerrainTransformer.h"

#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WorldPartition/WorldPartition.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldMeshTerrainRuntimeGate, Log, All);

namespace
{
	constexpr double GateTimeoutSeconds = 120.0;
	constexpr double PhaseTimeoutSeconds = 30.0;
	constexpr double MaximumSourceRadiusCentimeters = 1000000.0;

	bool IsSupportedGeneratedMapPackage(const FString& MapPackage)
	{
		return MapPackage.StartsWith(TEXT("/ProjectWorldTestData/Generated/")) ||
			MapPackage.StartsWith(TEXT("/ProjectWorldData/Generated/"));
	}

	bool ParseRequiredValue(const TCHAR* Name, FString& OutValue)
	{
		return FParse::Value(FCommandLine::Get(), Name, OutValue, true) && !OutValue.IsEmpty();
	}

	bool ParseRequiredNumber(const TCHAR* Name, double& OutValue)
	{
		FString Text;
		return ParseRequiredValue(Name, Text) && LexTryParseString(OutValue, *Text) && FMath::IsFinite(OutValue);
	}

	bool IsSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}
		return true;
	}

	FName ReadBuildVariant(const AActor* Section)
	{
		const FStructProperty* BuildInfoProperty =
			FindFProperty<FStructProperty>(Section->GetClass(), TEXT("BuildInfo"));
		const FNameProperty* VariantProperty = BuildInfoProperty != nullptr
			? FindFProperty<FNameProperty>(BuildInfoProperty->Struct, TEXT("BuildVariantName"))
			: nullptr;
		if (VariantProperty == nullptr)
		{
			return NAME_None;
		}
		const void* BuildInfo = BuildInfoProperty->ContainerPtrToValuePtr<void>(Section);
		return VariantProperty->GetPropertyValue_InContainer(BuildInfo);
	}
}

FProjectWorldMeshTerrainRuntimeGate::~FProjectWorldMeshTerrainRuntimeGate()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}
}

void FProjectWorldMeshTerrainRuntimeGate::StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldMeshTerrainRuntimeGate")))
	{
		return;
	}
	FString Error;
	if (!ParseConfig(Error))
	{
		FinishRejected(TEXT("runtime_gate_config_invalid"), Error);
		return;
	}
	StartedSeconds = FPlatformTime::Seconds();
	PhaseStartedSeconds = StartedSeconds;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FProjectWorldMeshTerrainRuntimeGate::Tick));
}

bool FProjectWorldMeshTerrainRuntimeGate::ParseConfig(FString& OutError)
{
	double CenterX = 0.0;
	double CenterY = 0.0;
	double EdgeX = 0.0;
	double EdgeY = 0.0;
	double ProbeZ = 0.0;
	double Radius = 0.0;
	if (!ParseRequiredValue(TEXT("ProjectWorldMeshTerrainMap="), MapPackage) ||
		!ParseRequiredValue(TEXT("ProjectWorldMeshTerrainResult="), ResultPath) ||
		!ParseRequiredValue(TEXT("ProjectWorldMeshTerrainSourceIdentity="), SourceIdentity) ||
		!ParseRequiredValue(TEXT("ProjectWorldMeshTerrainProbeDerivation="), ProbeDerivationSha256) ||
		!ParseRequiredNumber(TEXT("ProjectWorldMeshTerrainCenterX="), CenterX) ||
		!ParseRequiredNumber(TEXT("ProjectWorldMeshTerrainCenterY="), CenterY) ||
		!ParseRequiredNumber(TEXT("ProjectWorldMeshTerrainEdgeX="), EdgeX) ||
		!ParseRequiredNumber(TEXT("ProjectWorldMeshTerrainEdgeY="), EdgeY) ||
		!ParseRequiredNumber(TEXT("ProjectWorldMeshTerrainProbeZ="), ProbeZ) ||
		!ParseRequiredNumber(TEXT("ProjectWorldMeshTerrainRadiusCm="), Radius))
	{
		OutError = TEXT("Mesh Terrain runtime gate requires map, result, source identity, probe derivation, center, edge, probe Z, and radius values.");
		return false;
	}
	ResultPath = FPaths::ConvertRelativePathToFull(ResultPath);
	const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization")));
	if (!IsSupportedGeneratedMapPackage(MapPackage) ||
		!FPaths::IsUnderDirectory(ResultPath, EvidenceRoot) ||
		!IsSha256(SourceIdentity) || !IsSha256(ProbeDerivationSha256) ||
		Radius <= 0.0 || Radius > MaximumSourceRadiusCentimeters)
	{
		OutError = TEXT("Mesh Terrain runtime gate is confined to a ProjectWorld generated map and validation output.");
		return false;
	}
	Center = FVector(CenterX, CenterY, ProbeZ);
	Edge = FVector(EdgeX, EdgeY, ProbeZ);
	SourceRadiusCentimeters = static_cast<float>(Radius);
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainRuntimeMapBoundaryTest,
	"Project.World.Realization.MeshTerrain.RuntimeMapBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainRuntimeMapBoundaryTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Synthetic generated maps remain supported"),
		IsSupportedGeneratedMapPackage(
			TEXT("/ProjectWorldTestData/Generated/MeshTerrain/L_MeshTerrain")));
	TestTrue(TEXT("Production generated maps are supported"),
		IsSupportedGeneratedMapPackage(
			TEXT("/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory")));
	TestFalse(TEXT("Unowned game maps remain rejected"),
		IsSupportedGeneratedMapPackage(TEXT("/Game/Maps/L_Unowned")));
	TestFalse(TEXT("Generated-root prefix lookalikes remain rejected"),
		IsSupportedGeneratedMapPackage(TEXT("/ProjectWorldData/GeneratedOutside/L_Unowned")));
	TestTrue(TEXT("Authenticated source identities accept lowercase SHA-256"),
		IsSha256(TEXT("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")));
	TestFalse(TEXT("Truncated source identities remain rejected"), IsSha256(TEXT("0123456789abcdef")));
	TestFalse(TEXT("Non-hex source identities remain rejected"),
		IsSha256(TEXT("z123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")));
	return true;
}

#endif

bool FProjectWorldMeshTerrainRuntimeGate::Tick(float DeltaSeconds)
{
	if (Phase == EPhase::Finished)
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - StartedSeconds > GateTimeoutSeconds)
	{
		FinishRejected(TEXT("runtime_gate_timeout"), TEXT("Mesh Terrain runtime proof exceeded its total timeout."));
		return false;
	}
	if (Phase == EPhase::WaitingForWorld)
	{
		if (TryAcquireWorld())
		{
			FString Error;
			if (!CreateStreamingSource(Error))
			{
				FinishRejected(TEXT("runtime_gate_source_failed"), Error);
				return false;
			}
			MoveSource(Center, EPhase::WaitingAtCenter);
		}
		return true;
	}
	DisablePlayerStreamingSources();
	++PhaseFrames;
	const bool bPhaseTimedOut = Now - PhaseStartedSeconds > PhaseTimeoutSeconds;
	if (PhaseFrames < 3 && !bPhaseTimedOut)
	{
		return true;
	}
	UWorldPartitionStreamingSourceComponent* Source = StreamingSource.Get();
	bStreamingSourceEnabled = Source != nullptr && Source->IsStreamingSourceEnabled();
	if (!bStreamingSourceEnabled)
	{
		if (bPhaseTimedOut)
		{
			FinishRejected(TEXT("streaming_source_disabled"), TEXT("The test-owned World Partition streaming source is not enabled."));
			return false;
		}
		return true;
	}
	bSourceStreamingCompleted = Source->IsStreamingCompleted();
	if (!bSourceStreamingCompleted)
	{
		if (bPhaseTimedOut)
		{
			FinishRejected(TEXT("streaming_not_completed"), TEXT("UE did not report streaming completion for the test-owned source."));
			return false;
		}
		return true;
	}

	const FVector& ProbePoint = Phase == EPhase::WaitingAtEdge ? Edge : Center;
	LastProbe = FProbeDiagnostic();
	LastProbe.ProofPoint = PhaseName(Phase);
	InspectLoadedTerrainSectionsAround(ProbePoint, LastProbe);
	if (LastProbe.LoadedTaggedSectionCount == 0)
	{
		if (bPhaseTimedOut)
		{
			FinishRejected(TEXT("terrain_section_not_loaded"), TEXT("Streaming completed but no tagged Mesh Terrain section is loaded around the probe."));
			return false;
		}
		return true;
	}
	if (LastProbe.RenderableStaticMeshComponentCount == 0)
	{
		if (bPhaseTimedOut)
		{
			FinishRejected(TEXT("terrain_render_section_not_loaded"), TEXT("Streaming completed and collision terrain is loaded, but no renderable Mesh Terrain section is loaded around the probe."));
			return false;
		}
		return true;
	}

	FString Marker;
	const EProbeOutcome ProbeOutcome = ProbeTerrain(ProbePoint, Marker, LastProbe);
	if (ProbeOutcome != EProbeOutcome::TaggedTerrainHit)
	{
		if (bPhaseTimedOut)
		{
			const FString Code = ProbeOutcome == EProbeOutcome::NoHit
				? TEXT("collision_no_hit")
				: TEXT("collision_hit_untagged");
			const FString Message = ProbeOutcome == EProbeOutcome::NoHit
				? TEXT("Streaming completed and tagged terrain is loaded, but the collision trace returned no blocking hit.")
				: TEXT("Streaming completed and tagged terrain is loaded, but the collision trace hit only untagged geometry.");
			FinishRejected(Code, Message);
			return false;
		}
		return true;
	}

	if (Phase == EPhase::WaitingAtCenter)
	{
		bCenterCollision = true;
		CenterMarker = Marker;
		MoveSource(Edge, EPhase::WaitingAtEdge);
		return true;
	}
	if (Phase == EPhase::WaitingAtEdge)
	{
		bCenterUnloadedAtEdge = !HasTerrainMarker(CenterMarker);
		bEdgeCollision = true;
		EdgeMarker = Marker;
		if (!bCenterUnloadedAtEdge || EdgeMarker == CenterMarker)
		{
			if (bPhaseTimedOut)
			{
				FinishRejected(TEXT("terrain_section_not_unloaded"), TEXT("Streaming completed at the edge, but the center Mesh Terrain section remained loaded."));
				return false;
			}
			return true;
		}
		MoveSource(Center, EPhase::WaitingAtCenterReturn);
		return true;
	}
	bCenterReloaded = HasTerrainMarker(CenterMarker);
	if (!bCenterReloaded || Marker != CenterMarker)
	{
		if (bPhaseTimedOut)
		{
			FinishRejected(TEXT("terrain_section_not_reloaded"), TEXT("Streaming completed after return, but the original center Mesh Terrain section did not reload."));
			return false;
		}
		return true;
	}
	FinishAccepted();
	return false;
}

bool FProjectWorldMeshTerrainRuntimeGate::TryAcquireWorld()
{
	if (GEngine == nullptr)
	{
		return false;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* Candidate = Context.World();
		if (Candidate != nullptr && Candidate->IsGameWorld() &&
			Candidate->GetPackage()->GetName() == MapPackage)
		{
			World = Candidate;
			return true;
		}
	}
	return false;
}

bool FProjectWorldMeshTerrainRuntimeGate::CreateStreamingSource(FString& OutError)
{
	UWorld* ActiveWorld = World.Get();
	if (ActiveWorld == nullptr || ActiveWorld->GetWorldPartition() == nullptr)
	{
		OutError = TEXT("Mesh Terrain runtime map has no World Partition.");
		return false;
	}
	DisablePlayerStreamingSources();
	AActor* Actor = ActiveWorld->SpawnActor<AActor>();
	if (Actor == nullptr)
	{
		OutError = TEXT("Cannot create the test-owned World Partition streaming source.");
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Actor);
	if (Root == nullptr)
	{
		Actor->Destroy();
		OutError = TEXT("Cannot create the streaming source root component.");
		return false;
	}
	Actor->SetRootComponent(Root);
	Actor->AddInstanceComponent(Root);
	Root->RegisterComponent();
	UWorldPartitionStreamingSourceComponent* Component =
		NewObject<UWorldPartitionStreamingSourceComponent>(Actor);
	if (Component == nullptr)
	{
		Actor->Destroy();
		OutError = TEXT("Cannot create the World Partition streaming source component.");
		return false;
	}
	FStreamingSourceShape Shape;
	Shape.bUseGridLoadingRange = false;
	Shape.Radius = SourceRadiusCentimeters;
	Component->Shapes = {Shape};
	Component->TargetState = EStreamingSourceTargetState::Activated;
	Actor->AddInstanceComponent(Component);
	Component->RegisterComponent();
	SourceActor = Actor;
	StreamingSource = Component;
	return true;
}

void FProjectWorldMeshTerrainRuntimeGate::DisablePlayerStreamingSources() const
{
	UWorld* ActiveWorld = World.Get();
	if (ActiveWorld == nullptr)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = ActiveWorld->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Controller = It->Get())
		{
			Controller->bEnableStreamingSource = false;
		}
	}
}

void FProjectWorldMeshTerrainRuntimeGate::InspectLoadedTerrainSectionsAround(
	const FVector& Point,
	FProbeDiagnostic& OutDiagnostic) const
{
	UWorld* ActiveWorld = World.Get();
	if (ActiveWorld == nullptr)
	{
		return;
	}
	for (TActorIterator<AActor> It(ActiveWorld); It; ++It)
	{
		if (!ProjectWorldTerrainRuntimeRole::HasRole(**It))
		{
			continue;
		}
		FVector Origin;
		FVector Extent;
		It->GetActorBounds(false, Origin, Extent, true);
		if (FBox(Origin - Extent, Origin + Extent).ComputeSquaredDistanceToPoint(Point) <=
			FMath::Square(static_cast<double>(SourceRadiusCentimeters)))
		{
			++OutDiagnostic.LoadedTaggedSectionCount;
			static const UClass* CompiledSectionClass =
				LoadClass<AActor>(nullptr, TEXT("/Script/MeshPartition.CompiledSection"));
			if (CompiledSectionClass == nullptr || !It->IsA(CompiledSectionClass))
			{
				continue;
			}
			++OutDiagnostic.LoadedCompiledSectionCount;
			OutDiagnostic.BuildVariants.AddUnique(ReadBuildVariant(*It).ToString());
			TInlineComponentArray<UStaticMeshComponent*> Components(*It);
			for (const UStaticMeshComponent* Component : Components)
			{
				if (Component == nullptr || Component->GetStaticMesh() == nullptr)
				{
					continue;
				}
				++OutDiagnostic.StaticMeshComponentCount;
				const bool bMainPassRenderable = Component->IsRegistered() &&
					Component->IsVisible() && !Component->bHiddenInGame &&
					Component->ShouldRender() && Component->bRenderInMainPass;
				if (!Component->bRenderInMainPass)
				{
					++OutDiagnostic.NonMainPassStaticMeshComponentCount;
				}
				if (bMainPassRenderable)
				{
					++OutDiagnostic.RenderableStaticMeshComponentCount;
					if (Component->GetStaticMesh()->HasValidNaniteData())
					{
						++OutDiagnostic.NaniteStaticMeshComponentCount;
					}
				}
				const UMaterialInterface* Material = Component->GetMaterial(0);
				const UMaterialInstance* SectionInstance = Cast<UMaterialInstance>(Material);
				if (SectionInstance != nullptr && Material->GetName().StartsWith(TEXT("CompiledSectionMIC")))
				{
					++OutDiagnostic.SectionMaterialInstanceCount;
					Material = SectionInstance->Parent;
				}
				FString Chain;
				for (; Material != nullptr;)
				{
					if (!Chain.IsEmpty())
					{
						Chain += TEXT(" -> ");
					}
					Chain += Material->GetPathName();
					const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
					Material = Instance != nullptr ? Instance->Parent : nullptr;
				}
				OutDiagnostic.MaterialChains.AddUnique(Chain);
			}
		}
	}
}

FProjectWorldMeshTerrainRuntimeGate::EProbeOutcome FProjectWorldMeshTerrainRuntimeGate::ProbeTerrain(
	const FVector& Point,
	FString& OutMarker,
	FProbeDiagnostic& OutDiagnostic) const
{
	UWorld* ActiveWorld = World.Get();
	if (ActiveWorld == nullptr)
	{
		OutDiagnostic.Outcome = EProbeOutcome::NoHit;
		return OutDiagnostic.Outcome;
	}
	const FVector TraceStart = Point + FVector(0.0, 0.0, 100000.0);
	const FVector TraceEnd = Point - FVector(0.0, 0.0, 100000.0);
	FCollisionQueryParams Query(TEXT("ProjectWorldMeshTerrainRuntimeGate"), true);
	FHitResult RelevantHit;
	bool bSawUntaggedHit = false;
	for (int32 TraceIndex = 0; TraceIndex < 1024; ++TraceIndex)
	{
		FHitResult Candidate;
		if (!ActiveWorld->LineTraceSingleByChannel(
			Candidate, TraceStart, TraceEnd, ECC_Visibility, Query))
		{
			if (!bSawUntaggedHit)
			{
				OutDiagnostic.Outcome = EProbeOutcome::NoHit;
				return OutDiagnostic.Outcome;
			}
			break;
		}
		RelevantHit = Candidate;
		const AActor* Actor = Candidate.GetActor();
		if (Actor != nullptr && ProjectWorldTerrainRuntimeRole::HasRole(*Actor))
		{
			OutMarker = Actor->GetPackage()->GetName();
			OutDiagnostic.Outcome = EProbeOutcome::TaggedTerrainHit;
			break;
		}
		if (Actor == nullptr)
		{
			break;
		}
		bSawUntaggedHit = true;
		Query.AddIgnoredActor(Actor);
	}
	if (OutDiagnostic.Outcome != EProbeOutcome::TaggedTerrainHit)
	{
		OutDiagnostic.Outcome = EProbeOutcome::UntaggedHit;
	}
	if (const AActor* Actor = RelevantHit.GetActor())
	{
		OutDiagnostic.ActorClass = Actor->GetClass()->GetPathName();
		OutDiagnostic.ActorPackage = Actor->GetPackage()->GetName();
		OutDiagnostic.ActorTags = Actor->Tags;
	}
	if (const UPrimitiveComponent* Component = RelevantHit.GetComponent())
	{
		OutDiagnostic.Component = Component->GetPathName();
	}
	OutDiagnostic.ImpactZ = RelevantHit.ImpactPoint.Z;
	OutDiagnostic.bHasImpact = true;
	return OutDiagnostic.Outcome;
}

bool FProjectWorldMeshTerrainRuntimeGate::HasTerrainMarker(const FString& Marker) const
{
	UWorld* ActiveWorld = World.Get();
	if (ActiveWorld == nullptr || Marker.IsEmpty())
	{
		return false;
	}
	for (TActorIterator<AActor> It(ActiveWorld); It; ++It)
	{
		if (ProjectWorldTerrainRuntimeRole::HasRole(**It) &&
			It->GetPackage()->GetName() == Marker)
		{
			return true;
		}
	}
	return false;
}

void FProjectWorldMeshTerrainRuntimeGate::MoveSource(const FVector& Point, EPhase NewPhase)
{
	SourceActor->SetActorLocation(Point, false, nullptr, ETeleportType::TeleportPhysics);
	Phase = NewPhase;
	PhaseFrames = 0;
	PhaseStartedSeconds = FPlatformTime::Seconds();
	bStreamingSourceEnabled = false;
	bSourceStreamingCompleted = false;
	LastProbe = FProbeDiagnostic();
	LastProbe.ProofPoint = PhaseName(NewPhase);
}

const TCHAR* FProjectWorldMeshTerrainRuntimeGate::ProbeOutcomeName(EProbeOutcome Outcome)
{
	switch (Outcome)
	{
	case EProbeOutcome::NoHit: return TEXT("no_hit");
	case EProbeOutcome::UntaggedHit: return TEXT("untagged_hit");
	case EProbeOutcome::TaggedTerrainHit: return TEXT("tagged_terrain_hit");
	default: return TEXT("not_run");
	}
}

const TCHAR* FProjectWorldMeshTerrainRuntimeGate::PhaseName(EPhase Value)
{
	switch (Value)
	{
	case EPhase::WaitingAtCenter: return TEXT("center");
	case EPhase::WaitingAtEdge: return TEXT("edge");
	case EPhase::WaitingAtCenterReturn: return TEXT("center_return");
	case EPhase::WaitingForWorld: return TEXT("world");
	default: return TEXT("finished");
	}
}

void FProjectWorldMeshTerrainRuntimeGate::FinishAccepted()
{
	Phase = EPhase::Finished;
	WriteResult(TEXT("accepted"), TEXT("none"), TEXT(""));
	UE_LOG(LogProjectWorldMeshTerrainRuntimeGate, Display,
		TEXT("Mesh Terrain runtime gate accepted: center=%s edge=%s"), *CenterMarker, *EdgeMarker);
	FPlatformMisc::RequestExitWithStatus(false, 0, TEXT("ProjectWorldMeshTerrainRuntimeGate.Accepted"));
}

void FProjectWorldMeshTerrainRuntimeGate::FinishRejected(const FString& Code, const FString& Message)
{
	Phase = EPhase::Finished;
	WriteResult(TEXT("rejected"), Code, Message);
	UE_LOG(LogProjectWorldMeshTerrainRuntimeGate, Error,
		TEXT("Mesh Terrain runtime gate rejected: %s - %s"), *Code, *Message);
	FPlatformMisc::RequestExitWithStatus(false, 9, TEXT("ProjectWorldMeshTerrainRuntimeGate.Rejected"));
}

void FProjectWorldMeshTerrainRuntimeGate::WriteResult(
	const FString& Status,
	const FString& Code,
	const FString& Message) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("project-world-mesh-terrain-runtime-gate:v2"));
	Root->SetStringField(TEXT("status"), Status);
	Root->SetStringField(TEXT("code"), Code);
	Root->SetStringField(TEXT("message"), Message);
	Root->SetStringField(TEXT("map"), MapPackage);
	Root->SetStringField(TEXT("source_identity_sha256"), SourceIdentity);
	Root->SetStringField(TEXT("probe_derivation_sha256"), ProbeDerivationSha256);
	Root->SetStringField(TEXT("proof_point"), LastProbe.ProofPoint);
	Root->SetBoolField(TEXT("streaming_source_enabled"), bStreamingSourceEnabled);
	Root->SetBoolField(TEXT("source_streaming_completed"), bSourceStreamingCompleted);
	Root->SetNumberField(TEXT("loaded_tagged_section_count"), LastProbe.LoadedTaggedSectionCount);
	Root->SetNumberField(TEXT("loaded_compiled_section_count"), LastProbe.LoadedCompiledSectionCount);
	Root->SetNumberField(TEXT("static_mesh_component_count"), LastProbe.StaticMeshComponentCount);
	Root->SetNumberField(TEXT("renderable_static_mesh_component_count"), LastProbe.RenderableStaticMeshComponentCount);
	Root->SetNumberField(TEXT("non_main_pass_static_mesh_component_count"), LastProbe.NonMainPassStaticMeshComponentCount);
	Root->SetNumberField(TEXT("nanite_static_mesh_component_count"), LastProbe.NaniteStaticMeshComponentCount);
	Root->SetNumberField(TEXT("section_material_instance_count"), LastProbe.SectionMaterialInstanceCount);
	TArray<TSharedPtr<FJsonValue>> BuildVariants;
	for (const FString& BuildVariant : LastProbe.BuildVariants)
	{
		BuildVariants.Add(MakeShared<FJsonValueString>(BuildVariant));
	}
	Root->SetArrayField(TEXT("loaded_build_variants"), BuildVariants);
	TArray<TSharedPtr<FJsonValue>> MaterialChains;
	for (const FString& MaterialChain : LastProbe.MaterialChains)
	{
		MaterialChains.Add(MakeShared<FJsonValueString>(MaterialChain));
	}
	Root->SetArrayField(TEXT("material_chains"), MaterialChains);
	Root->SetStringField(TEXT("trace_outcome"), ProbeOutcomeName(LastProbe.Outcome));
	Root->SetStringField(TEXT("trace_hit_actor_class"), LastProbe.ActorClass);
	Root->SetStringField(TEXT("trace_hit_actor_package"), LastProbe.ActorPackage);
	Root->SetStringField(TEXT("trace_hit_component"), LastProbe.Component);
	Root->SetBoolField(TEXT("trace_has_impact"), LastProbe.bHasImpact);
	Root->SetNumberField(TEXT("trace_impact_z"), LastProbe.ImpactZ);
	TArray<TSharedPtr<FJsonValue>> TraceActorTags;
	for (const FName Tag : LastProbe.ActorTags)
	{
		TraceActorTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Root->SetArrayField(TEXT("trace_hit_actor_tags"), TraceActorTags);
	Root->SetArrayField(TEXT("center_unreal_cm"), {
		MakeShared<FJsonValueNumber>(Center.X), MakeShared<FJsonValueNumber>(Center.Y), MakeShared<FJsonValueNumber>(Center.Z)});
	Root->SetArrayField(TEXT("edge_unreal_cm"), {
		MakeShared<FJsonValueNumber>(Edge.X), MakeShared<FJsonValueNumber>(Edge.Y), MakeShared<FJsonValueNumber>(Edge.Z)});
	Root->SetNumberField(TEXT("source_radius_cm"), SourceRadiusCentimeters);
	Root->SetStringField(TEXT("center_marker"), CenterMarker);
	Root->SetStringField(TEXT("edge_marker"), EdgeMarker);
	Root->SetBoolField(TEXT("center_collision"), bCenterCollision);
	Root->SetBoolField(TEXT("edge_collision"), bEdgeCollision);
	Root->SetBoolField(TEXT("center_unloaded_at_edge"), bCenterUnloadedAtEdge);
	Root->SetBoolField(TEXT("center_reloaded"), bCenterReloaded);
	Root->SetNumberField(TEXT("duration_seconds"), FPlatformTime::Seconds() - StartedSeconds);
	UWorld* ActiveWorld = World.Get();
	UWorldPartition* WorldPartition = ActiveWorld != nullptr ? ActiveWorld->GetWorldPartition() : nullptr;
	Root->SetBoolField(
		TEXT("world_partition_streaming_enabled"),
		WorldPartition != nullptr && WorldPartition->IsStreamingEnabled());
	auto AddMarkerState = [ActiveWorld, &Root](const TCHAR* Prefix, const FString& Marker)
	{
		const AActor* Match = nullptr;
		if (ActiveWorld != nullptr)
		{
			for (TActorIterator<AActor> It(ActiveWorld); It; ++It)
			{
				if (ProjectWorldTerrainRuntimeRole::HasRole(**It) &&
					It->GetPackage()->GetName() == Marker)
				{
					Match = *It;
					break;
				}
			}
		}
		Root->SetBoolField(FString(Prefix) + TEXT("_loaded"), Match != nullptr);
	};
	AddMarkerState(TEXT("center"), CenterMarker);
	AddMarkerState(TEXT("edge"), EdgeMarker);
	TArray<TSharedPtr<FJsonValue>> Sources;
	if (WorldPartition != nullptr)
	{
		for (const FWorldPartitionStreamingSource& Source : WorldPartition->GetStreamingSources())
		{
			TSharedRef<FJsonObject> SourceObject = MakeShared<FJsonObject>();
			SourceObject->SetStringField(TEXT("name"), Source.Name.ToString());
			SourceObject->SetNumberField(TEXT("x"), Source.Location.X);
			SourceObject->SetNumberField(TEXT("y"), Source.Location.Y);
			SourceObject->SetNumberField(TEXT("z"), Source.Location.Z);
			SourceObject->SetNumberField(TEXT("shape_count"), Source.Shapes.Num());
			if (!Source.Shapes.IsEmpty())
			{
				SourceObject->SetBoolField(
					TEXT("shape_uses_grid_loading_range"), Source.Shapes[0].bUseGridLoadingRange);
				SourceObject->SetNumberField(TEXT("shape_radius"), Source.Shapes[0].Radius);
			}
			Sources.Add(MakeShared<FJsonValueObject>(SourceObject));
		}
	}
	Root->SetArrayField(TEXT("streaming_sources"), Sources);
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
	const FString Staging = ResultPath + TEXT(".staging");
	if (FFileHelper::SaveStringToFile(
		Json + LINE_TERMINATOR, *Staging, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		IFileManager::Get().Move(*ResultPath, *Staging, true, true, false, false);
	}
}
