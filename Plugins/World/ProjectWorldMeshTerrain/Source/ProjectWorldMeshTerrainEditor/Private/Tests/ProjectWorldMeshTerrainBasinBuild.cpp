// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinSceneTest
{
    void DescribePackage(FAutomationTestBase& Test,const FString& Name)
    {
        UPackage* Package=FindPackage(nullptr,*Name);
        Test.AddInfo(FString::Printf(TEXT("BasinPackage name=%s loaded=%d flags=%u dirty=%d"),*Name,Package!=nullptr,
            Package ? uint32(Package->GetPackageFlags()) : 0,Package ? Package->IsDirty() : false));
        if (!Package) return;
        int32 Count=0;
        ForEachObjectWithPackage(Package,[&](UObject* Object)
        {
            if (Count++<8) Test.AddInfo(FString::Printf(TEXT("BasinPackageObject package=%s class=%s object=%s flags=%u editor_only=%d"),
                *Name,*Object->GetClass()->GetPathName(),*Object->GetPathName(),uint32(Object->GetFlags()),Object->IsEditorOnly()));
            return true;
        });
        Test.AddInfo(FString::Printf(TEXT("BasinPackageObjectCount package=%s objects=%d"),*Name,Count));
    }
    bool PrepareNativePlugin(FAutomationTestBase& Test,const FInput& Input)
    {
        if (!Test.TestTrue(TEXT("Disposable editor preferences are isolated under project scratch before loading maps."),
            FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(GEditorPerProjectIni),FPaths::ProjectDir()/TEXT("tmp")))) return false;
        FString Plugin=Input.Mount;Plugin.RemoveFromStart(TEXT("/"));Plugin.RemoveFromEnd(TEXT("/"));
        const FString PluginRoot=Input.Root/TEXT("runtime_plugin");
        FString ActualContent=Input.Directory,ExpectedContent=PluginRoot/TEXT("Content");
        FPaths::NormalizeDirectoryName(ActualContent);FPaths::NormalizeDirectoryName(ExpectedContent);
        if (!Test.TestTrue(TEXT("The commandlet plugin has an exact scratch-local native Content root."),
            !Plugin.IsEmpty() && Input.Mount==TEXT("/")+Plugin+TEXT("/") &&
            FPaths::IsSamePath(ActualContent,ExpectedContent))) return false;
        auto Descriptor=MakeShared<FJsonObject>();Descriptor->SetNumberField(TEXT("FileVersion"),3);Descriptor->SetNumberField(TEXT("Version"),1);
        Descriptor->SetStringField(TEXT("FriendlyName"),TEXT("Disposable basin native consumer fixture"));
        Descriptor->SetBoolField(TEXT("CanContainContent"),true);Descriptor->SetBoolField(TEXT("EnabledByDefault"),true);
        Descriptor->SetStringField(TEXT("Category"),TEXT("Project.World"));
        TArray<TSharedPtr<FJsonValue>> Dependencies;
        for (const TCHAR* Name : {TEXT("MeshPartition"),TEXT("ProjectWorldMeshTerrain"),TEXT("ProjectMaterial")})
        {auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("Name"),Name);Row->SetBoolField(TEXT("Enabled"),true);Dependencies.Add(MakeShared<FJsonValueObject>(Row));}
        Descriptor->SetArrayField(TEXT("Plugins"),Dependencies);
        FString Text;FJsonSerializer::Serialize(Descriptor,TJsonWriterFactory<>::Create(&Text));
        const FString Path=PluginRoot/(Plugin+TEXT(".uplugin"));
        if (!Test.TestTrue(TEXT("The temporary content plugin is declared without changing project or host configuration."),FFileHelper::SaveStringToFile(Text,*Path))) return false;
        FText Failure;
        if (!Test.TestTrue(TEXT("The parent registers the actual temporary plugin and its asset-reference domain."),IPluginManager::Get().AddToPluginsList(Path,&Failure)))
        {Test.AddError(Failure.ToString());return false;}
        IPluginManager::Get().MountNewlyCreatedPlugin(Plugin);
        const auto Mounted=IPluginManager::Get().FindPlugin(Plugin);
        return Test.TestTrue(TEXT("Both processes use the exact native plugin mount and content directory."),Mounted.IsValid() && Mounted->IsMounted() &&
            Mounted->GetMountedAssetPath()==Input.Mount && FPaths::IsSamePath(Mounted->GetContentDir(),ExpectedContent));
    }
    bool BuildWithNativeCommandlet(FAutomationTestBase& Test,const FInput& Input)
    {
        FString Plugin=Input.Mount;Plugin.RemoveFromStart(TEXT("/"));Plugin.RemoveFromEnd(TEXT("/"));
        const FString Path=Input.Root/TEXT("runtime_plugin")/(Plugin+TEXT(".uplugin"));
        if (!Test.TestTrue(TEXT("The builder consumes the parent's real content-plugin descriptor."),IFileManager::Get().FileSize(*Path)>0)) return false;
        const FString Log=Input.Root/TEXT("native-builder.log");
        const FString EditorState=Input.Root/TEXT("builder-editor-state.ini");
        FString Args=FString::Printf(TEXT("\"%s\" %s -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionMeshPartitionBuilder -AllowCommandletRendering -PLUGIN=\"%s\" -EnablePlugins=%s -SCCProvider=None -nop4 -unattended -nopause -nosplash -nosound -RenderOffScreen -EditorPerProjectUserSettingsINI=\"%s\" -abslog=\"%s\""),
            *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),*Input.PackageName,*Path,*Plugin,*EditorState,*Log);
        FString Graph,Cache;
        const bool HasGraph=FParse::Value(FCommandLine::Get(),TEXT("ddc="),Graph);
        const bool HasCache=FParse::Value(FCommandLine::Get(),TEXT("LocalDataCachePath="),Cache);
        if (HasGraph || HasCache)
        {
            if (!Test.TestTrue(TEXT("An explicit local DDC graph requires a scratch-confined child cache."),
                HasGraph && Graph==TEXT("InstalledNoZenLocalFallback") && HasCache && !Cache.Contains(TEXT("\"")) &&
                FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Cache),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))) return false;
            Cache=FPaths::ConvertRelativePathToFull(Cache);
            Args+=FString::Printf(TEXT(" -ddc=InstalledNoZenLocalFallback -LocalDataCachePath=\"%s\""),*Cache);
        }
        uint32 Id=0;
        FProcHandle Process=FPlatformProcess::CreateProc(FPlatformProcess::ExecutablePath(),*Args,false,true,true,&Id,0,nullptr,nullptr);
        if (!Test.TestTrue(TEXT("The native builder runs in its required separate commandlet process."),Process.IsValid())) return false;
        Test.AddInfo(FString::Printf(TEXT("BasinNativeBuilder child_pid=%u map=%s log=%s"),Id,*Input.PackageName,*Log));
        const double Started=FPlatformTime::Seconds();
        while (FPlatformProcess::IsProcRunning(Process) && FPlatformTime::Seconds()-Started<240) FPlatformProcess::SleepNoStats(.1f);
        const bool TimedOut=FPlatformProcess::IsProcRunning(Process);
        if (TimedOut) {FPlatformProcess::TerminateProc(Process,true);FPlatformProcess::WaitForProc(Process);}
        int32 Code=-1;const bool Returned=FPlatformProcess::GetProcReturnCode(Process,&Code);FPlatformProcess::CloseProc(Process);
        Test.AddInfo(FString::Printf(TEXT("BasinNativeBuilder exit_code=%d elapsed_s=%.3f timeout=%d"),Code,FPlatformTime::Seconds()-Started,TimedOut));
        return Test.TestTrue(TEXT("The bounded native builder completes successfully with a retained process log."),
            !TimedOut && Returned && Code==0 && IFileManager::Get().FileSize(*Log)>0);
    }
}
#endif
