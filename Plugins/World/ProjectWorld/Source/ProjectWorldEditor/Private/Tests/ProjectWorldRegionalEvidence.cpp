// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldRegionalEvidence.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldEvidenceCapture.h"
#include "ProjectWorldPresentationProfile.h"
#include "ProjectWorldPresentationRealization.h"
#include "ProjectWorldRealizationService.h"
#include "Dom/JsonObject.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalEvidence
{
    namespace
    {
        bool DisposablePIE(UWorld& World,FString& Error)
        {
            int32 Instance=INDEX_NONE;FString File;
            const FString Source=UWorld::RemovePIEPrefix(World.GetOutermost()->GetName(),&Instance);
            if (World.WorldType==EWorldType::PIE && Instance>=0 &&
                FPackageName::TryConvertLongPackageNameToFilename(Source,File,FPackageName::GetMapPackageExtension()) &&
                FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(File),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
                IFileManager::Get().FileExists(*File)) return true;
            Error=TEXT("Regional evidence requires the disposable PIE world, never an editor preview.");return false;
        }
    }
    bool Configure(UWorld& World,const FProjectWorldCanonicalBundle& Bundle,const FString& ProfilePath,
        const FString& ProfileHash,FJsonObject& Receipt,FString& Error)
    {
        Error.Reset();FString Actual,Code;
        if (!DisposablePIE(World,Error)) return false;
        if (ProfileHash.Len()!=64 || !FProjectWorldCanonicalLoader::ComputeFileSha256(ProfilePath,Actual) || Actual!=ProfileHash)
        {Error=TEXT("Regional presentation refuses stale or unreadable profile bytes before applying actors.");return false;}
        FProjectWorldPresentationProfile Profile;FProjectWorldPresentationResources Resources;
        FProjectWorldRealizationResult Result;
        if (!ProjectWorldPresentationProfile::Load(ProfilePath,Profile,Code,Error) ||
            !ProjectWorldPresentationProfile::ResolveResources(Profile,Resources,Error) ||
            !ProjectWorldPresentationRealization::Apply(&World,Bundle,Profile,Resources,Result,Error)) return false;
        int32 Suns=0,Volumes=0;double ObservedEV=0;
        for (TActorIterator<ADirectionalLight> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.PresentationRole=Sun")))
        {
            ++Suns;
            if (!It->GetActorRotation().Equals(Profile.SunRotation,.001) ||
                !FMath::IsNearlyEqual(It->GetComponent()->Intensity,Profile.SunIntensityLux))
            {Error=TEXT("Actual regional Sun rotation/intensity differs from the selected profile.");return false;}
            Receipt.SetNumberField(TEXT("actual_sun_intensity_lux"),It->GetComponent()->Intensity);
        }
        for (TActorIterator<APostProcessVolume> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.PresentationRole=PostProcess")))
        {
            ++Volumes;const auto& Settings=It->Settings;
            ObservedEV=FMath::Log2(FMath::Square(double(Settings.DepthOfFieldFstop))*Settings.CameraShutterSpeed*100./Settings.CameraISO);
            if (!It->bUnbound || Settings.AutoExposureMethod!=AEM_Manual || !Settings.AutoExposureApplyPhysicalCameraExposure ||
                !Settings.bOverride_AutoExposureMethod || !Settings.bOverride_AutoExposureApplyPhysicalCameraExposure ||
                !Settings.bOverride_CameraISO || !Settings.bOverride_CameraShutterSpeed || !Settings.bOverride_DepthOfFieldFstop ||
                !FMath::IsFinite(ObservedEV) || !FMath::IsNearlyEqual(ObservedEV,double(Profile.FixedExposureEv100),1e-5))
            {Error=TEXT("Actual regional exposure differs from the selected profile's physical EV100.");return false;}
        }
        if (Suns!=1 || Volumes!=1) {Error=TEXT("Regional presentation requires one actual Sun and one unbound exposure owner.");return false;}
        Receipt.SetNumberField(TEXT("actual_fixed_exposure_ev100"),ObservedEV);
        Receipt.SetStringField(TEXT("presentation_profile_sha256"),Actual);
        Receipt.SetStringField(TEXT("presentation_profile_id"),Profile.ProfileId);
        Receipt.SetNumberField(TEXT("presentation_actors"),Result.PresentationActorCount);
        Receipt.SetNumberField(TEXT("sun_intensity_lux"),Profile.SunIntensityLux);
        Receipt.SetNumberField(TEXT("fixed_exposure_ev100"),Profile.FixedExposureEv100);
        return true;
    }
    bool Capture(UWorld& World,const FString& PlanPath,const FString& PlanHash,const FString& Root,
        FJsonObject& Receipt,FString& Error)
    {
        Error.Reset();if (!DisposablePIE(World,Error)) return false;
        const FString Absolute=FPaths::ConvertRelativePathToFull(Root);
        const FString Tmp=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"));
        if (!FPaths::IsUnderDirectory(Absolute,Tmp) || IFileManager::Get().DirectoryExists(*Absolute))
        {Error=TEXT("Regional capture requires a fresh confined scratch directory.");return false;}
        int32 Width=0,Height=0;TArray<FProjectWorldCaptureVantage> Vantages;FString Actual;
        if (!ProjectWorldEvidenceCapture::LoadVantagePlan(PlanPath,Width,Height,Vantages,Actual,Error)) return false;
        if (PlanHash.Len()!=64 || Actual!=PlanHash)
        {Error=TEXT("Regional capture refuses stale camera-plan bytes before writing images.");return false;}
        FProjectWorldCaptureResult Result;Result.MapPackage=World.GetOutermost()->GetName();Result.VantagePlanSha256=Actual;
        const bool Captured=ProjectWorldEvidenceCapture::CaptureVantages(&World,Vantages,Width,Height,Absolute,Result,Error);
        Result.Status=Captured && Result.bViewsPairwiseDistinct ? TEXT("accepted") : TEXT("rejected");
        Result.Message=Captured ? (Result.bViewsPairwiseDistinct ? TEXT("Captured settled PIE views; geographic acceptance is separate.") :
            TEXT("Distinct vantages returned identical images.")) : Error;
        if (!ProjectWorldEvidenceCapture::WriteReceipt(Result,Absolute/TEXT("capture.json"),Error)) return false;
        if (Result.Status!=TEXT("accepted")) {Error=Result.Message;return false;}
        Receipt.SetStringField(TEXT("capture_receipt"),Absolute/TEXT("capture.json"));
        Receipt.SetNumberField(TEXT("captured_views"),Result.Views.Num());
        Receipt.SetBoolField(TEXT("views_pairwise_distinct"),Result.bViewsPairwiseDistinct);
        return Result.Status==TEXT("accepted");
    }
}
#endif
