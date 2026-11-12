#include "ProjectBuildingAssemblyRealization.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProjectBuildingAssembly
{
    bool ReadKit(const FString& Path,FKit& Out,FString& Error)
    {
        const auto String=[](const FJsonObject& O,const TCHAR* Key,FString& Value)
        {const auto V=O.TryGetField(Key);return V && V->Type==EJson::String && V->TryGetString(Value);};
        const auto Number=[](const FJsonObject& O,const TCHAR* Key,double& Value)
        {const auto V=O.TryGetField(Key);return V && V->Type==EJson::Number && V->TryGetNumber(Value) && FMath::IsFinite(Value);};
        FString Text;TSharedPtr<FJsonObject> D;
        if (!FFileHelper::LoadFileToString(Text,*Path) || Text.Len()>65536 ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),D) || !D || D->Values.Num()!=5)
        {Error=TEXT("Modular kit must be a bounded readable document.");return false;}
        FString Schema,Id,Generator;const TSharedPtr<FJsonObject>* P=nullptr;const TSharedPtr<FJsonObject>* V=nullptr;
        const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("ProjectBuildingAssembly"));
        if (!String(*D,TEXT("$schema"),Schema) || !FPaths::IsRelative(Schema) || !Plugin ||
            !FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::GetPath(Path)/Schema),
                FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("Data/Schemas/modular-kit.schema.json"))) ||
            !String(*D,TEXT("kit_id"),Id) || Id.IsEmpty() ||
            !String(*D,TEXT("generator"),Generator) || Generator!=TEXT("original_box_modules_v1") ||
            !D->TryGetObjectField(TEXT("provenance"),P) || (*P)->Values.Num()!=2 ||
            !String(**P,TEXT("kind"),Id) || Id!=TEXT("original_procedural_test_geometry") ||
            !String(**P,TEXT("owner"),Id) || Id!=TEXT("project") ||
            !D->TryGetObjectField(TEXT("parameters"),V) || (*V)->Values.Num()!=9)
        {Error=TEXT("Modular kit identity or original-test provenance is unsupported.");return false;}
        FKit K;double Every=0,Capacity=0;
        if (!Number(**V,TEXT("panel_width_m"),K.PanelWidth) ||
            !Number(**V,TEXT("floor_height_m"),K.FloorHeight) ||
            !Number(**V,TEXT("wall_thickness_m"),K.WallThickness) ||
            !Number(**V,TEXT("slab_thickness_m"),K.SlabThickness) ||
            !Number(**V,TEXT("foundation_embed_m"),K.FoundationEmbed) ||
            !Number(**V,TEXT("maximum_support_span_m"),K.MaximumSupportSpan) ||
            !Number(**V,TEXT("frame_fraction"),K.FrameFraction) ||
            !Number(**V,TEXT("window_every"),Every) || !FMath::IsFinite(Every) || Every<1 || Every>100 || Every!=FMath::FloorToDouble(Every) ||
            !Number(**V,TEXT("maximum_instances"),Capacity) || !FMath::IsFinite(Capacity) || Capacity<4 || Capacity>65536 || Capacity!=FMath::FloorToDouble(Capacity))
        {Error=TEXT("Modular kit parameters are incomplete or invalid.");return false;}
        K.WindowEvery=static_cast<int>(Every);K.MaximumInstances=static_cast<int>(Capacity);
        FInput Probe;Probe.FeatureId="kit-admission";Probe.Width=10;Probe.Depth=10;Probe.Height=6;Probe.Kit=K;
        FPlan Plan;std::string Why;
        if (!Build(Probe,Plan,Why)) {Error=UTF8_TO_TCHAR(Why.c_str());return false;}
        Out=K;return true;
    }
}
