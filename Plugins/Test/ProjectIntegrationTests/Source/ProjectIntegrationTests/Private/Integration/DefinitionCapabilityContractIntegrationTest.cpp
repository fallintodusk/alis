// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"

#include "AssetRegistry/AssetBundleData.h"
#include "CapabilityRegistry.h"
#include "Data/ObjectDefinition.h"
#include "DefinitionTypeInfo.h"
#include "DefinitionValidator.h"
#include "Interfaces/IAssemblyViewConfigSource.h"
#include "Spawning/ObjectSpawnUtility.h"
#include "Support/DefinitionCapabilityContractTestDoubles.h"
#include "Template/Interactable/InteractableActor.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA

namespace
{
	FObjectCapabilityEntry MakeCapability(
		const FName Type,
		const TMap<FName, FString>& Properties)
	{
		FObjectCapabilityEntry Entry;
		Entry.Type = Type;
		Entry.Scope = { NAME_CapabilityScope_Actor };
		Entry.Properties = Properties;
		return Entry;
	}

	bool HasErrorContaining(
		const TArray<FDefinitionValidationError>& Errors,
		const FString& Needle)
	{
		return Errors.ContainsByPredicate([&Needle](const FDefinitionValidationError& Error)
		{
			return !Error.bIsWarning
				&& (Error.PropertyPath.Contains(Needle) || Error.Message.Contains(Needle));
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefinitionCapabilityContract_RuntimeRejectsInvalidDefinitionTest,
	"ProjectIntegrationTests.ObjectDefinition.Capabilities.RuntimeRejectsInvalidDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefinitionCapabilityContract_RuntimeRejectsInvalidDefinitionTest::RunTest(
	const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!World && GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	TestNotNull(TEXT("Automation world must exist"), World);
	if (!World)
	{
		return false;
	}

	UObjectDefinition* Definition = NewObject<UObjectDefinition>(GetTransientPackage());
	Definition->ObjectId = TEXT("InvalidRuntimeCapabilityContract");
	Definition->Capabilities.Add(MakeCapability(TEXT("MissingCapability"), {}));

	AddExpectedError(
		TEXT("Invalid capability definition"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World, Definition, FTransform::Identity, FActorSpawnParameters(), &SpawnError);
	TestNull(TEXT("Initial spawn must reject an unknown capability"), Spawned);
	TestTrue(TEXT("Initial spawn failure must be observable"), !SpawnError.IsEmpty());

	AInteractableActor* Existing = World->SpawnActor<AInteractableActor>();
	TestNotNull(TEXT("Reapply fixture actor must spawn"), Existing);
	if (!Existing)
	{
		return false;
	}
	AddExpectedError(
		TEXT("ApplyDefinition rejected invalid capability data"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(TEXT("Definition reapply must reject an unknown capability"),
		Existing->ApplyDefinition_Implementation(Definition));
	Existing->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefinitionViewConfigContract_SpawnCarriesNeckOffsetTest,
	"ProjectIntegrationTests.ObjectDefinition.ViewConfig.SpawnCarriesNeckOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefinitionViewConfigContract_SpawnCarriesNeckOffsetTest::RunTest(
	const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!World && GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	TestNotNull(TEXT("Automation world must exist"), World);
	if (!World)
	{
		return false;
	}

	UObjectDefinition* Definition = NewObject<UObjectDefinition>(GetTransientPackage());
	Definition->ObjectId = TEXT("ViewConfigContract");
	Definition->Capabilities.Add(MakeCapability(TEXT("SkeletalAssembly"), {}));

	FViewSection AuthoredView;
	AuthoredView.RelativeOffset = FVector(23.f, 0.f, 73.f);
	AuthoredView.NeckOffset = FVector(-24.19f, 0.f, -17.24f);
	FInstancedStruct ViewSection;
	ViewSection.InitializeAs(FViewSection::StaticStruct());
	*ViewSection.GetMutablePtr<FViewSection>() = AuthoredView;
	Definition->Sections.Add(ObjectSectionIds::View, MoveTemp(ViewSection));

	FText SpawnError;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World, Definition, FTransform::Identity, FActorSpawnParameters(), &SpawnError);
	TestNotNull(TEXT("View-config fixture must spawn"), Spawned);
	if (!Spawned)
	{
		AddError(SpawnError.ToString());
		return false;
	}

	IAssemblyViewConfigSource* ViewConfigSource = nullptr;
	TArray<UActorComponent*> Components;
	Spawned->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (IAssemblyViewConfigSource* Candidate = Cast<IAssemblyViewConfigSource>(Component))
		{
			ViewConfigSource = Candidate;
			break;
		}
	}
	TestNotNull(TEXT("SkeletalAssembly must expose the view-config contract"), ViewConfigSource);
	if (ViewConfigSource)
	{
		FAssemblyViewConfig RuntimeView;
		TestTrue(TEXT("Spawn must populate the assembly view config"),
			ViewConfigSource->GetViewConfig(RuntimeView));
		TestEqual(TEXT("Camera offset must survive definition composition"),
			RuntimeView.RelativeOffset, AuthoredView.RelativeOffset);
		TestEqual(TEXT("Neck offset must survive definition composition"),
			RuntimeView.NeckOffsetFromCamera, AuthoredView.NeckOffset);
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefinitionCapabilityContract_ValidationRejectsUnknownIdentityAndPropertyTest,
	"ProjectIntegrationTests.ObjectDefinition.Capabilities.ValidationRejectsUnknownIdentityAndProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefinitionCapabilityContract_ValidationRejectsUnknownIdentityAndPropertyTest::RunTest(
	const FString& Parameters)
{
	UObjectDefinition* Definition = NewObject<UObjectDefinition>(GetTransientPackage());
	Definition->ObjectId = TEXT("CapabilityValidationContract");
	Definition->Capabilities.Add(MakeCapability(TEXT("MissingCapability"), {}));
	Definition->Capabilities.Add(MakeCapability(
		TEXT("DefinitionCapabilityContractTest"),
		{{ TEXT("MissingProperty"), TEXT("42") }}));
	Definition->Capabilities.Add(MakeCapability(
		TEXT("DefinitionCapabilityContractTest"),
		{{ TEXT("PrimaryAsset"), TEXT("/Script/Engine.Actor") }}));
	Definition->Capabilities.Add(MakeCapability(
		TEXT("DefinitionCapabilityContractTest"),
		{{ TEXT("PrimaryAsset"), TEXT("/MissingMount/Asset.Asset") }}));

	TArray<FDefinitionValidationError> Errors;
	FDefinitionValidator::ValidateObjectCapabilities(
		Definition,
		Definition->ObjectId.ToString(),
		Errors);

	TestTrue(TEXT("Unknown capability identity must be an error"),
		HasErrorContaining(Errors, TEXT("MissingCapability")));
	TestTrue(TEXT("Unknown declared property must be an error"),
		HasErrorContaining(Errors, TEXT("MissingProperty")));
	TestTrue(TEXT("Native script paths must be rejected for content properties"),
		HasErrorContaining(Errors, TEXT("/Script/Engine.Actor")));
	TestTrue(TEXT("Unmounted content paths must be rejected"),
		HasErrorContaining(Errors, TEXT("/MissingMount/Asset.Asset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefinitionCapabilityContract_BundleUsesReflectedSoftPropertiesTest,
	"ProjectIntegrationTests.ObjectDefinition.Capabilities.BundleUsesReflectedSoftProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefinitionCapabilityContract_BundleUsesReflectedSoftPropertiesTest::RunTest(
	const FString& Parameters)
{
	UProjectCapabilityBundleTestDefinition* Definition =
		NewObject<UProjectCapabilityBundleTestDefinition>(GetTransientPackage());
	Definition->ObjectId = TEXT("CapabilityBundleContract");
	Definition->Capabilities.Add(MakeCapability(
		TEXT("DefinitionCapabilityContractTest"),
		{
			{ TEXT("PrimaryAsset"), TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture") },
			{ TEXT("RelatedAssets"), TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture;/Engine/EngineResources/DefaultMaterial.DefaultMaterial") },
			{ TEXT("GeneratedClass"), TEXT("/ProjectSkeletalCapabilities/MotionMatching/ABP_MotionMatchingBridge.ABP_MotionMatchingBridge_C") },
			{ TEXT("PathLookingText"), TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture") },
			{ TEXT("Enabled"), TEXT("true") }
		}));

	Definition->UpdateAssetBundleData();
	FAssetBundleEntry* DefaultBundle =
		Definition->GetMutableAssetBundleData().FindEntry(TEXT("Default"));

	TestNotNull(TEXT("Capability soft references must create a Default bundle"), DefaultBundle);
	if (!DefaultBundle)
	{
		return false;
	}

	const FTopLevelAssetPath TexturePath = FSoftObjectPath(
		TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")).GetAssetPath();
	const FTopLevelAssetPath MaterialPath = FSoftObjectPath(
		TEXT("/Engine/EngineResources/DefaultMaterial.DefaultMaterial")).GetAssetPath();
	const FTopLevelAssetPath GeneratedClassPath = FSoftObjectPath(
		TEXT("/ProjectSkeletalCapabilities/MotionMatching/ABP_MotionMatchingBridge.ABP_MotionMatchingBridge_C")).GetAssetPath();

	TestTrue(TEXT("Scalar soft-object property must be bundled"),
		DefaultBundle->AssetPaths.Contains(TexturePath));
	TestTrue(TEXT("Semicolon soft-object array entries must be bundled"),
		DefaultBundle->AssetPaths.Contains(MaterialPath));
	TestTrue(TEXT("Soft-class properties must be bundled by reflected type"),
		DefaultBundle->AssetPaths.Contains(GeneratedClassPath));
	TestEqual(TEXT("Canonical duplicate paths must be collapsed"),
		DefaultBundle->AssetPaths.FilterByPredicate([&TexturePath](const FTopLevelAssetPath& Path)
		{
			return Path == TexturePath;
		}).Num(),
		1);
	TestEqual(TEXT("Ordinary path-looking strings must not add bundle entries"),
		DefaultBundle->AssetPaths.Num(),
		3);

	return true;
}

#endif
