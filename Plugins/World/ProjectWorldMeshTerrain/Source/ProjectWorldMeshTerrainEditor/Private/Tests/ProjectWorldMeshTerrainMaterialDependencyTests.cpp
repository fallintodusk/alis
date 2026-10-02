// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Materials/MaterialInstanceConstant.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionDependencyInterface.h"
#include "Misc/AutomationTest.h"
#include "ProjectWorldMeshTerrainProducer.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldMeshTerrainMaterialDependencyTests
{
using UE::MeshPartition::EDependencyFlags;

// Records what a definition declares when Mesh Partition decides whether compiled sections are
// current: dependent data bytes plus every object or package declared as a dependency.
struct FRecordingDependencies final : UE::MeshPartition::IDependencyInterface
{
	TArray<uint8> Data;
	FMemoryWriter Writer{Data};
	TArray<FString> Objects;

	void AddPackageDependency(const UObject* Object, EDependencyFlags) override
	{
		Record(Object);
	}
	void AddPackageDependency(TArrayView<UObject*> ObjectArray) override
	{
		for (const UObject* Object : ObjectArray)
		{
			Record(Object);
		}
	}
	void AddClassDependency(const UClass*) override {}
	FArchive& GetDependentDataArchive() override { return Writer; }
	void operator+=(const UDynamicMesh&) override { Objects.Add(TEXT("UDynamicMesh")); }
	void operator+=(const UStaticMesh&) override { Objects.Add(TEXT("UStaticMesh")); }
	void operator+=(const UTexture&) override { Objects.Add(TEXT("UTexture")); }
	void operator+=(const UTexture2D&) override { Objects.Add(TEXT("UTexture2D")); }
	void operator+=(const UCurveFloat&) override { Objects.Add(TEXT("UCurveFloat")); }
	void operator+=(const USplineComponent&) override { Objects.Add(TEXT("USplineComponent")); }
	void operator+=(const UObject& Object) override { Record(&Object); }

	void Record(const UObject* Object)
	{
		if (Object != nullptr)
		{
			Objects.Add(Object->GetPathName());
			Objects.Add(Object->GetOutermost()->GetName());
		}
	}
};

bool ContainsSerializedPath(const TArray<uint8>& Data, FString Path)
{
	TArray<uint8> Needle;
	FMemoryWriter PathWriter(Needle);
	PathWriter << Path;
	for (int32 Start = 0; Start + Needle.Num() <= Data.Num(); ++Start)
	{
		if (FMemory::Memcmp(Data.GetData() + Start, Needle.GetData(), Needle.Num()) == 0)
		{
			return true;
		}
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainMaterialDependencyTest,
	"Project.World.MeshTerrain.MaterialDependencyIsPathOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldMeshTerrainMaterialDependencyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldMeshTerrainMaterialDependencyTests;
	using UE::MeshPartition::UMeshPartitionDefinition;
	const UMeshPartitionDefinition* Shared = LoadObject<UMeshPartitionDefinition>(
		nullptr, ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
	UMaterialInstanceConstant* Terrain = LoadObject<UMaterialInstanceConstant>(
		nullptr, TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
	if (!TestNotNull(TEXT("The shared terrain definition loads."), Shared) ||
		!TestNotNull(TEXT("The terrain material instance loads."), Terrain))
	{
		return false;
	}

	// Transient copies keep the persistent definition and material untouched.
	UMeshPartitionDefinition* Definition = DuplicateObject(Shared, GetTransientPackage());
	UMaterialInstanceConstant* Probe = DuplicateObject(
		Terrain, GetTransientPackage(), TEXT("MI_ProjectTerrain_DependencyProbe"));
	FObjectPropertyBase* MaterialProperty = FindFProperty<FObjectPropertyBase>(
		Definition->GetClass(), TEXT("Material"));
	if (!TestNotNull(TEXT("The definition exposes its material property."), MaterialProperty))
	{
		return false;
	}
	const TConstArrayView<UE::MeshPartition::FCompiledSectionBuildVariant> Variants =
		Definition->GetCompiledSectionBuildVariants();
	const FName Variant = Variants.IsEmpty() ? NAME_Default : Variants[0].Name;

	MaterialProperty->SetObjectPropertyValue_InContainer(Definition, Probe);
	FRecordingDependencies Before;
	Definition->GatherDependencies(Before, Variant);

	const FMaterialParameterInfo Macro(TEXT("MacroStrength"));
	Probe->SetScalarParameterValueEditorOnly(Macro, 1.0f);
	float Changed = 0.0f;
	TestTrue(TEXT("The transient appearance change is applied to the material."),
		Probe->GetScalarParameterValue(Macro, Changed) && FMath::IsNearlyEqual(Changed, 1.0f));
	FRecordingDependencies AfterContent;
	Definition->GatherDependencies(AfterContent, Variant);

	MaterialProperty->SetObjectPropertyValue_InContainer(Definition, Terrain);
	FRecordingDependencies AfterPath;
	Definition->GatherDependencies(AfterPath, Variant);

	TestTrue(TEXT("The material enters the section dependencies as its object path."),
		ContainsSerializedPath(Before.Data, Probe->GetPathName()));
	TestFalse(TEXT("The material is never declared as an object or package dependency."),
		Before.Objects.Contains(Probe->GetPathName()) ||
		Before.Objects.Contains(Probe->GetOutermost()->GetName()) ||
		AfterPath.Objects.Contains(Terrain->GetPathName()) ||
		AfterPath.Objects.Contains(Terrain->GetOutermost()->GetName()));
	TestTrue(TEXT("A material appearance change leaves the section dependencies identical."),
		Before.Data == AfterContent.Data && Before.Objects == AfterContent.Objects);
	TestFalse(TEXT("A material path change alters the section dependencies."),
		Before.Data == AfterPath.Data);

	Definition->MarkAsGarbage();
	Probe->MarkAsGarbage();
	return true;
}

#endif
