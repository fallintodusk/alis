#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ProjectWorldRealizationProfile.h"
class FAutomationTestBase;
class FJsonObject;
class UWorld;
class UStaticMeshComponent;
struct FProjectWorldCanonicalBundle;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionDomain
{
    bool Selection(const FJsonObject& Document,bool Region,TSet<FString>& Cells,FString& Primary,FString& Error);
    bool QueryCell(const FJsonObject& Query,const FString& Primary,FString& Out);
    bool ExactWaterBindings(TConstArrayView<FString> Expected,TConstArrayView<FString> Observed);
    bool VerifyInputContracts(FAutomationTestBase& Test);
    bool RestoreCollision(UStaticMeshComponent& Component,ECollisionEnabled::Type Mode,FName Profile);
    struct FStructures
    {
        FProjectWorldRealizationProfile Profile;
        TSharedPtr<FJsonObject> Before;
        FString Feature;
    };
    bool ApplyStructures(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,
        const FProjectWorldCanonicalBundle& Bundle,const FString& Mount,TArray<FString>& Packages,FStructures& State);
    bool ObserveStructures(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        FStructures& State,FJsonObject& Receipt,bool Reloaded);
    class FIsolatedTerrain
    {
        FAutomationTestBase& Test;
        struct FOriginal {UStaticMeshComponent* Component;ECollisionEnabled::Type Mode;FName Profile;};
        TArray<FOriginal> Components;
        bool Valid=false,Applied=false,Restored=false;
    public:
        FIsolatedTerrain(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle);
        ~FIsolatedTerrain();
        bool IsValid() const {return Valid;}
        bool Restore();
        TSharedPtr<FJsonObject> Receipt() const;
    };
}
#endif
