#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "ProjectWorldCanonicalBundle.h"
class FAutomationTestBase;
class FJsonObject;
class UWorld;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalSceneTest
{
    struct FScene
    {
        FAutomationTestBase& Test;FString Root,Map,Profile,ProfileHash,Plan,PlanHash;
        FProjectWorldCanonicalBundle Bundle;TSharedPtr<FJsonObject> Input,Expected;
        TSharedRef<FJsonObject> Receipt=MakeShared<FJsonObject>();
        TMap<FString,FString> SourceFiles;TArray<FString> Packages;TWeakObjectPtr<UWorld> EditorWorld;
        explicit FScene(FAutomationTestBase& In):Test(In) {}
        bool Unchanged(FString& Error) const;
        bool Finalizable(FString& Error) const;
    };
    bool ReadObject(const FString& Path,TSharedPtr<FJsonObject>& Out);
    TArray<TSharedPtr<FJsonObject>> NativeBindings(const FJsonObject& Row);
    bool Read(FScene& State,const FString& Path,const FString& Hash);
    bool ReadReference(FScene& State,const FString& Path,const FString& Hash);
    bool Census(FScene& State,UWorld* World,FString& Error,FName RequiredVariant=NAME_None);
}
#endif
