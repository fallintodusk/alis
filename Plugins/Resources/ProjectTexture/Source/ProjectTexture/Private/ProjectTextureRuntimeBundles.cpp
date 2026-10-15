// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureRuntimeSubsystem.h"
#include "ProjectTextureBundleProducer.h"
#include "ProjectTextureBundlePreflight.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeCache.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectTextureBundles, Log, All);

namespace
{
FString DescriptorIdentity(const UProjectTextureRuntimeCatalog* Catalog)
{
	FString Result = Catalog->SemanticIdentity + Catalog->OutputContract.ToString() + LexToString(Catalog->BudgetBytes);
	for (const auto& N : Catalog->Nodes)
	{
		Result += FString::Printf(TEXT("|%s:%s:%d:%d:%d:%d:%s"), *N.NodeId.ToString(),
			*N.StructuralIdentity, N.Width, N.Height, int32(N.Format), N.bGenerateMips, *N.ParentTextureParameter.ToString());
		for (FName Parent : N.ParentNodeIds) Result += TEXT(":") + Parent.ToString();
	}
	for (const auto& Slot : Catalog->OutputSlots)
	{
		Result += FString::Printf(TEXT("|slot:%p:%p:%s:%s"), Slot.Get(), Slot ? Slot->GetRenderTarget() : nullptr,
			Slot ? *Slot->SlotName.ToString() : TEXT(""), Slot ? *Slot->NodeId.ToString() : TEXT(""));
	}
	return Result;
}
FString BundleKey(const FString& Identity) { return TEXT("bundle:") + Identity; }
}

EProjectTextureBundleRequest UProjectTextureRuntimeSubsystem::RequestBundle(
	UProjectTextureOutputSlot* Slot, UProjectTextureBundleProducer* Producer, bool bPin, FString& Error)
{
	Error.Reset();
	++Telemetry.Requests;
	TMap<FName, UTextureRenderTarget2D*> Targets;
	if (!IsInGameThread() || !Cache || !Producer ||
		!ProjectTextureBundlePreflight::Validate(Slot, Targets, Error, false))
	{
		if (Error.IsEmpty()) Error = TEXT("Bundle requests require the game thread, producer and valid catalog.");
		++Telemetry.Failures;
		return EProjectTextureBundleRequest::Rejected;
	}
	auto* Catalog = Slot->Catalog.Get();
	if (Catalog->OutputContract != TEXT("soil_structure_normal_masks_v1") && Catalog->OutputContract!=TEXT("aggregate_signals_normal_v1"))
	{
		Error = TEXT("Explicit producers require a complete admitted soil or aggregate bundle.");
		return EProjectTextureBundleRequest::Rejected;
	}
	const FString Identity = Catalog->SemanticIdentity;
	const int64 Peak = Producer->GetPeakBytes();
	if (Producer->GetStructuralIdentity() != Identity || !Producer->Validate(Catalog, Error) || Peak <= 0)
	{
		if (Error.IsEmpty()) Error = TEXT("Producer does not authenticate this structural identity and allocation.");
		++Telemetry.Failures;
		return EProjectTextureBundleRequest::Rejected;
	}
	if (Peak > Catalog->BudgetBytes)
	{
		Error = TEXT("Producer peak reservation exceeds its catalog generation envelope.");
		++Telemetry.Failures;
		return EProjectTextureBundleRequest::Rejected;
	}
	const FString Descriptor = DescriptorIdentity(Catalog);
	for (const auto& Pair : Bundles)
	{
		if (Pair.Key == Identity) continue;
		for (const auto& Target : Pair.Value.Targets)
			for (const auto& Output : Catalog->OutputSlots)
				if (Target == Output->GetRenderTarget())
				{
					Error = TEXT("Another immutable bundle owns this output target.");
					return EProjectTextureBundleRequest::Rejected;
				}
	}
	if (const auto* Entry = Cache->Find(BundleKey(Identity)); Entry && Entry->State == EProjectTextureCacheState::Failed)
	{
		// Failed operations have no accepted pixels or pins; drain the old producer before retry.
		ReleaseBundleResources(Identity);
	}
	if (!Bundles.Contains(Identity))
	{
		for (const auto& N : Catalog->Nodes)
		{
			if (Cache->Find(Identity + TEXT(":") + N.StructuralIdentity) ||
				ReadyOutputs.Contains(Identity + TEXT(":") + N.NodeId.ToString()))
			{
				Error = TEXT("Default generation already owns this catalog; opt-in replacement is refused.");
				return EProjectTextureBundleRequest::Rejected;
			}
		}
	}
	if (const auto* Existing = Bundles.Find(Identity))
	{
		if (Existing->DescriptorIdentity != Descriptor || Existing->Catalog != Catalog || Existing->PeakBytes != Peak || Existing->bCancelled)
		{
			Error = TEXT("An immutable bundle cannot change descriptors, targets, allocation or cancellation state.");
			return EProjectTextureBundleRequest::Rejected;
		}
	}
	const auto Request = Cache->Request(BundleKey(Identity), Peak, bPin, Error);
	// Reconcile physical ownership before admitting another GPU allocation.
	ReconcileBundles();
	if (Request == EProjectTextureCacheRequest::Rejected) return EProjectTextureBundleRequest::Rejected;
	if (Request == EProjectTextureCacheRequest::Ready)
	{
		if (!IsBundleReady(Slot))
		{
			if (bPin) Cache->Release(BundleKey(Identity), Error);
			Error = TEXT("A cached bundle lost its stable output resource; refuse replacement while referenced.");
			return EProjectTextureBundleRequest::Rejected;
		}
		++Telemetry.CacheHits;
		return EProjectTextureBundleRequest::Ready;
	}
	if (Request == EProjectTextureCacheRequest::Pending) return EProjectTextureBundleRequest::Pending;
	FProjectTextureAsyncBundle& Bundle = Bundles.Add(Identity);
	Bundle.Producer = Producer;
	Bundle.Catalog = Catalog;
	Bundle.PeakBytes = Peak;
	Bundle.DescriptorIdentity = Descriptor;
	for (const auto& Output : Catalog->OutputSlots)
	{
		Bundle.Targets.Add(Output->GetRenderTarget());
		Bundle.ReadyKeys.Add(Identity + TEXT(":") + Output->NodeId.ToString());
	}
	if (!Producer->Begin(Error))
	{
		Producer->CancelAndDrain();
		Producer->ReleaseOutputs();
		Cache->Fail(BundleKey(Identity));
		Bundles.Remove(Identity);
		++Telemetry.Failures;
		return EProjectTextureBundleRequest::Rejected;
	}
	++Telemetry.Generations;
	UE_LOG(LogProjectTextureBundles, Display, TEXT("[Bundle::Begin] identity=%s peak_reservation=%lld"), *Identity, Peak);
	return EProjectTextureBundleRequest::Pending;
}

