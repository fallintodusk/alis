// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureBundleTestProducer.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UProjectTextureRuntimeCatalog* MakeCatalog(TCHAR Id)
{
	auto* Catalog = NewObject<UProjectTextureRuntimeCatalog>();
	Catalog->SemanticIdentity = FString::ChrN(64, Id);
	Catalog->OutputContract = TEXT("soil_structure_normal_masks_v1");
	Catalog->BudgetBytes = 4 * 1024 * 1024;
	for (int32 I = 0; I < 3; ++I)
	{
		FProjectTextureRuntimeNodeDescriptor N;
		N.NodeId = FName(I == 0 ? TEXT("Structure") : I == 1 ? TEXT("Normal") : TEXT("Masks"));
		N.StructuralIdentity = FString::ChrN(64, TCHAR('b' + I));
		N.Width = N.Height = 8;
		N.GeneratorMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		if (I) { N.ParentNodeIds.Add(TEXT("Structure")); N.ParentTextureParameter = TEXT("ParentStructure"); }
		Catalog->Nodes.Add(N);
		auto* Target = NewObject<UTextureRenderTarget2D>(Catalog->GetOuter());
		auto* Slot = NewObject<UProjectTextureOutputSlot>(Target);
		Slot->Catalog = Catalog; Slot->SlotName = N.NodeId; Slot->NodeId = N.NodeId;
		Target->AddAssetUserData(Slot); Catalog->OutputSlots.Add(Slot);
	}
	return Catalog;
}

class FVerifyBundleOwnership final : public IAutomationLatentCommand
{
public:
	explicit FVerifyBundleOwnership(FAutomationTestBase* InTest) : Test(InTest), Catalog(MakeCatalog('e')),
		Producer(NewObject<UProjectTextureBundleTestProducer>()), Started(FPlatformTime::Seconds())
	{
		Producer->Identity = Catalog->SemanticIdentity;
		auto* Owner = GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
		FString Error;
		Test->TestEqual(TEXT("Explicit cold request starts."), Owner->RequestBundle(Catalog->OutputSlots[0], Producer.Get(), true, Error), EProjectTextureBundleRequest::Pending);
		Test->TestEqual(TEXT("Concurrent request shares."), Owner->RequestBundle(Catalog->OutputSlots[1], Producer.Get(), true, Error), EProjectTextureBundleRequest::Pending);
		Test->TestFalse(TEXT("Default writer cannot race pending producer."), Owner->RequestOutput(GEngine->GetCurrentPlayWorld(), Catalog->OutputSlots[0], Error));
		Test->TestTrue(TEXT("Refusal comes from ownership."), Error.Contains(TEXT("owned by an explicit")));
		Test->TestNull(TEXT("Refusal has not allocated stable targets."), Catalog->OutputSlots[0]->GetRenderTarget()->GetResource());
	}
	virtual bool Update() override
	{
		auto* Owner = GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
		if (!Owner->IsBundleReady(Catalog->OutputSlots[0]))
		{
			if (FPlatformTime::Seconds() - Started < 10) return false;
			Test->AddError(TEXT("Bundle did not become GPU-ready within 10 seconds.")); return true;
		}
		FString Error;
		auto* Target = Catalog->OutputSlots[0]->GetRenderTarget();
		FTextureRHIRef Before = Target->GetResource()->TextureRHI;
		Test->TestEqual(TEXT("Single writer ran once."), Producer->Starts, 1);
		const auto WorldOptions = UWorld::InitializationValues().CreatePhysicsScene(false);
		auto* OtherWorld = UWorld::CreateWorld(EWorldType::GamePreview, false, NAME_None, nullptr,
			true, ERHIFeatureLevel::Num, &WorldOptions);
		OtherWorld->DestroyWorld(false);
		Test->TestTrue(TEXT("Other-world cleanup preserves shared readiness."), Owner->IsBundleReady(Catalog->OutputSlots[0]));
		Test->TestTrue(TEXT("Other-world cleanup preserves pinned RHI."), Before == Target->GetResource()->TextureRHI);
		Test->TestTrue(TEXT("Consumer one releases."), Owner->ReleaseBundle(Catalog->SemanticIdentity, Error));
		Test->TestTrue(TEXT("Consumer two releases."), Owner->ReleaseBundle(Catalog->SemanticIdentity, Error));
		TStrongObjectPtr<UProjectTextureRuntimeCatalog> Default(MakeCatalog('f'));
		Test->TestTrue(TEXT("Different catalog uses unchanged default producer."), Owner->RequestOutput(GEngine->GetCurrentPlayWorld(), Default->OutputSlots[0], Error));
		Test->TestFalse(TEXT("Default eviction invalidates opt-in readiness."), Owner->IsBundleReady(Catalog->OutputSlots[0]));
		Test->TestNull(TEXT("Default eviction releases physical target."), Target->GetResource());
		TStrongObjectPtr<UProjectTextureBundleTestProducer> Replacement(NewObject<UProjectTextureBundleTestProducer>());
		Replacement->Identity = Default->SemanticIdentity;
		auto* DefaultResource = Default->OutputSlots[0]->GetRenderTarget()->GetResource();
		Test->TestEqual(TEXT("Default-owned catalog refuses opt-in replacement."), Owner->RequestBundle(Default->OutputSlots[0], Replacement.Get(), true, Error), EProjectTextureBundleRequest::Rejected);
		Test->TestTrue(TEXT("Refusal preserves the default resource."), DefaultResource == Default->OutputSlots[0]->GetRenderTarget()->GetResource());
		return true;
	}
private:
	FAutomationTestBase* Test;
	TStrongObjectPtr<UProjectTextureRuntimeCatalog> Catalog;
	TStrongObjectPtr<UProjectTextureBundleTestProducer> Producer;
	double Started;
};

