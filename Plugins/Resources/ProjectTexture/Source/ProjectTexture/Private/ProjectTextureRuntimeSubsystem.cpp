// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureRuntimeSubsystem.h"

#include "ProjectTextureRuntimeCache.h"
#include "ProjectTextureRuntimeCatalog.h"

#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "RenderingThread.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectTextureRuntime, Log, All);

namespace ProjectTextureRuntimePrivate
{
bool IsGenerationWorld(const UWorld* World)
{
	return World != nullptr &&
		(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE ||
			World->WorldType == EWorldType::GamePreview);
}

int64 TextureBytes(const FProjectTextureRuntimeNodeDescriptor& Node)
{
	if (Node.Width <= 0 || Node.Height <= 0 || Node.Format != RTF_RGBA8)
	{
		return 0;
	}
	int64 Bytes = 0;
	int32 Width = Node.Width;
	int32 Height = Node.Height;
	while (true)
	{
		Bytes += static_cast<int64>(Width) * Height * 4;
		if (!Node.bGenerateMips || (Width == 1 && Height == 1))
		{
			break;
		}
		Width = FMath::Max(1, Width / 2);
		Height = FMath::Max(1, Height / 2);
	}
	return Bytes;
}

bool ConfigureTarget(
	UTextureRenderTarget2D* Target,
	const FProjectTextureRuntimeNodeDescriptor& Node,
	FString& OutError)
{
	if (Target == nullptr || TextureBytes(Node) <= 0)
	{
		OutError = TEXT("Runtime pattern nodes require a positive RGBA8 render target.");
		return false;
	}
	Target->RenderTargetFormat = Node.Format;
	Target->ClearColor = FLinearColor::Transparent;
	Target->bAutoGenerateMips = Node.bGenerateMips;
	Target->AddressX = TA_Wrap;
	Target->AddressY = TA_Wrap;
	Target->MipsAddressU = TA_Wrap;
	Target->MipsAddressV = TA_Wrap;
	Target->Filter = TF_Trilinear;
	Target->bForceLinearGamma = true;
	Target->SRGB = false;
	Target->InitAutoFormat(Node.Width, Node.Height);
	Target->UpdateResourceImmediate(true);
	if (Target->GetResource() == nullptr)
	{
		OutError = TEXT("Runtime texture render-target resource creation failed.");
		return false;
	}
	return true;
}
}

void FProjectTextureRuntimeCacheDeleter::operator()(FProjectTextureRuntimeCache* Cache) const
{
	delete Cache;
}

UProjectTextureRuntimeSubsystem::~UProjectTextureRuntimeSubsystem() = default;

bool UProjectTextureRuntimeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && FApp::CanEverRender() &&
		!IsRunningDedicatedServer();
}

void UProjectTextureRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Cache.Reset(new FProjectTextureRuntimeCache());
	WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &UProjectTextureRuntimeSubsystem::HandlePostWorldInitialization);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UProjectTextureRuntimeSubsystem::HandleWorldCleanup);
	for (TObjectIterator<UProjectTextureOutputSlot> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject))
		{
			QueueOrGenerate(*It);
		}
	}
}

void UProjectTextureRuntimeSubsystem::Deinitialize()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	ReleaseTransientTargets();
	PendingOutputs.Reset();
	ReadyOutputs.Reset();
	NodeGenerationCounts.Reset();
	if (Cache)
	{
		Cache->Reset();
		Cache.Reset();
	}
	Super::Deinitialize();
}

void UProjectTextureRuntimeSubsystem::NotifyOutputSlotLoaded(UProjectTextureOutputSlot* OutputSlot)
{
	if (OutputSlot == nullptr || GEngine == nullptr || !FApp::CanEverRender())
	{
		return;
	}
	if (UProjectTextureRuntimeSubsystem* Subsystem =
		GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>())
	{
		Subsystem->QueueOrGenerate(OutputSlot);
	}
}

void UProjectTextureRuntimeSubsystem::HandlePostWorldInitialization(
	UWorld* World,
	const UWorld::InitializationValues InitializationValues)
{
	if (!ProjectTextureRuntimePrivate::IsGenerationWorld(World))
	{
		return;
	}
	TArray<TObjectPtr<UProjectTextureOutputSlot>> Outputs = MoveTemp(PendingOutputs);
	PendingOutputs.Reset();
	for (UProjectTextureOutputSlot* Output : Outputs)
	{
		FString Error;
		if (!RequestOutput(World, Output, Error))
		{
			UE_LOG(LogProjectTextureRuntime, Error,
				TEXT("[RuntimeCache::Prewarm] Failed - slot=%s error=%s"),
				*GetPathNameSafe(Output), *Error);
		}
	}
}

void UProjectTextureRuntimeSubsystem::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
{
	if (ProjectTextureRuntimePrivate::IsGenerationWorld(World))
	{
		ReleaseTransientTargets();
	}
}

