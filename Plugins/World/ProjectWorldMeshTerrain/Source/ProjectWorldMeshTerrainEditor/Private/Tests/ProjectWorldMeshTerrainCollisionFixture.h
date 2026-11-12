#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class FAutomationTestBase;
class UPrimitiveComponent;
class UWorld;
class FJsonObject;
struct FProjectWorldCanonicalBundle;
namespace UE::MeshPartition { class ACompiledSection; }

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainCollisionFixture
{
	bool ReadRequiredPoints(const FJsonObject& Document, TArray<FVector>& Out, FString& Error);
	struct FInventory
	{
		UWorld* World = nullptr;
		TArray<UPrimitiveComponent*> Components;
		bool Gather(FAutomationTestBase& Test, UWorld& ExpectedWorld, TConstArrayView<UE::MeshPartition::ACompiledSection*> Sections);
		bool Accepts(const FHitResult& Hit, UPrimitiveComponent* Requested, bool AllowAdjacent) const;
		bool Calibrate(FAutomationTestBase& Test, UPrimitiveComponent* Requested, bool AllowAdjacent) const;
		bool CheckRequiredPoints(FAutomationTestBase& Test, const FProjectWorldCanonicalBundle& Bundle,
			TConstArrayView<FVector> Points, FJsonObject& Receipt) const;
	};
}
#endif
