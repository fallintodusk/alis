// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfacePreview.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialFixtureDomain.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialPreviewAuthority.h"
#include "ProjectMaterialSurfaceBuilder.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialTerrainLayoutContract.h"
#include "Interfaces/IPluginManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetReferencingDomains.h"
#include "AssetReferencingPolicySettings.h"
#include "AssetReferencingPolicySubsystem.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Misc/PackageName.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialLiveRecipePreviewTest,
	"Project.Material.Preview.LiveRecipeRevert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialLiveRecipePreviewTest::RunTest(const FString& Parameters)
{
	UMaterialInterface* Baseline = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
	if (!TestNotNull(TEXT("Accepted fixture material is available"), Baseline)) return false;
	UMaterialInterface* RenderBinding = Baseline;
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetTransientPackage());
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Baseline));
	Component->SetStaticMesh(Mesh);
	const FString Root = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Plugins/Resources/ProjectMaterial/Data/SurfaceRecipes"));
	const FString Layout = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Plugins/World/ProjectWorldMeshTerrain/Data/TestFixtures/mesh-terrain-layout-receipt.json"));
	FString Error;
	FProjectMaterialSurfacePreview Preview;
	if (!TestTrue(TEXT("Owner recipe creates a live preview"), Preview.ApplyRecipeFile(
		FPaths::Combine(Root, TEXT("Terrain/MI_ProjectTerrain_Default.surface.json")), Root,
		Layout, Component, 0, Error)))
	{
		AddInfo(Error);
		return false;
	}
	TestTrue(TEXT("Preview is active"), Preview.IsActive());
	TestNotEqual(TEXT("Only the selected live binding changes"), Component->GetMaterial(0), RenderBinding);
	TestTrue(TEXT("Revert succeeds"), Preview.Revert(Error));
	TestEqual(TEXT("Exact original binding restored"), Component->GetMaterial(0), RenderBinding);
	TestEqual(TEXT("No explicit override added to an inherited slot"), Component->OverrideMaterials.Num(), 0);
	TestFalse(TEXT("Revert releases preview"), Preview.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialPreviewRefusalTest,
	"Project.Material.Preview.RefusalPreservesLiveState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialPreviewRefusalTest::RunTest(const FString& Parameters)
{
	UMaterialInterface* Baseline = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
	if (!TestNotNull(TEXT("Accepted material"), Baseline)) return false;
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetTransientPackage());
	Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	UMaterialInstanceDynamic* RenderBinding = UMaterialInstanceDynamic::Create(Baseline, GetTransientPackage());
	RenderBinding->SetScalarParameterValue(TEXT("NativePreviewControl"), 3.25f);
	Component->SetMaterial(0, RenderBinding);
	const FString SourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/Resources/ProjectMaterial/Data/SurfaceRecipes"));
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/preview"), FGuid::NewGuid().ToString());
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Root, false, true); };
	const FString File = FPaths::Combine(Root, TEXT("Terrain/MI_ProjectTerrain_Default.surface.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	FString Json;
	if (!TestTrue(TEXT("Read accepted recipe"), FFileHelper::LoadFileToString(Json,
		*FPaths::Combine(SourceRoot, TEXT("Terrain/MI_ProjectTerrain_Default.surface.json"))))) return false;
	const FString Layout = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Plugins/World/ProjectWorldMeshTerrain/Data/TestFixtures/mesh-terrain-layout-receipt.json"));
	FString Error;
	FProjectMaterialSurfacePreview Session;
	if (!TestTrue(TEXT("Write candidate"), FFileHelper::SaveStringToFile(Json, *File)) ||
		!TestTrue(*FString::Printf(TEXT("Apply candidate: %s"), *Error), Session.ApplyRecipeFile(File, Root, Layout, Component, 0, Error))) return false;
	UMaterialInterface* LastGood = Component->GetMaterial(0);
	float NativeValue = 0.0f;
	TestTrue(TEXT("Native renderer overrides survive"), LastGood->GetScalarParameterValue(
		FHashedMaterialParameterInfo(TEXT("NativePreviewControl")), NativeValue));
	TestEqual(TEXT("Exact native override retained"), NativeValue, 3.25f);
	TestTrue(TEXT("Write invalid candidate"), FFileHelper::SaveStringToFile(TEXT("{broken"), *File));
	TestFalse(TEXT("Malformed JSON refused"), Session.ApplyRecipeFile(File, Root, Layout, Component, 0, Error));
	TestEqual(TEXT("Refusal retains last good binding"), Component->GetMaterial(0), LastGood);
	TestTrue(TEXT("Write valid candidate again"), FFileHelper::SaveStringToFile(Json, *File));
	const bool bWasDirty = Baseline->GetOutermost()->IsDirty();
	Baseline->GetOutermost()->SetDirtyFlag(true);
	TestFalse(TEXT("Unsaved baseline refused"), Session.ApplyRecipeFile(File, Root, Layout, Component, 0, Error));
	TestTrue(TEXT("Unsaved baseline remains dirty"), Baseline->GetOutermost()->IsDirty());
	TestEqual(TEXT("Dirty refusal retains last good binding"), Component->GetMaterial(0), LastGood);
	Baseline->GetOutermost()->SetDirtyFlag(bWasDirty);
	FObjectSaveContextData SaveData;
	FCoreUObjectDelegates::OnObjectPreSave.Broadcast(Component, FObjectPreSaveContext(SaveData));
	TestFalse(TEXT("Save interruption discards transient preview"), Session.IsActive());
	TestEqual(TEXT("Save starts with exact original renderer binding"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(RenderBinding));
	TestTrue(TEXT("Preview can resume after interruption"), Session.ApplyRecipeFile(File, Root, Layout, Component, 0, Error));
	Component->SetMaterial(0, Baseline);
	TestFalse(TEXT("External binding change reported"), Session.Revert(Error));
	TestEqual(TEXT("Recovery preserves external binding"), Component->GetMaterial(0), Baseline);
	TestFalse(TEXT("Conflict releases session"), Session.IsActive());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialTerrainDetailFixtureTest,
	"Project.Material.Preview.AuthenticatedTerrainDetailFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialTerrainDetailFixtureTest::RunTest(const FString& Parameters)
{
	auto PolicyState = []()
	{
		const auto* Settings = GetDefault<UAssetReferencingPolicySettings>();
		FString Text = FString::Printf(TEXT("%d:%d:"), Settings->bUseAssetReferenceRestrictions, Settings->bIgnoreEditorOnlyReferences);
		for (const auto& Rule : Settings->AdditionalDomains)
		{
			FARPDomainDefinitionByContentRoot::StaticStruct()->ExportText(Text, &Rule, nullptr, nullptr, PPF_None, nullptr);
		}
		return Text;
	};
	const FString PolicyAtEntry = PolicyState();
	ProjectMaterialFixtureDomain::Shutdown();
	const FString PolicyBefore = PolicyState();
	const bool bRestoreExistingFixtureDomain = PolicyAtEntry != PolicyBefore;
	ON_SCOPE_EXIT
	{
		// Native save validation runs on the Editor's next tick; keep its domain alive through that work.
		const uint64 CleanupFrame = GFrameCounter + 2;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
			[this, PolicyState, PolicyAtEntry, PolicyBefore, bRestoreExistingFixtureDomain, CleanupFrame]()
		{
			if (GFrameCounter < CleanupFrame) return false;
			ProjectMaterialFixtureDomain::Shutdown();
			TestEqual(TEXT("Every fixture-test exit restores unrelated native policy"), PolicyState(), PolicyBefore);
			if (bRestoreExistingFixtureDomain)
			{
				FString RestoreError;
				TestTrue(TEXT("Restore the fixture domain present at test entry"), ProjectMaterialFixtureDomain::Register(RestoreError));
			}
			TestEqual(TEXT("Every fixture-test exit restores original settings"), PolicyState(), PolicyAtEntry);
			return true;
		}));
	};
	auto ScopedRegistration = [this](bool bExitEarly)
	{
		ON_SCOPE_EXIT { ProjectMaterialFixtureDomain::Shutdown(); };
		FString RegistrationError;
		if (!TestTrue(TEXT("Scoped fixture domain registration"), ProjectMaterialFixtureDomain::Register(RegistrationError))) return false;
		if (bExitEarly) return false;
		return true;
	};
	TestFalse(TEXT("Exercise an early return after registration"), ScopedRegistration(true));
	TestEqual(TEXT("Early return releases the fixture policy"), PolicyState(), PolicyBefore);
	TestTrue(TEXT("Exercise normal return after registration"), ScopedRegistration(false));
	TestEqual(TEXT("Normal return releases the fixture policy"), PolicyState(), PolicyBefore);
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation"), TEXT("terrain_detail_") + Suffix);
	const FString SourceRoot = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Resources/ProjectMaterial/Data/SurfaceRecipes"));
	const FString LayoutPath = FPaths::Combine(FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorldMeshTerrain/Data/TestFixtures/mesh-terrain-layout-receipt.json"));
	const FString ParentId = TEXT("M_ProjectTerrainDetail_") + Suffix;
	const FString InstanceId = TEXT("MI_ProjectTerrainDetail_") + Suffix;
	FProjectMaterialSurfaceGenerationRequest Request;
	FString Error;
	if (!TestTrue(TEXT("Confined fixture request"), FProjectMaterialSurfaceGenerationService::MakeTestRequest(Root, LayoutPath, Request, Error))) return false;
	FProjectMaterialSurfaceGenerationRequest BadRequest;
	TestFalse(TEXT("Production root is refused"), FProjectMaterialSurfaceGenerationService::MakeTestRequest(
		FPaths::ProjectContentDir(), LayoutPath, BadRequest, Error));
	FProjectMaterialSurfaceGenerationRequest ProductionAttempt = Request;
	ProductionAttempt.OutputPackageRoot = TEXT("/ProjectMaterial/Surfaces");
	FProjectMaterialSurfaceGenerationResult RefusedProduction;
	TestFalse(TEXT("Editor fixture route cannot mutate production"),
		FProjectMaterialSurfaceGenerationService::RegenerateTestMount(ProductionAttempt, RefusedProduction));
	FString InstanceJson;
	for (bool bInstance : {false, true})
	{
		const FString Name = bInstance ? TEXT("MI_ProjectTerrain_Default") : TEXT("M_ProjectTerrain");
		FString Json;
		if (!TestTrue(TEXT("Read production-shaped source recipe"), FFileHelper::LoadFileToString(Json,
			*FPaths::Combine(SourceRoot, TEXT("Terrain"), Name + TEXT(".surface.json"))))) return false;
		Json.ReplaceInline(TEXT("\"terrain_metric\""), TEXT("\"terrain_detail_metric\""));
		Json.ReplaceInline(TEXT("MI_ProjectTerrain_Default"), *InstanceId);
		Json.ReplaceInline(TEXT("M_ProjectTerrain"), *ParentId);
		Json.ReplaceInline(TEXT("\"BaseRoughness\":"),
			TEXT("\"FineScaleMeters\": 1.0, \"BumpHeightCentimeters\": 0.15, \"FineColorStrength\": 0.12, \"BaseRoughness\":"));
		const FString File = FPaths::Combine(Request.RecipeRoot, TEXT("Terrain"),
			(bInstance ? InstanceId : ParentId) + TEXT(".surface.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		if (!TestTrue(TEXT("Write isolated recipe"), FFileHelper::SaveStringToFile(Json, *File))) return false;
		if (bInstance) InstanceJson = Json;
	}
	FProjectMaterialSurfaceGenerationRequest EscapedOutput = Request;
	EscapedOutput.OutputContentRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation"));
	auto MountState = []()
	{
		FString Path;
		return FPackageName::TryConvertLongPackageNameToFilename(FString(TEXT("/ProjectMaterialTest/Surfaces/__probe")), Path)
			? Path : FString(TEXT("unmounted"));
	};
	const FString MountBeforeInvalidRoot = MountState();
	if (MountBeforeInvalidRoot == TEXT("unmounted"))
	{
		FProjectMaterialSurfaceRecipe UngeneratedRecipe;
		UMaterialInstanceConstant* Ungenerated = nullptr;
		TestFalse(TEXT("Unmapped fixture preview refuses without asserting"), ProjectMaterialPreviewAuthority::Load(
			FPaths::Combine(Request.RecipeRoot, TEXT("Terrain"), InstanceId + TEXT(".surface.json")),
			Request.RecipeRoot, LayoutPath, UngeneratedRecipe, Ungenerated, Error, Root));
		TestNull(TEXT("Unmapped fixture loads no material"), Ungenerated);
	}
	FProjectMaterialSurfaceGenerationResult RefusedOutput;
	TestFalse(TEXT("Output directory must match the admitted package mount"),
		FProjectMaterialSurfaceGenerationService::RegenerateTestMount(EscapedOutput, RefusedOutput));
	TestEqual(TEXT("Invalid output root preserves mount"),
		MountState(), MountBeforeInvalidRoot);
	TestEqual(TEXT("Invalid output root generates nothing"), RefusedOutput.Generated, 0);
	FProjectMaterialSurfaceGenerationResult Generated;
	if (!TestTrue(TEXT("Owner compiler saves and reloads detail graph"),
		FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request, Generated)))
	{
		AddInfo(Generated.Error);
		return false;
	}
	TestEqual(TEXT("Only one isolated parent and instance generated"), Generated.Generated, 2);
	TestEqual(TEXT("Only the isolated parent compiles"), Generated.ShaderCompiles, 1);
	const FString ParentPackage = TEXT("/ProjectMaterialTest/Surfaces/Terrain/") + ParentId;
	const FString InstancePackage = TEXT("/ProjectMaterialTest/Surfaces/Terrain/") + InstanceId;
	const FString HolderPackage = TEXT("/ProjectMaterialTest/Referencers/MI_Holder_") + Suffix;
	const FString SavedParent = FPackageName::LongPackageNameToFilename(ParentPackage, FPackageName::GetAssetPackageExtension());
	const FString SavedInstance = FPackageName::LongPackageNameToFilename(InstancePackage, FPackageName::GetAssetPackageExtension());
	const FString HolderFile = FPackageName::LongPackageNameToFilename(HolderPackage, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(HolderFile), true);
	if (!TestTrue(TEXT("Copy an unloaded on-disk referencer"), IFileManager::Get().Copy(*HolderFile, *SavedInstance) == COPY_OK)) return false;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanFilesSynchronous({SavedParent, SavedInstance, HolderFile}, true);
	TArray<FName> Dependencies, Referencers;
	Registry.GetDependencies(FName(InstancePackage), Dependencies);
	Registry.GetReferencers(FName(ParentPackage), Referencers);
	TestTrue(TEXT("Fixture dependencies are actually gathered"), Dependencies.Contains(FName(ParentPackage)));
	TestTrue(TEXT("Native scratch domain preserves unloaded referencer census"), Referencers.Contains(FName(HolderPackage)));
	TestNull(TEXT("Referencer census does not load the holder"), FindPackage(nullptr, *HolderPackage));
	UAssetReferencingPolicySubsystem* Policy = GEditor->GetEditorSubsystem<UAssetReferencingPolicySubsystem>();
	if (!TestNotNull(TEXT("Native reference policy remains enabled"), Policy)) return false;
	TestTrue(TEXT("Fixture parent references pass native validation"),
		Policy->ValidateAssetReferences(Registry.GetAssetByObjectPath(FSoftObjectPath(ParentPackage + TEXT(".") + ParentId))).HasValue());
	TestTrue(TEXT("Fixture instance references pass native validation"),
		Policy->ValidateAssetReferences(Registry.GetAssetByObjectPath(FSoftObjectPath(InstancePackage + TEXT(".") + InstanceId))).HasValue());
	const auto Database = Policy->GetDomainDB();
	const auto FixtureDomain = Database->FindOrAddDomainByName(TEXT("ProjectMaterialFixture"));
	const auto ProductionDomain = Database->FindOrAddDomainByName(TEXT("ProjectMaterial"));
	TestFalse(TEXT("Fixture has no unrestricted visibility"), FixtureDomain->bCanSeeEverything);
	TestFalse(TEXT("Production cannot reference disposable fixture"),
		Database->CanDomainsSeeEachOther(FixtureDomain, ProductionDomain).Get<0>());
	TestFalse(TEXT("Fixture cannot reference production material"),
		Database->CanDomainsSeeEachOther(ProductionDomain, FixtureDomain).Get<0>());
	const FString CandidateRoot = FPaths::Combine(Root, TEXT("preview"));
	const FString Candidate = FPaths::Combine(CandidateRoot, TEXT("Terrain"), InstanceId + TEXT(".surface.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Candidate), true);
	if (!TestTrue(TEXT("Write preview candidate"), FFileHelper::SaveStringToFile(InstanceJson, *Candidate))) return false;
	UMaterialInterface* Baseline = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
	if (!TestNotNull(TEXT("Accepted production terrain"), Baseline)) return false;
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Baseline));
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetTransientPackage());
	Component->SetStaticMesh(Mesh);
	UMaterialInstanceDynamic* RenderBinding = UMaterialInstanceDynamic::Create(Baseline, GetTransientPackage());
	RenderBinding->SetScalarParameterValue(TEXT("NativePreviewControl"), 3.25f);
	Component->SetMaterial(0, RenderBinding);
	FProjectMaterialSurfacePreview Session;
	if (!TestTrue(TEXT("Authenticated fixture substitutes a transient instance"), Session.ApplyFixtureRecipeFile(
		Candidate, CandidateRoot, Root, LayoutPath, Component, 0, Error)))
	{
		AddInfo(Error);
		return false;
	}
	UMaterialInterface* LastGood = Component->GetMaterial(0);
	TestNotEqual(TEXT("Rendered binding changed"), LastGood, Baseline);
	float NativeValue = 0.0f;
	TestTrue(TEXT("Fixture retains native renderer override"), LastGood->GetScalarParameterValue(
		FHashedMaterialParameterInfo(TEXT("NativePreviewControl")), NativeValue));
	TestEqual(TEXT("Native override identity"), NativeValue, 3.25f);
	FString Tuned = InstanceJson.Replace(TEXT("\"BumpHeightCentimeters\": 0.15"), TEXT("\"BumpHeightCentimeters\": 0.3"));
	TestTrue(TEXT("Write changed preview parameter"), FFileHelper::SaveStringToFile(Tuned, *Candidate));
	TestTrue(TEXT("Parameters tune without regenerating fixture"), Session.ApplyFixtureRecipeFile(
		Candidate, CandidateRoot, Root, LayoutPath, Component, 0, Error));
	LastGood = Component->GetMaterial(0);
	float Height = 0.0f;
	TestTrue(TEXT("Fine normal parameter is consumed"), LastGood->GetScalarParameterValue(
		FHashedMaterialParameterInfo(TEXT("BumpHeightCentimeters")), Height));
	TestEqual(TEXT("Exact candidate height"), Height, 0.3f);
	TestTrue(TEXT("Write invalid candidate"), FFileHelper::SaveStringToFile(TEXT("{broken"), *Candidate));
	TestFalse(TEXT("Invalid recipe refuses"), Session.ApplyFixtureRecipeFile(Candidate, CandidateRoot, Root, LayoutPath, Component, 0, Error));
	TestEqual(TEXT("Refusal preserves last good fixture binding"), Component->GetMaterial(0), LastGood);
	TestTrue(TEXT("Restore candidate"), FFileHelper::SaveStringToFile(Tuned, *Candidate));
	const FString Manifest = FPaths::Combine(Request.ManifestRoot, TEXT("accepted.surface-manifest.json"));
	TArray<uint8> ManifestBytes;
	if (!TestTrue(TEXT("Read manifest bytes"), FFileHelper::LoadFileToArray(ManifestBytes, *Manifest))) return false;
	TestTrue(TEXT("Inject corrupt authority"), FFileHelper::SaveStringToFile(TEXT("{broken"), *Manifest));
	TestFalse(TEXT("Corrupt fixture authority refuses"), Session.ApplyFixtureRecipeFile(Candidate, CandidateRoot, Root, LayoutPath, Component, 0, Error));
	TestEqual(TEXT("Corrupt authority preserves binding"), Component->GetMaterial(0), LastGood);
	TestTrue(TEXT("Restore exact manifest bytes"), FFileHelper::SaveArrayToFile(ManifestBytes, *Manifest));
	const FString ParentFile = FPackageName::LongPackageNameToFilename(
		TEXT("/ProjectMaterialTest/Surfaces/Terrain/") + ParentId, FPackageName::GetAssetPackageExtension());
	if (!TestTrue(TEXT("Parent package stays within fixture"), FPaths::IsUnderDirectory(ParentFile, Root))) return false;
	TArray<uint8> ParentBytes;
	if (!TestTrue(TEXT("Read parent bytes"), FFileHelper::LoadFileToArray(ParentBytes, *ParentFile)) ||
		!TestTrue(TEXT("Parent has bytes"), !ParentBytes.IsEmpty())) return false;
	FObjectSaveContextData SaveData;
	FCoreUObjectDelegates::OnObjectPreSave.Broadcast(Component, FObjectPreSaveContext(SaveData));
	TestFalse(TEXT("Save interruption releases fixture preview"), Session.IsActive());
	TestEqual(TEXT("Save interruption restores renderer"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(RenderBinding));
	TestTrue(TEXT("Fixture preview resumes after interruption"), Session.ApplyFixtureRecipeFile(
		Candidate, CandidateRoot, Root, LayoutPath, Component, 0, Error));
	TestTrue(TEXT("Fixture revert"), Session.Revert(Error));
	TestEqual(TEXT("Production renderer binding restored"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(RenderBinding));
	TestEqual(TEXT("Original explicit override count restored"), Component->OverrideMaterials.Num(), 1);
	TestFalse(TEXT("Production preview cannot silently substitute fixture graph"), Session.ApplyRecipeFile(
		Candidate, CandidateRoot, LayoutPath, Component, 0, Error));
	FProjectMaterialSurfaceRecipe Authenticated;
	UMaterialInstanceConstant* Fixture = nullptr;
	if (!TestTrue(TEXT("Reload fixture authority after refusal controls"), ProjectMaterialPreviewAuthority::Load(
		Candidate, CandidateRoot, LayoutPath, Authenticated, Fixture, Error, Root))) return false;
	FProjectMaterialPatternDependency Pattern;
	FProjectMaterialTerrainLayoutContract Layout;
	if (!TestTrue(TEXT("Resolve accepted pattern"), FProjectMaterialPatternAuthority::Resolve(Authenticated.PatternId,
		Request.PatternManifestPath, Request.PatternPackageRoot, Request.PatternContentRoot, Pattern, Error)) ||
		!TestTrue(TEXT("Resolve accepted layout"), FProjectMaterialTerrainLayoutCompiler::CompileFile(LayoutPath, FString(), Layout, Error))) return false;
	UMaterial* Parent = Cast<UMaterial>(Fixture->Parent);
	if (!TestNotNull(TEXT("Fixture parent has a graph"), Parent)) return false;
	for (UMaterialExpression* Expression : UMaterialEditingLibrary::GetMaterialExpressions(Parent))
	{
		if (auto* Slab = Cast<UMaterialExpressionSubstrateSlabBSDF>(Expression))
		{
			const FExpressionInput Saved = Slab->Normal;
			Slab->Normal.Expression = nullptr;
			Authenticated.ArtifactKind = EProjectMaterialSurfaceArtifactKind::Parent;
			TestFalse(TEXT("Disconnected normal fails closed graph verification"), ProjectMaterialSurfaceBuilder::Verify(
				Authenticated, Pattern, Layout.SemanticChannels, Parent, Error));
			Slab->Normal = Saved;
		}
	}
	FProjectMaterialSurfaceGenerationResult NoOp;
	TestTrue(TEXT("Fixture remains accepted"), FProjectMaterialSurfaceGenerationService::Validate(Request, NoOp));
	TestEqual(TEXT("Validation recompiles zero graphs"), NoOp.ShaderCompiles, 0);
	// A disk-identical fixture in another root must not borrow already loaded UObjects.
	const FString OtherRoot = Root + TEXT("_collision");
	TestTrue(TEXT("Copy disposable fixture to another root"), FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*OtherRoot, *Root, true));
	FProjectMaterialSurfaceGenerationRequest OtherRequest;
	if (!TestTrue(TEXT("Second confined request"), FProjectMaterialSurfaceGenerationService::MakeTestRequest(
		OtherRoot, LayoutPath, OtherRequest, Error))) return false;
	const FString MountBefore = FPackageName::LongPackageNameToFilename(TEXT("/ProjectMaterialTest/Surfaces/__probe"));
	const bool bParentDirty = Parent->GetOutermost()->IsDirty();
	const bool bInstanceDirty = Fixture->GetOutermost()->IsDirty();
	const int32 ExpressionCount = UMaterialEditingLibrary::GetMaterialExpressions(Parent).Num();
	FProjectMaterialSurfaceGenerationResult Collision;
	TestFalse(TEXT("Same IDs from a different loaded root refuse generation"),
		FProjectMaterialSurfaceGenerationService::RegenerateTestMount(OtherRequest, Collision));
	TestEqual(TEXT("Collision refusal leaves mount unchanged"),
		FPackageName::LongPackageNameToFilename(TEXT("/ProjectMaterialTest/Surfaces/__probe")), MountBefore);
	TestEqual(TEXT("Collision refusal generates nothing"), Collision.Generated, 0);
	FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"), FPaths::Combine(OtherRoot, TEXT("content/")));
	ON_SCOPE_EXIT { FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"), FPaths::Combine(Root, TEXT("content/"))); };
	const FString OtherParentFile = FPaths::Combine(OtherRequest.OutputContentRoot, TEXT("Terrain"), ParentId + TEXT(".uasset"));
	TArray<uint8> Tampered = ParentBytes;
	Tampered[0] ^= 1;
	ON_SCOPE_EXIT { FFileHelper::SaveArrayToFile(ParentBytes, *OtherParentFile); };
	if (!TestTrue(TEXT("Inject wrong bytes only into unloaded copy"), FFileHelper::SaveArrayToFile(Tampered, *OtherParentFile))) return false;
	TestFalse(TEXT("Wrong fixture package bytes refuse"), Session.ApplyFixtureRecipeFile(Candidate, CandidateRoot, OtherRoot, LayoutPath, Component, 0, Error));
	TestEqual(TEXT("Wrong bytes preserve renderer binding"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(RenderBinding));
	if (!TestTrue(TEXT("Restore exact copied parent bytes"), FFileHelper::SaveArrayToFile(ParentBytes, *OtherParentFile))) return false;
	TestFalse(TEXT("Disk authentication cannot borrow another root's loaded instance"), Session.ApplyFixtureRecipeFile(
		Candidate, CandidateRoot, OtherRoot, LayoutPath, Component, 0, Error));
	Session.Revert(Error);
	TestEqual(TEXT("Cross-root refusal preserves renderer binding"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(RenderBinding));
	TestEqual(TEXT("Loaded instance identity unchanged"), FindObject<UMaterialInstanceConstant>(nullptr, *Fixture->GetPathName()), Fixture);
	TestEqual(TEXT("Loaded parent identity unchanged"), FindObject<UMaterial>(nullptr, *Parent->GetPathName()), Parent);
	TestEqual(TEXT("Parent dirty flag unchanged"), Parent->GetOutermost()->IsDirty(), bParentDirty);
	TestEqual(TEXT("Instance dirty flag unchanged"), Fixture->GetOutermost()->IsDirty(), bInstanceDirty);
	TestEqual(TEXT("Loaded graph unchanged"), UMaterialEditingLibrary::GetMaterialExpressions(Parent).Num(), ExpressionCount);
	for (const FString& Relative : {FString(TEXT("manifests/accepted.surface-manifest.json")),
		FString(TEXT("content/Surfaces/Terrain/")) + ParentId + TEXT(".uasset"),
		FString(TEXT("content/Surfaces/Terrain/")) + InstanceId + TEXT(".uasset")})
	{
		TArray<uint8> Original, Copied;
		TestTrue(TEXT("Read original fixture bytes"), FFileHelper::LoadFileToArray(Original, *FPaths::Combine(Root, Relative)));
		TestTrue(TEXT("Read second fixture bytes"), FFileHelper::LoadFileToArray(Copied, *FPaths::Combine(OtherRoot, Relative)));
		TestTrue(TEXT("Collision leaves both fixtures byte identical"), Original == Copied);
	}
	AddInfo(TEXT("Isolated detail fixture: ") + Root);
	ProjectMaterialFixtureDomain::Shutdown();
	TestEqual(TEXT("Fixture domain removal preserves existing policy and switches"), PolicyState(), PolicyBefore);
	TestTrue(TEXT("Fixture domain can resume after verified removal"), ProjectMaterialFixtureDomain::Register(Error));
	return true;
}
#endif
