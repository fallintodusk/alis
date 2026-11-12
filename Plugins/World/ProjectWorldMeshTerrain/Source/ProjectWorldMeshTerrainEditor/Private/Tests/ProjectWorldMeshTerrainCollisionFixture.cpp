#include "ProjectWorldMeshTerrainCollisionFixture.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MeshPartitionCollisionComponent.h"
#include "MeshPartitionCompiledSection.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainCollisionFixture
{
	bool ReadRequiredPoints(const FJsonObject& Document, TArray<FVector>& Out, FString& Error)
	{
		Out.Reset();Error.Reset();
		const auto Refuse=[&](const TCHAR* Message) {Error=Message;return false;};
		if (!Document.HasField(TEXT("required_physics_points"))) return true;
		const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
		if (!Document.TryGetArrayField(TEXT("required_physics_points"),Values) || Values->Num()==0 || Values->Num()>64)
			return Refuse(TEXT("Declared physics points require a bounded nonempty canonical array."));
		TArray<FVector> Candidate;
		for (const auto& Value : *Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Coordinates=nullptr;
			if (!Value.IsValid() || !Value->TryGetArray(Coordinates) || Coordinates->Num()!=3)
				return Refuse(TEXT("Every declared physics point requires three finite numeric coordinates."));
			FVector Point;
			for (int32 Axis=0;Axis<3;++Axis)
				if (!(*Coordinates)[Axis].IsValid() || (*Coordinates)[Axis]->Type!=EJson::Number ||
					!(*Coordinates)[Axis]->TryGetNumber(Point[Axis]) || !FMath::IsFinite(Point[Axis]))
					return Refuse(TEXT("Physics coordinates cannot substitute strings, null or nonfinite values."));
			Candidate.Add(Point);
		}
		Out=MoveTemp(Candidate); return true;
	}

	bool FInventory::CheckRequiredPoints(FAutomationTestBase& Test, const FProjectWorldCanonicalBundle& Bundle,
		TConstArrayView<FVector> Points, FJsonObject& Receipt) const
	{
		if (!Test.TestTrue(TEXT("Declared support queries use the current nonempty native collision inventory."),
			IsValid(World) && !Components.IsEmpty())) return false;
		TArray<TSharedPtr<FJsonValue>> Rows;
		int32 Supported=0,DisabledMisses=0,HeightFaultRefusals=0;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldRequiredPhysics),true);
		for (const FVector& Canonical : Points)
		{
			const FVector Point=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,Canonical);
			const auto Supports=[&](const FHitResult& Hit)
			{ return Accepts(Hit,Components[0],true) && FMath::IsFinite(Hit.ImpactPoint.Z) && FMath::Abs(Hit.ImpactPoint.Z-Point.Z)<=.01; };
			FHitResult Hit;
			const bool Found=World->LineTraceSingleByChannel(Hit,Point+FVector(0,0,400),Point-FVector(0,0,400),ECC_Pawn,Query);
			const bool Accepted=Found && Supports(Hit);
			Supported+=Accepted ? 1 : 0;
			Test.TestTrue(TEXT("Every retained support point has owned cooked collision at its expected height."),Accepted);
			if (Accepted)
			{
				auto Shifted=Hit; Shifted.ImpactPoint.Z+=.1;
				const bool Refused=!Supports(Shifted);HeightFaultRefusals+=Refused ? 1 : 0;
				Test.TestTrue(TEXT("The same support predicate rejects a one millimetre height fault."),Refused);
			}
			TArray<ECollisionEnabled::Type> Enabled;
			for (auto* Component : Components) {Enabled.Add(Component->GetCollisionEnabled());Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
			FHitResult Disabled;
			const bool DisabledFound=World->LineTraceSingleByChannel(Disabled,Point+FVector(0,0,400),Point-FVector(0,0,400),ECC_Pawn,Query);
			for (int32 Index=0;Index<Components.Num();++Index) Components[Index]->SetCollisionEnabled(Enabled[Index]);
			DisabledMisses+=!DisabledFound ? 1 : 0;
			Test.TestFalse(TEXT("The retained support query cannot pass with its native collision disabled."),DisabledFound);
			auto Row=MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("canonical_x"),Canonical.X);Row->SetNumberField(TEXT("canonical_y"),Canonical.Y);Row->SetNumberField(TEXT("canonical_z"),Canonical.Z);
			Row->SetBoolField(TEXT("owned_support"),Accepted);Row->SetBoolField(TEXT("disabled_hit"),DisabledFound);
			if (Found) Row->SetNumberField(TEXT("height_error_cm"),FMath::Abs(Hit.ImpactPoint.Z-Point.Z));
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Receipt.SetNumberField(TEXT("requested"),Points.Num());Receipt.SetNumberField(TEXT("supported"),Supported);
		Receipt.SetNumberField(TEXT("disabled_misses"),DisabledMisses);Receipt.SetNumberField(TEXT("height_fault_refusals"),HeightFaultRefusals);
		Receipt.SetArrayField(TEXT("points"),Rows);
		return Supported==Points.Num() && DisabledMisses==Points.Num() && HeightFaultRefusals==Points.Num();
	}

	bool FInventory::Gather(FAutomationTestBase& Test, UWorld& ExpectedWorld, TConstArrayView<UE::MeshPartition::ACompiledSection*> Sections)
	{
		World = nullptr; Components.Reset();
		TArray<UPrimitiveComponent*> Candidate;
		for (auto* Section : Sections)
		{
			if (!Test.TestTrue(TEXT("Every declared physical section is live in the actual probe world."),
				IsValid(Section) && Section->GetWorld() == &ExpectedWorld)) return false;
			for (const auto& Value : Section->GetCollisionComponents())
			{
				auto* Component = Value.Get();
				if (!Test.TestTrue(TEXT("Declared collision inventory contains only live registered components owned by its section."),
					IsValid(Component) && Component->IsRegistered() && Component->GetWorld() == &ExpectedWorld && Component->GetOwner() == Section)) return false;
				Candidate.AddUnique(Component);
			}
		}
		World = &ExpectedWorld; Components = MoveTemp(Candidate); return true;
	}

	bool FInventory::Accepts(const FHitResult& Hit, UPrimitiveComponent* Requested, bool AllowAdjacent) const
	{
		auto* Component = Hit.GetComponent();
		return Hit.bBlockingHit && IsValid(World) && IsValid(Component) && Component->IsRegistered() &&
			Component->GetWorld() == World && Components.Contains(Component) &&
			IsValid(Requested) && Requested->IsRegistered() && Requested->GetWorld() == World &&
			Components.Contains(Requested) && (AllowAdjacent || Component == Requested);
	}

	bool FInventory::Calibrate(FAutomationTestBase& Test, UPrimitiveComponent* Requested, bool AllowAdjacent) const
	{
		FHitResult Owned; Owned.bBlockingHit = true; Owned.Component = Requested;
		if (!Test.TestTrue(TEXT("The ownership predicate accepts its actual requested collider."), Accepts(Owned, Requested, AllowAdjacent))) return false;
		FHitResult Null; Null.bBlockingHit = true;
		if (!Test.TestFalse(TEXT("A null hit cannot match a declared collision entry."), Accepts(Null, Requested, AllowAdjacent))) return false;
		FHitResult NonBlocking = Owned; NonBlocking.bBlockingHit = false;
		if (!Test.TestFalse(TEXT("A nonblocking result cannot establish physical support."), Accepts(NonBlocking, Requested, AllowAdjacent))) return false;
		FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;Spawn.bCreateActorPackage=false;
		auto* Actor = World->SpawnActor<AActor>(Spawn);
		if (!Test.TestNotNull(TEXT("The foreign-owner control has an independent actor."), Actor)) return false;
		ON_SCOPE_EXIT { Actor->Destroy(); };
		auto* Foreign = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Foreign);
		Foreign->SetCollisionEnabled(ECollisionEnabled::NoCollision); Foreign->RegisterComponent();
		FHitResult Other; Other.bBlockingHit = true; Other.Component = Foreign;
		if (!Test.TestTrue(TEXT("The foreign component is live, registered and in the same world."),
			Foreign->IsRegistered() && Foreign->GetWorld() == World) ||
			!Test.TestFalse(TEXT("The same predicate rejects a genuine registered foreign owner."), Accepts(Other, Requested, AllowAdjacent))) return false;
		for (auto* Component : Components) if (Component != Requested)
		{
			FHitResult Neighbor = Owned; Neighbor.Component = Component;
			return Test.TestTrue(TEXT("Adjacent owned support is accepted only in the explicitly combined-field arm."),
				Accepts(Neighbor, Requested, true) && !Accepts(Neighbor, Requested, false));
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldPhysicsQueryInputTest,
    "Project.World.Realization.MeshTerrain.PhysicsQueryInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldPhysicsQueryInputTest::RunTest(const FString&)
{
    using ProjectWorldTerrainCollisionFixture::ReadRequiredPoints;
    FJsonObject Document; TArray<FVector> Points; FString Error;
    TestTrue(TEXT("An undeclared optional witness has no points."),ReadRequiredPoints(Document,Points,Error) && Points.IsEmpty());
    const auto Point=[](TSharedPtr<FJsonValue> X)
    {
        return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            X,MakeShared<FJsonValueNumber>(20.),MakeShared<FJsonValueNumber>(30.)});
    };
    Document.SetArrayField(TEXT("required_physics_points"),{Point(MakeShared<FJsonValueNumber>(10.))});
    TestTrue(TEXT("The declared canonical point is retained without coercion or reindexing."),
        ReadRequiredPoints(Document,Points,Error) && Points.Num()==1 && Points[0]==FVector(10,20,30));
    Document.SetArrayField(TEXT("required_physics_points"),{Point(MakeShared<FJsonValueString>(TEXT("10")))});
    TestFalse(TEXT("Numeric strings cannot masquerade as canonical measurements."),ReadRequiredPoints(Document,Points,Error));
    TestTrue(TEXT("Refusal has a diagnostic and cannot retain a stale valid point."),!Error.IsEmpty() && Points.IsEmpty());
    Document.SetArrayField(TEXT("required_physics_points"),{Point(MakeShared<FJsonValueNumber>(std::numeric_limits<double>::infinity()))});
    TestFalse(TEXT("Nonfinite numeric values refuse through the same parser."),ReadRequiredPoints(Document,Points,Error));
    Document.SetField(TEXT("required_physics_points"),MakeShared<FJsonValueNull>());
    TestFalse(TEXT("Present null is unknown data rather than an absent optional query."),ReadRequiredPoints(Document,Points,Error));
    TArray<TSharedPtr<FJsonValue>> TooMany; TooMany.Init(Point(MakeShared<FJsonValueNumber>(10.)),65);
    Document.SetArrayField(TEXT("required_physics_points"),TooMany);
    TestFalse(TEXT("The declared query array remains bounded."),ReadRequiredPoints(Document,Points,Error));
    return !HasAnyErrors();
}
#endif
