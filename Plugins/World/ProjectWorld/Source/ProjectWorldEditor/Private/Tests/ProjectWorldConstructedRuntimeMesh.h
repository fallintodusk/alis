// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldStoredSurface.h"
class FAutomationTestBase;
class UStaticMesh;
class UPackage;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	UStaticMesh* Compile(const FProjectWorldStoredSurface& Unit,FAutomationTestBase& Test,UPackage* Package=nullptr);
	struct FSavedCandidate
	{
		FString Mount,Directory,MountedDirectory;
		TArray<FString> PackageNames;
		FAutomationTestBase* Test=nullptr;
		static bool Unload(const TArray<UPackage*>& Packages);
		explicit FSavedCandidate(bool MapDomain=false);
		~FSavedCandidate();
		UStaticMesh* RoundTrip(const FProjectWorldStoredSurface& Unit,FAutomationTestBase& Check);
	};
}
#endif