class FVerifyBundleRecovery final : public IAutomationLatentCommand
{
public:
	explicit FVerifyBundleRecovery(FAutomationTestBase* InTest, bool bCancel = false) : Test(InTest), Catalog(MakeCatalog(bCancel ? '9' : '8')),
		Producer(NewObject<UProjectTextureBundleTestProducer>()), Started(FPlatformTime::Seconds())
	{
		Producer->Identity = Catalog->SemanticIdentity;
		Producer->bOmitSibling = !bCancel;
		FString Error;
		Test->TestEqual(TEXT("Incomplete producer is admitted before results exist."),
			GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>()->RequestBundle(Catalog->OutputSlots[0], Producer.Get(), true, Error),
			EProjectTextureBundleRequest::Pending);
		if (bCancel) Test->TestTrue(TEXT("Pending bundle can be cancelled."),
			GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>()->CancelBundle(Catalog->SemanticIdentity, Error));
	}
	virtual bool Update() override
	{
		auto* Owner = GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
		FString Error;
		if (FPlatformTime::Seconds() - Started > 10) { Test->AddError(TEXT("Recovery exceeded 10 seconds.")); return true; }
		if (!bRetried)
		{
			if (Producer->GetOutputs().Num() != 0) return false;
			Test->TestFalse(TEXT("Missing sibling never becomes ready."), Owner->IsBundleReady(Catalog->OutputSlots[0]));
			for (const auto& Slot : Catalog->OutputSlots) Test->TestNull(TEXT("No partial stable output allocated."), Slot->GetRenderTarget()->GetResource());
			Producer.Reset(NewObject<UProjectTextureBundleTestProducer>());
			Producer->Identity = Catalog->SemanticIdentity;
			Test->TestEqual(TEXT("Quiescent failure permits a fresh producer."),
				Owner->RequestBundle(Catalog->OutputSlots[0], Producer.Get(), true, Error), EProjectTextureBundleRequest::Pending);
			bRetried = true;
			return false;
		}
		if (!Owner->IsBundleReady(Catalog->OutputSlots[0])) return false;
		auto* Resource = Catalog->OutputSlots[0]->GetRenderTarget()->GetResource();
		Catalog->BudgetBytes /= 2;
		Test->TestFalse(TEXT("Descriptor mutation invalidates the public Ready query."), Owner->IsBundleReady(Catalog->OutputSlots[0]));
		Test->TestEqual(TEXT("Descriptor mutation refuses warm identity."),
			Owner->RequestBundle(Catalog->OutputSlots[0], Producer.Get(), true, Error), EProjectTextureBundleRequest::Rejected);
		Test->TestTrue(TEXT("Refusal preserves accepted resource."), Resource == Catalog->OutputSlots[0]->GetRenderTarget()->GetResource());
		Catalog->BudgetBytes *= 2;
		Test->TestTrue(TEXT("Recovered consumer releases."), Owner->ReleaseBundle(Catalog->SemanticIdentity, Error));
		Test->TestEqual(TEXT("Recovered bundle releases exactly once."), Owner->EvictReleasedBundles(), 1);
		return true;
	}
private:
	FAutomationTestBase* Test;
	TStrongObjectPtr<UProjectTextureRuntimeCatalog> Catalog;
	TStrongObjectPtr<UProjectTextureBundleTestProducer> Producer;
	double Started;
	bool bRetried = false;
};

class FVerifyBundleMutationShutdown final : public IAutomationLatentCommand
{
public:
	explicit FVerifyBundleMutationShutdown(FAutomationTestBase* InTest) : Test(InTest), Catalog(MakeCatalog('7')),
		Producer(NewObject<UProjectTextureBundleTestProducer>()), Started(FPlatformTime::Seconds())
	{
		Producer->Identity = Catalog->SemanticIdentity;
		FString Error;
		Test->TestEqual(TEXT("Complete descriptor is admitted."),
			GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>()->RequestBundle(Catalog->OutputSlots[0], Producer.Get(), true, Error),
			EProjectTextureBundleRequest::Pending);
		Catalog->Nodes.Empty();
	}
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 10) { Test->AddError(TEXT("Mutation did not drain.")); return true; }
		if (!Producer->GetOutputs().IsEmpty()) return false;
		for (const auto& Slot : Catalog->OutputSlots)
			Test->TestNull(TEXT("Changed descriptor publishes no output."), Slot->GetRenderTarget()->GetResource());
		// Preserve the malformed descriptor through engine shutdown. The fixture
		// host verifies clean termination, as well as the automation result.
		GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>()->Deinitialize();
		return true;
	}
private:
	FAutomationTestBase* Test;
	TStrongObjectPtr<UProjectTextureRuntimeCatalog> Catalog;
	TStrongObjectPtr<UProjectTextureBundleTestProducer> Producer;
	double Started;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureBundleOwnershipTest, "Project.Texture.Runtime.BundleOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureBundleOwnershipTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyBundleOwnership(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureBundleRecoveryTest, "Project.Texture.Runtime.BundleFailureRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureBundleRecoveryTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyBundleRecovery(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureBundleCancelRecoveryTest, "Project.Texture.Runtime.BundleCancellationRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureBundleCancelRecoveryTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyBundleRecovery(this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureBundleMutationShutdownTest, "Project.Texture.Runtime.BundleMutationShutdown",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureBundleMutationShutdownTest::RunTest(const FString& Parameters)
{
	if (WITH_EDITOR || !FParse::Param(FCommandLine::Get(), TEXT("ProjectTextureMutationShutdownProof")))
	{
		AddError(TEXT("Shutdown proof requires the disposable cooked fixture and explicit lifecycle flag."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyBundleMutationShutdown(this));
	return true;
}
#endif
