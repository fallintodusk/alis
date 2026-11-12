#include "ProjectWorldMeshTerrainStorageTransformer.h"
#include "ProjectWorldMeshTerrainPartition.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshAttributeUtil.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionMeshData.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	TSharedPtr<const UE::MeshPartition::FMeshData> SourceTriangle(const FVector& A, const FVector& B, const FVector& C)
	{
		using namespace UE::Geometry;
		FDynamicMesh3 Mesh;
		Mesh.EnableVertexUVs(FVector2f::ZeroVector);
		for (const FVector& P : {A, B, C})
		{
			const int32 I = Mesh.AppendVertex(P);
			Mesh.SetVertexUV(I, FVector2f(.125f * (I + 1), .75f));
		}
		Mesh.AppendTriangle(0, 1, 2);
		Mesh.EnableAttributes();
		CopyVertexUVsToOverlay(Mesh, *Mesh.Attributes()->PrimaryUV());
		Mesh.Attributes()->SetNumWeightLayers(2);
		for (int32 Layer = 0; Layer < 2; ++Layer)
		{
			auto* Attribute = Mesh.Attributes()->GetWeightLayer(Layer);
			Attribute->SetName(Layer == 0 ? TEXT("ground") : TEXT("hydro_transition"));
			Attribute->Initialize();
			const float Value = Layer == 0 ? .25f : .75f;
			for (int32 I : Mesh.VertexIndicesItr()) Attribute->SetValue(I, &Value);
		}
		auto Native = MakeShared<UE::MeshPartition::FMeshData>();
		Native->AppendDynamicMesh(Mesh, FTransform::Identity);
		Native->RecomputeNormals();
		return Native;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldTerrainSectionStorageContract,
	"Project.World.Realization.MeshTerrain.SectionStorageContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldTerrainSectionStorageContract::RunTest(const FString&)
{
	using namespace UE::MeshPartition;
	const auto Init = UWorld::InitializationValues().CreateWorldPartition(false).EnableWorldPartitionStreaming(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("SectionStorageContract"), nullptr, true, ERHIFeatureLevel::Num, &Init);
	if (!TestNotNull(TEXT("The storage contract owns a separate disposable editor world."), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); World->RemoveFromRoot(); };
	auto* Parent = World->SpawnActor<AProjectWorldMeshTerrainPartition>();
	auto* Section = World->SpawnActor<ACompiledSection>();
	if (!TestNotNull(TEXT("Native parent exists."), Parent) || !TestNotNull(TEXT("Native compiled section exists."), Section)) return false;
	Parent->SetActorTransform(FTransform(FVector(120000, -430000, 1200)));
	Section->SetParent(Parent);
	const TArray<TSharedPtr<const FMeshData>> Inputs = {
		SourceTriangle(FVector(100, 200, 20), FVector(500, 200, 21), FVector(500, 600, 22)),
		SourceTriangle(FVector(500, 200, 21), FVector(900, 200, 23), FVector(500, 600, 22))};
	FProjectWorldMeshTerrainStorageTransformer Transformer;
	FTransform FirstFrame;
	TArray<TSharedPtr<const FMeshData>> FirstOutput;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		FTransformerContext Context;
		for (const auto& Input : Inputs) Context.TransformerUnits.Add(MakeTransformerUnit(Section, Input));
		if (!TestTrue(TEXT("Fresh parent-local input can compile on a previously translated section."), Transformer.Execute(Context)) ||
			!TestFalse(TEXT("Successful centering is not cancelled."), Context.bWasCancelled)) return false;
		const FTransform Expected = FTransform(FVector(500, 400, 0)) * Parent->GetActorTransform();
		if (!TestTrue(TEXT("All units share one compensated section frame from the parent, not the previous section."),
			Section->GetActorTransform().Equals(Expected, 0))) return false;
		for (int32 Unit = 0; Unit < Inputs.Num(); ++Unit)
		{
			const auto& Input = *Inputs[Unit];
			const auto& Output = *Context.TransformerUnits[Unit].MeshData;
			TestTrue(TEXT("The cached source mesh remains a distinct immutable object."), &Input != &Output);
			TestEqual(TEXT("All source triangles are retained."), Output.TriangleCount(), Input.TriangleCount());
			TestTrue(TEXT("Both named semantic channels survive."), Output.GetWeightLayerNames() == Input.GetWeightLayerNames());
			for (int32 I : Input.VertexIndicesItr())
			{
				TestTrue(TEXT("Decoded world positions agree with the original parent-local source."),
					Section->GetActorTransform().TransformPosition(Output.GetVertex(I)).Equals(Parent->GetActorTransform().TransformPosition(Input.GetVertex(I)), 1.e-6));
				TestTrue(TEXT("UVs and normals survive the storage translation."),
					Output.GetVertexUV(I, 0) == Input.GetVertexUV(I, 0) && Output.GetVertexNormal(I) == Input.GetVertexNormal(I));
				for (const auto Name : Input.GetWeightLayerNames()) TestEqual(TEXT("Every semantic value is unchanged."),
					Output.GetWeightLayerValue(Name, I), Input.GetWeightLayerValue(Name, I));
				if (Pass == 1) TestTrue(TEXT("Repeated compilation cannot accumulate translation in stored vertices."),
					Output.GetVertex(I) == FirstOutput[Unit]->GetVertex(I));
			}
			if (Pass == 0) FirstOutput.Add(Context.TransformerUnits[Unit].MeshData);
		}
		if (Pass == 0) FirstFrame = Section->GetActorTransform();
		else TestTrue(TEXT("Repeated compilation cannot move the section frame."), Section->GetActorTransform().Equals(FirstFrame, 0));
	}
	TestTrue(TEXT("Repeated operations cannot mutate either cached input."),
		Inputs[0]->GetVertex(0) == FVector(100, 200, 20) && Inputs[1]->GetVertex(0) == FVector(500, 200, 21));
	{
		FTransformerContext Empty;
		AddExpectedError(TEXT("refused=missing_units"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Missing input refuses observably."), Transformer.Execute(Empty));
		TestTrue(TEXT("Refusal sets the native cancellation signal."), Empty.bWasCancelled);
	}
	{
		auto Invalid = MakeShared<FMeshData>(); Invalid->Copy(*Inputs[1]);
		Invalid->SetVertex(0, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0));
		FTransformerContext Context;
		Context.TransformerUnits = {MakeTransformerUnit(Section, Inputs[0]), MakeTransformerUnit(Section, Invalid)};
		AddExpectedError(TEXT("refused=nonfinite_source_position"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("A corrupt later unit refuses the complete section."), Transformer.Execute(Context));
		TestTrue(TEXT("Refusal leaves actor transform and earlier mesh ownership unchanged."), Context.bWasCancelled &&
			Section->GetActorTransform().Equals(FirstFrame, 0) && Context.TransformerUnits[0].MeshData.Get() == Inputs[0].Get());
	}
	return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldTerrainSectionStorageContract,
	"Project.World.Realization.MeshTerrain.SectionStorageContract", "[Fast][Integration][World]")
#endif