UWorld* UProjectTextureRuntimeSubsystem::FindGenerationWorld() const
{
	if (GEngine == nullptr)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (ProjectTextureRuntimePrivate::IsGenerationWorld(Context.World()))
		{
			return Context.World();
		}
	}
	return nullptr;
}

void UProjectTextureRuntimeSubsystem::QueueOrGenerate(UProjectTextureOutputSlot* OutputSlot)
{
	if (OutputSlot == nullptr || OutputSlot->Catalog == nullptr)
	{
		return;
	}
	if (UWorld* World = FindGenerationWorld())
	{
		FString Error;
		if (!RequestOutput(World, OutputSlot, Error))
		{
			UE_LOG(LogProjectTextureRuntime, Error,
				TEXT("[RuntimeCache::Request] Failed - slot=%s error=%s"),
				*OutputSlot->GetPathName(), *Error);
		}
		return;
	}
	PendingOutputs.AddUnique(OutputSlot);
}

bool UProjectTextureRuntimeSubsystem::RequestOutput(
	UObject* WorldContextObject,
	UProjectTextureOutputSlot* OutputSlot,
	FString& OutError)
{
	using namespace ProjectTextureRuntimePrivate;
	const double RequestStartedSeconds = FPlatformTime::Seconds();
	OutError.Reset();
	++Telemetry.Requests;
	if (!IsInGameThread() || OutputSlot == nullptr || OutputSlot->Catalog == nullptr ||
		OutputSlot->GetRenderTarget() == nullptr ||
		WorldContextObject == nullptr || Cache == nullptr)
	{
		OutError = TEXT("Runtime texture generation requires the game thread, a world, slot, and catalog.");
		++Telemetry.Failures;
		return false;
	}
	UProjectTextureRuntimeCatalog* Catalog = OutputSlot->Catalog;
	const FString OutputIdentity = Catalog->SemanticIdentity + TEXT(":") + OutputSlot->NodeId.ToString();
	if (ReadyOutputs.Contains(OutputIdentity))
	{
		++Telemetry.CacheHits;
		return true;
	}
	if (Catalog->SemanticIdentity.Len() != 64 || Catalog->Nodes.IsEmpty() ||
		OutputSlot->SlotName != TEXT("Structure") || OutputSlot->NodeId.IsNone())
	{
		OutError = TEXT("Runtime texture catalog or output-slot identity is invalid.");
		++Telemetry.Failures;
		return false;
	}
	Cache->SetBudgetBytes(Catalog->BudgetBytes);
	const int32 GenerationsBeforeCold = Telemetry.Generations;

	TMap<FName, UTextureRenderTarget2D*> NodeTargets;
	TSet<FName> SeenNodes;
	TArray<FString> AdmittedIdentities;
	for (const FProjectTextureRuntimeNodeDescriptor& Node : Catalog->Nodes)
	{
		if (Node.NodeId.IsNone() || Node.StructuralIdentity.Len() != 64 ||
			Node.GeneratorMaterial == nullptr || SeenNodes.Contains(Node.NodeId))
		{
			OutError = TEXT("Runtime texture catalog contains an invalid or duplicate node.");
			break;
		}
		for (const FName Parent : Node.ParentNodeIds)
		{
			if (!SeenNodes.Contains(Parent))
			{
				OutError = FString::Printf(
					TEXT("Runtime texture node %s has a missing or non-topological parent %s."),
					*Node.NodeId.ToString(), *Parent.ToString());
				break;
			}
		}
		if (!OutError.IsEmpty())
		{
			break;
		}

		const FString Identity = Catalog->SemanticIdentity + TEXT(":") + Node.StructuralIdentity;
		const int64 Bytes = TextureBytes(Node);
		const bool bExported = Node.NodeId == OutputSlot->NodeId;
		const EProjectTextureCacheRequest Request = Cache->Request(
			Identity, Bytes, bExported, OutError);
		if (Request == EProjectTextureCacheRequest::Rejected)
		{
			break;
		}
		if (Request == EProjectTextureCacheRequest::Pending)
		{
			OutError = FString::Printf(
				TEXT("Texture node %s is already being generated."), *Node.NodeId.ToString());
			break;
		}
		AdmittedIdentities.Add(Identity);

		UTextureRenderTarget2D* Target = nullptr;
		if (bExported)
		{
			Target = OutputSlot->GetRenderTarget();
		}
		else
		{
			Target = NewObject<UTextureRenderTarget2D>(this);
			TransientTargets.Add(Identity, Target);
		}
		if (!ConfigureTarget(Target, Node, OutError))
		{
			break;
		}

		UMaterialInterface* DrawMaterial = Node.GeneratorMaterial;
		if (Node.ParentNodeIds.Num() == 1)
		{
			UTextureRenderTarget2D* const* Parent = NodeTargets.Find(Node.ParentNodeIds[0]);
			if (Parent == nullptr || *Parent == nullptr || Node.ParentTextureParameter.IsNone())
			{
				OutError = TEXT("Runtime child node has no resolved parent texture parameter.");
				break;
			}
			UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(
				Node.GeneratorMaterial, this);
			if (Dynamic == nullptr)
			{
				OutError = TEXT("Could not create the runtime child generator instance.");
				break;
			}
			Dynamic->SetTextureParameterValue(Node.ParentTextureParameter, *Parent);
			DrawMaterial = Dynamic;
		}
		else if (Node.ParentNodeIds.Num() > 1)
		{
			OutError = TEXT("The first runtime texture contract admits at most one parent per node.");
			break;
		}

		UKismetRenderingLibrary::DrawMaterialToRenderTarget(WorldContextObject, Target, DrawMaterial);
		NodeTargets.Add(Node.NodeId, Target);
		SeenNodes.Add(Node.NodeId);
		++NodeGenerationCounts.FindOrAdd(Identity);
		++Telemetry.Generations;
	}

	if (!OutError.IsEmpty() || !SeenNodes.Contains(OutputSlot->NodeId))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Runtime texture catalog did not generate the exported node.");
		}
		for (const FString& Identity : AdmittedIdentities)
		{
			Cache->Fail(Identity);
		}
		ReleaseTransientTargets();
		Telemetry.ResidentBytes = Cache->GetResidentBytes();
		Telemetry.PeakResidentBytes = Cache->GetPeakResidentBytes();
		++Telemetry.Failures;
		return false;
	}

	FlushRenderingCommands();
	for (const FString& Identity : AdmittedIdentities)
	{
		FString CompleteError;
		if (!Cache->Complete(Identity, CompleteError))
		{
			OutError = CompleteError;
			++Telemetry.Failures;
			return false;
		}
	}
	ReadyOutputs.Add(OutputIdentity);
	ReleaseTransientTargets();
	Cache->EvictUnpinned(MAX_int64);
	Telemetry.ResidentBytes = Cache->GetResidentBytes();
	Telemetry.PeakResidentBytes = Cache->GetPeakResidentBytes();
	UE_LOG(LogProjectTextureRuntime, Display,
		TEXT("[RuntimeCache::Generate] Ready - pattern=%s slot=%s nodes=%d resident_bytes=%lld peak_bytes=%lld"),
		*Catalog->PatternId.ToString(),
		*OutputSlot->SlotName.ToString(),
		SeenNodes.Num(),
		Telemetry.ResidentBytes,
		Telemetry.PeakResidentBytes);
	if (FParse::Param(FCommandLine::Get(), TEXT("ProjectTextureColdWarmProof")))
	{
		const double ColdMilliseconds = (FPlatformTime::Seconds() - RequestStartedSeconds) * 1000.0;
		const int32 ColdNodeGenerations = Telemetry.Generations - GenerationsBeforeCold;
		UTextureRenderTarget2D* ResourceBeforeWarm = OutputSlot->GetRenderTarget();
		const int32 GenerationsBeforeWarm = Telemetry.Generations;
		const int32 HitsBeforeWarm = Telemetry.CacheHits;
		const double WarmStartedSeconds = FPlatformTime::Seconds();
		FString WarmError;
		const bool bWarmReady = RequestOutput(WorldContextObject, OutputSlot, WarmError) &&
			IsOutputReady(OutputSlot);
		const double WarmMilliseconds = (FPlatformTime::Seconds() - WarmStartedSeconds) * 1000.0;
		UE_LOG(LogProjectTextureRuntime, Display,
			TEXT("[RuntimeCache::ColdWarmProof] identity=%s node=%s cold_request_to_ready_ms=%.6f ")
			TEXT("warm_request_to_ready_ms=%.6f cold_node_generations=%d warm_node_generations=%d ")
			TEXT("warm_cache_hits=%d same_resource=%d warm_ready=%d resident_bytes=%lld peak_bytes=%lld"),
			*Catalog->SemanticIdentity, *OutputSlot->NodeId.ToString(),
			ColdMilliseconds, WarmMilliseconds, ColdNodeGenerations,
			Telemetry.Generations - GenerationsBeforeWarm, Telemetry.CacheHits - HitsBeforeWarm,
			ResourceBeforeWarm == OutputSlot->GetRenderTarget() ? 1 : 0, bWarmReady ? 1 : 0,
			Telemetry.ResidentBytes, Telemetry.PeakResidentBytes);
	}
	return true;
}

bool UProjectTextureRuntimeSubsystem::IsOutputReady(
	const UProjectTextureOutputSlot* OutputSlot) const
{
	return OutputSlot != nullptr && OutputSlot->Catalog != nullptr &&
		ReadyOutputs.Contains(
			OutputSlot->Catalog->SemanticIdentity + TEXT(":") + OutputSlot->NodeId.ToString());
}

void UProjectTextureRuntimeSubsystem::ReleaseTransientTargets()
{
	for (const TPair<FString, TObjectPtr<UTextureRenderTarget2D>>& Pair : TransientTargets)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->ReleaseResource();
		}
	}
	TransientTargets.Reset();
}
