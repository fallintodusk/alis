// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureBundleTestProducer.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"

bool UProjectTextureBundleTestProducer::Validate(const UProjectTextureRuntimeCatalog* Catalog, FString& Error) const
{
	return Catalog && Catalog->Nodes.Num() == 3 && Catalog->Nodes[0].Width == 8;
}

bool UProjectTextureBundleTestProducer::Begin(FString& Error)
{
	++Starts;
	for (int32 I = 0; I < 3; ++I)
	{
		auto* Target = NewObject<UTextureRenderTarget2D>(this);
		Target->RenderTargetFormat = RTF_RGBA8;
		Target->SRGB = false; Target->bForceLinearGamma = true;
		Target->ClearColor = FLinearColor(.2f + .1f * I, .5f, .7f, 1);
		Target->InitAutoFormat(8, 8); Target->UpdateResourceImmediate(true);
		Outputs.Add(Target);
	}
	Fence.BeginFence(FRenderCommandFence::ESyncDepth::RHIThread);
	return true;
}

EProjectTextureProducerState UProjectTextureBundleTestProducer::Poll(float Delta, FString& Error)
{
	return Fence.IsFenceComplete() ? EProjectTextureProducerState::Ready : EProjectTextureProducerState::Pending;
}

TArray<UTextureRenderTarget2D*> UProjectTextureBundleTestProducer::GetOutputs() const
{
	TArray<UTextureRenderTarget2D*> Result;
	for (UTextureRenderTarget2D* Output : Outputs) Result.Add(Output);
	if (bOmitSibling && !Result.IsEmpty()) Result.Pop();
	return Result;
}

void UProjectTextureBundleTestProducer::CancelAndDrain() { Fence.Wait(); }
void UProjectTextureBundleTestProducer::ReleaseOutputs()
{
	for (auto& Target : Outputs) Target->ReleaseResource();
	Outputs.Reset();
}
