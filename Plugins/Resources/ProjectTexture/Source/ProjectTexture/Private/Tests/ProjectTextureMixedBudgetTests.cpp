// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureBundleTestProducer.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UProjectTextureRuntimeCatalog* Catalog(TCHAR Id, int64 Budget)
{
	auto* C = NewObject<UProjectTextureRuntimeCatalog>();
	C->SemanticIdentity = FString::ChrN(64, Id);
	C->OutputContract = TEXT("soil_structure_normal_masks_v1");
	C->BudgetBytes = Budget;
	for (int32 I = 0; I < 3; ++I)
	{
		FProjectTextureRuntimeNodeDescriptor N;
		N.NodeId = FName(I == 0 ? TEXT("Structure") : I == 1 ? TEXT("Normal") : TEXT("Masks"));
		N.StructuralIdentity = FString::ChrN(64, TCHAR('a' + I));
		N.Width = N.Height = 8;
		N.GeneratorMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		if (I) { N.ParentNodeIds.Add(TEXT("Structure")); N.ParentTextureParameter = TEXT("ParentStructure"); }
		C->Nodes.Add(N);
		auto* RT = NewObject<UTextureRenderTarget2D>(C->GetOuter());
		auto* Slot = NewObject<UProjectTextureOutputSlot>(RT);
		Slot->Catalog = C; Slot->SlotName = Slot->NodeId = N.NodeId;
		RT->AddAssetUserData(Slot); C->OutputSlots.Add(Slot);
	}
	return C;
}
class FMixedBudgetProof final : public IAutomationLatentCommand
{
public:
	explicit FMixedBudgetProof(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		auto* Owner = GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
		FString Error;
		if (Phase == 0)
		{
			Budget = Owner->GetBudgetBytes();
			Test->TestTrue(TEXT("Initialization establishes positive global policy."), Budget > 0);
			Native.Reset(Catalog('3', 16 * 1024 * 1024));
			Small.Reset(Catalog('4', 4 * 1024 * 1024));
			Producer.Reset(NewObject<UProjectTextureBundleTestProducer>());
			Producer->Identity = Native->SemanticIdentity; Producer->PeakBytes = 8 * 1024 * 1024;
			Test->TestEqual(TEXT("Large reservation starts pinned."), Owner->RequestBundle(Native->OutputSlots[0], Producer.Get(), true, Error), EProjectTextureBundleRequest::Pending);
			Test->TestTrue(TEXT("Smaller catalog cannot shrink shared policy."), Owner->RequestOutput(GEngine->GetCurrentPlayWorld(), Small->OutputSlots[0], Error));
			Test->TestEqual(TEXT("Concurrent native request shares its pin."), Owner->RequestBundle(Native->OutputSlots[1], Producer.Get(), true, Error), EProjectTextureBundleRequest::Pending);
			Phase = 1; return false;
		}
		if (Phase == 1)
		{
			if (!Owner->IsBundleReady(Native->OutputSlots[0]))
			{
				if (FPlatformTime::Seconds() - Started < 15) return false;
				Test->AddError(TEXT("Mixed producer readiness timed out.")); return true;
			}
			auto* Before = Native->OutputSlots[0]->GetRenderTarget()->GetResource();
			TStrongObjectPtr<UProjectTextureRuntimeCatalog> TooSmall(Catalog('5', 4 * 1024 * 1024));
			TStrongObjectPtr<UProjectTextureBundleTestProducer> Oversized(NewObject<UProjectTextureBundleTestProducer>());
			Oversized->Identity = TooSmall->SemanticIdentity; Oversized->PeakBytes = 8 * 1024 * 1024;
			Test->TestEqual(TEXT("Producer cannot exceed its catalog envelope."), Owner->RequestBundle(TooSmall->OutputSlots[0], Oversized.Get(), true, Error), EProjectTextureBundleRequest::Rejected);
			Test->TestEqual(TEXT("Envelope refusal never starts a draw."), Oversized->Starts, 0);
			Test->TestTrue(TEXT("Envelope rejection is explicit."), Error.Contains(TEXT("catalog generation envelope")));
			TStrongObjectPtr<UProjectTextureRuntimeCatalog> Huge(Catalog('6', Budget + 1024 * 1024));
			Oversized->Identity = Huge->SemanticIdentity; Oversized->PeakBytes = Budget + 1;
			Test->TestEqual(TEXT("Catalog cannot raise the global budget."), Owner->RequestBundle(Huge->OutputSlots[0], Oversized.Get(), true, Error), EProjectTextureBundleRequest::Rejected);
			Test->TestEqual(TEXT("Global refusal never starts a draw."), Oversized->Starts, 0);
			Test->TestEqual(TEXT("Request order and refusals cannot mutate policy."), Owner->GetBudgetBytes(), Budget);
			Test->TestTrue(TEXT("Refusals preserve pinned readiness."), Owner->IsBundleReady(Native->OutputSlots[0]));
			Test->TestTrue(TEXT("Refusals preserve physical resources."), Before == Native->OutputSlots[0]->GetRenderTarget()->GetResource());
			Test->TestTrue(TEXT("Both shared pins release independently."), Owner->ReleaseBundle(Native->SemanticIdentity, Error) && Owner->ReleaseBundle(Native->SemanticIdentity, Error));
			Owner->EvictReleasedBundles();
			Native.Reset(Catalog('7', 16 * 1024 * 1024)); Producer->Identity = Native->SemanticIdentity;
			Test->TestTrue(TEXT("Default-first request is shared."), Owner->RequestOutput(GEngine->GetCurrentPlayWorld(), Small->OutputSlots[1], Error));
			Test->TestEqual(TEXT("Reverse order admits native reservation."), Owner->RequestBundle(Native->OutputSlots[0], Producer.Get(), true, Error), EProjectTextureBundleRequest::Pending);
			Phase = 2; return false;
		}
		if (!Owner->IsBundleReady(Native->OutputSlots[0]))
		{
			if (FPlatformTime::Seconds() - Started < 15) return false;
			Test->AddError(TEXT("Reverse-order readiness timed out.")); return true;
		}
		Test->TestTrue(TEXT("Reverse-order pin releases."), Owner->ReleaseBundle(Native->SemanticIdentity, Error));
		Test->TestEqual(TEXT("Reverse order preserves policy."), Owner->GetBudgetBytes(), Budget);
		Owner->EvictReleasedBundles(); return true;
	}
private:
	FAutomationTestBase* Test;
	double Started;
	int32 Phase = 0;
	int64 Budget = 0;
	TStrongObjectPtr<UProjectTextureRuntimeCatalog> Native, Small;
	TStrongObjectPtr<UProjectTextureBundleTestProducer> Producer;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureMixedBudgetTest, "Project.Texture.Runtime.MixedProducerBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FProjectTextureMixedBudgetTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FMixedBudgetProof(this)); return true;
}
#endif
