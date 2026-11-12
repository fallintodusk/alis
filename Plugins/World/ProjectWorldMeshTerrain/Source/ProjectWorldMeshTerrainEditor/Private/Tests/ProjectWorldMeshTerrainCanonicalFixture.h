#pragma once
#include "CoreMinimal.h"
class FAutomationTestBase;
class FJsonObject;
class UWorld;
struct FProjectWorldCanonicalBundle;
#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainCanonicalFixture
{
	bool Read(FAutomationTestBase& Test,const FJsonObject& Document,FProjectWorldCanonicalBundle& Bundle,bool Region=false);
	bool Capture(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
		const FJsonObject& Document,const FString& Root,FJsonObject& Receipt,bool Region=false);
}
#endif
