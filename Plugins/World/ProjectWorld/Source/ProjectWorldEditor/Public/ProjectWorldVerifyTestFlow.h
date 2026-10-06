// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldVerify.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// One producer verify: identity first, then realize into the test mount, save, reload, project,
// and compare, record, or probe as the command line selects.
namespace ProjectWorldVerifyTestFlow
{
	struct FSpec
	{
		const TCHAR* Verify = nullptr;
		const TCHAR* Folder = nullptr;
		const TCHAR* GeneratorId = nullptr;
		int32 GeneratorVersion = 0;

		FString ArtifactRoot() const
		{
			return FString::Printf(TEXT("/ProjectWorldVerify/%s/"), Folder);
		}
	};

	inline FString SerializeLayer(const FProjectWorldRealizationLayer& Layer)
	{
		return FString::Printf(
			TEXT("layer_id=%s\nkind=%d\ngenerator=%s:v%d\ndepends_on=%s\nselectors=%s\nartifact_root=%s\n")
			TEXT("spatial_ownership=%s\ndirty_granularity=%d\nhalo=%d\nruntime_mapping=%s\nsettings=%s\n"),
			*Layer.LayerId, static_cast<int32>(Layer.LayerKind), *Layer.GeneratorId, Layer.GeneratorVersion,
			*FString::Join(Layer.DependsOn, TEXT(",")), *FString::Join(Layer.CanonicalSelectors, TEXT(",")),
			*Layer.ArtifactRoot, *Layer.SpatialOwnership, static_cast<int32>(Layer.DirtyGranularity),
			Layer.DependencyHaloCells, *Layer.RuntimeMapping, *Layer.NormalizedSettings);
	}

	/** In-code layers derive their contract hash from their own fields, as production does from the profile. */
	inline FProjectWorldRealizationLayer Finalize(FProjectWorldRealizationLayer Layer)
	{
		Layer.ContractHash = ProjectWorldProjection::HashText(SerializeLayer(Layer));
		return Layer;
	}

	inline FString SerializeProfile(const FProjectWorldRealizationProfile& Profile)
	{
		FString Text = FString::Printf(TEXT("profile=%s|%s|%s\n"),
			*Profile.ProfileId, *Profile.WorldDataPluginName, *Profile.CanonicalProfileId);
		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			Text += SerializeLayer(Layer) + TEXT("contract=") + Layer.ContractHash + TEXT("\n");
		}
		return Text;
	}

	inline FProjectWorldLayerInventory WholeLayer(const FProjectWorldRealizationLayer& Layer)
	{
		FProjectWorldLayerInventory Inventory;
		Inventory.LayerId = Layer.LayerId;
		Inventory.GeneratorId = Layer.GeneratorId;
		Inventory.GeneratorVersion = Layer.GeneratorVersion;
		Inventory.ArtifactRoot = Layer.ArtifactRoot;
		Inventory.NormalizedLayerContractHash = Layer.ContractHash;
		Inventory.FinalDirtyUnits.Add(TEXT("*"));
		return Inventory;
	}

	using FFixtures = TFunctionRef<bool(FProjectWorldVerifyIdentity&, FString&)>;
	using FRealize = TFunctionRef<bool(UWorld*, FString&)>;

	inline bool Run(FAutomationTestBase& Test, const FSpec& Spec, FFixtures AddFixtures, FRealize Realize)
	{
		int32 ContractVersion = 0;
		FProjectWorldProjectFn Projection;
		if (!ProjectWorldVerify::FindProjection(Spec.GeneratorId, Spec.GeneratorVersion, ContractVersion, Projection))
		{
			Test.AddError(FString::Printf(TEXT("%s: no projection is registered for %s:v%d."),
				Spec.Verify, Spec.GeneratorId, Spec.GeneratorVersion));
			return false;
		}
		FProjectWorldVerifyIdentity Identity;
		FString Error;
		if (!FProjectWorldVerifyIdentity::ForProducer(
				Spec.Verify, Spec.GeneratorId, Spec.GeneratorVersion, ContractVersion, Identity, Error) ||
			!AddFixtures(Identity, Error))
		{
			Test.AddError(FString::Printf(TEXT("%s: verification identity: %s"), Spec.Verify, *Error));
			return false;
		}
		FProjectWorldVerifyWorld World(Spec.Folder);
		FProjectWorldProjection Output;
		if (World.IsReady(Error) && World.GetRoot() != Spec.ArtifactRoot())
		{
			Error = TEXT("verify world root differs from the fixture artifact root: ") + World.GetRoot();
		}
		if (!Error.IsEmpty() || !World.IsReady(Error) || !Realize(World.GetWorld(), Error) ||
			!World.SaveOwned(Error) || !World.Reload(Error) ||
			!Projection(FProjectWorldVerifyScope{World.GetWorld(), World.GetRoot()}, Output, Error))
		{
			Test.AddError(FString::Printf(TEXT("%s: %s"), Spec.Verify, *Error));
			return false;
		}
		FString ProbeDir;
		const ProjectWorldVerify::EMode Mode = ProjectWorldVerify::ModeFromCommandLine(ProbeDir);
		const ProjectWorldVerify::FResult Result = Mode == ProjectWorldVerify::EMode::Probe
			? ProjectWorldVerify::WriteProbe(Identity, Output, World.PackageDigests(), ProbeDir)
			: ProjectWorldVerify::CompareOrRecord(Identity, Output, Mode);
		if (Result.bPassed)
		{
			Test.AddInfo(Result.Message);
		}
		else
		{
			Test.AddError(Result.Message);
		}
		return Result.bPassed;
	}
}

#endif