bool UProjectTextureRuntimeSubsystem::TickBundles(float Delta)
{
	if (!Cache) return true;
	GeneratePendingOutputs();
	for (auto& Pair : Bundles)
	{
		auto& B = Pair.Value;
		const auto* Entry = Cache->Find(BundleKey(Pair.Key));
		if (!Entry || Entry->State != EProjectTextureCacheState::Generating) continue;
		if (B.DescriptorIdentity != DescriptorIdentity(B.Catalog))
		{
			B.bCancelled = true;
			B.Error = TEXT("Catalog changed during immutable generation.");
		}
		if (!B.bCopying)
		{
			const auto State = B.Producer->Poll(Delta, B.Error);
			if (State == EProjectTextureProducerState::Pending) continue;
			if (State == EProjectTextureProducerState::Failed || B.bCancelled)
			{
				B.Producer->CancelAndDrain();
				B.Producer->ReleaseOutputs();
				Cache->Fail(BundleKey(Pair.Key));
				++Telemetry.Failures;
				UE_LOG(LogProjectTextureBundles, Warning, TEXT("[Bundle::Refused] identity=%s error=%s"), *Pair.Key, *B.Error);
				continue;
			}
			const auto Sources = B.Producer->GetOutputs();
			bool Valid = Sources.Num() == B.Targets.Num() && !Sources.IsEmpty();
			TArray<const FProjectTextureRuntimeNodeDescriptor*> ExportDescriptors;
			for (int32 I = 0; Valid && I < Sources.Num(); ++I)
			{
				const auto* Slot=B.Catalog->OutputSlots[I].Get();
				const auto* Descriptor=B.Catalog->Nodes.FindByPredicate([Slot](const auto& Node){return Slot && Node.NodeId==Slot->NodeId;});
				if (!Descriptor) { Valid=false; break; }
				ExportDescriptors.Add(Descriptor);
				const auto& N = *Descriptor;
				UE_LOG(LogProjectTextureBundles, Display, TEXT("[Bundle::Source] index=%d size=%dx%d expected=%dx%d format=%d srgb=%d resource=%d rhi=%d count=%d targets=%d same=%d"),
					I, Sources[I] ? Sources[I]->SizeX : 0, Sources[I] ? Sources[I]->SizeY : 0,
					N.Width, N.Height,
					Sources[I] ? int32(Sources[I]->GetFormat()) : -1, Sources[I] ? Sources[I]->SRGB : -1,
					Sources[I] && Sources[I]->GetResource() ? 1 : 0,
					Sources[I] && Sources[I]->GetResource() && Sources[I]->GetResource()->TextureRHI.IsValid() ? 1 : 0,
					Sources.Num(), B.Targets.Num(), Sources[I] == B.Targets[I] ? 1 : 0);
				Valid = Sources[I] && Sources[I] != B.Targets[I] && Sources[I]->GetResource() &&
					Sources[I]->SizeX == N.Width && Sources[I]->SizeY == N.Height && !Sources[I]->SRGB &&
					Sources[I]->GetResource()->TextureRHI.IsValid() && Sources[I]->GetFormat() == PF_B8G8R8A8;
			}
			if (!Valid)
			{
				B.Error = TEXT("Producer returned an incomplete or incompatible linear RGBA8 bundle.");
				B.Producer->CancelAndDrain(); B.Producer->ReleaseOutputs(); Cache->Fail(BundleKey(Pair.Key));
				++Telemetry.Failures;
				UE_LOG(LogProjectTextureBundles, Warning, TEXT("[Bundle::Refused] identity=%s error=%s"), *Pair.Key, *B.Error);
				continue;
			}
			for (int32 I = 0; I < Sources.Num(); ++I)
			{
				auto* Target = B.Targets[I].Get();
				const auto& N = *ExportDescriptors[I];
				Target->RenderTargetFormat = RTF_RGBA8;
				Target->bAutoGenerateMips = true;
				Target->AddressX = Target->AddressY = TA_Wrap;
				Target->MipsAddressU = Target->MipsAddressV = TA_Wrap;
				Target->Filter = Target->MipsSamplerFilter = TF_Trilinear;
				Target->bForceLinearGamma = true; Target->SRGB = false;
				Target->InitAutoFormat(N.Width, N.Height); Target->UpdateResourceImmediate(true);
				FTextureRHIRef Src = Sources[I]->GetResource()->TextureRHI;
				FTextureRenderTargetResource* DstResource = Target->GameThread_GetRenderTargetResource();
				ENQUEUE_RENDER_COMMAND(ProjectTextureCopyBundle)([Src, DstResource](FRHICommandListImmediate& Cmd)
				{
					FTextureRHIRef Dst = DstResource->GetRenderTargetTexture();
					Cmd.Transition(FRHITransitionInfo(Src, ERHIAccess::Unknown, ERHIAccess::CopySrc));
					Cmd.Transition(FRHITransitionInfo(Dst, ERHIAccess::Unknown, ERHIAccess::CopyDest));
					Cmd.CopyTexture(Src, Dst, FRHICopyTextureInfo());
					Cmd.Transition(FRHITransitionInfo(Src, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
					Cmd.Transition(FRHITransitionInfo(Dst, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
				});
				Target->UpdateResourceImmediate(false);
			}
			B.Fence = RHICreateGPUFence(TEXT("ProjectTextureBundleReady"));
			ENQUEUE_RENDER_COMMAND(ProjectTextureFenceBundle)([Fence = B.Fence](FRHICommandListImmediate& Cmd) { Cmd.WriteGPUFence(Fence); });
			B.bCopying = true;
			continue;
		}
		if (!B.Fence->Poll()) continue;
		B.Producer->ReleaseOutputs();
		if (B.bCancelled)
		{
			for (auto& Target : B.Targets) Target->ReleaseResource();
			Cache->Fail(BundleKey(Pair.Key));
			continue;
		}
		for (int32 I = 0; I < B.Targets.Num(); ++I)
		{
			auto* Target = B.Targets[I].Get();
			ReadyOutputs.Add(B.ReadyKeys[I],
				FReadyOutput{Target, Target->GetResource()->TextureRHI});
		}
		Cache->Complete(BundleKey(Pair.Key), B.Error);
		UE_LOG(LogProjectTextureBundles, Display, TEXT("[Bundle::Ready] identity=%s gpu_fence_complete=1"), *Pair.Key);
	}
	Telemetry.ResidentBytes = Cache->GetResidentBytes();
	Telemetry.PeakResidentBytes = Cache->GetPeakResidentBytes();
	return true;
}

bool UProjectTextureRuntimeSubsystem::IsBundleReady(const UProjectTextureOutputSlot* Slot) const
{
	TMap<FName, UTextureRenderTarget2D*> Targets;
	FString Error;
	if (!ProjectTextureBundlePreflight::Validate(Slot, Targets, Error, false)) return false;
	const auto* Entry = Cache ? Cache->Find(BundleKey(Slot->Catalog->SemanticIdentity)) : nullptr;
	const auto* Bundle = Bundles.Find(Slot->Catalog->SemanticIdentity);
	return Bundle && Bundle->Catalog == Slot->Catalog && Bundle->DescriptorIdentity == DescriptorIdentity(Slot->Catalog) &&
		Entry && Entry->State == EProjectTextureCacheState::Ready && AreOutputsReady(Slot->Catalog, Targets);
}

bool UProjectTextureRuntimeSubsystem::ReleaseBundle(const FString& Identity, FString& Error)
{
	return IsInGameThread() && Cache && Cache->Release(BundleKey(Identity), Error);
}

bool UProjectTextureRuntimeSubsystem::CancelBundle(const FString& Identity, FString& Error)
{
	Error.Reset();
	auto* B = IsInGameThread() ? Bundles.Find(Identity) : nullptr;
	const auto* Entry = Cache ? Cache->Find(BundleKey(Identity)) : nullptr;
	if (!B || !Entry || Entry->State != EProjectTextureCacheState::Generating)
	{
		Error = TEXT("Only an in-flight bundle can be cancelled."); return false;
	}
	B->bCancelled = true;
	B->Error = TEXT("Cancelled; native work drains before resource release.");
	return true;
}

void UProjectTextureRuntimeSubsystem::ReleaseBundleResources(const FString& Identity)
{
	if (auto* B = Bundles.Find(Identity))
	{
		B->Producer->CancelAndDrain();
		if (B->bCopying && B->Fence.IsValid() && !B->Fence->Poll())
		{
			ENQUEUE_RENDER_COMMAND(ProjectTextureDrainBundle)([](FRHICommandListImmediate& Cmd)
			{
				Cmd.SubmitAndBlockUntilGPUIdle();
			});
			FlushRenderingCommands();
		}
		B->Producer->ReleaseOutputs();
		for (int32 I = 0; I < B->Targets.Num(); ++I)
		{
			ReadyOutputs.Remove(B->ReadyKeys[I]);
			B->Targets[I]->ReleaseResource();
		}
		Bundles.Remove(Identity);
	}
}

int32 UProjectTextureRuntimeSubsystem::EvictReleasedBundles()
{
	if (!IsInGameThread() || !Cache) return 0;
	Cache->EvictUnpinned(MAX_int64);
	const int32 Before = Bundles.Num();
	ReconcileBundles();
	Telemetry.ResidentBytes = Cache->GetResidentBytes();
	return Before - Bundles.Num();
}

void UProjectTextureRuntimeSubsystem::ReconcileBundles()
{
	TArray<FString> Evicted;
	for (const auto& Pair : Bundles) if (!Cache->Find(BundleKey(Pair.Key))) Evicted.Add(Pair.Key);
	for (const auto& Identity : Evicted) ReleaseBundleResources(Identity);
}

void UProjectTextureRuntimeSubsystem::DrainBundles()
{
	TArray<FString> Identities;
	Bundles.GetKeys(Identities);
	for (const auto& Identity : Identities)
	{
		ReleaseBundleResources(Identity);
		if (Cache) Cache->Fail(BundleKey(Identity));
	}
}
