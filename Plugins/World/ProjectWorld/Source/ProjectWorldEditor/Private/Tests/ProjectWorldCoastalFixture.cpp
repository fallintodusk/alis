// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldCoastalFixture.h"
#include "ProjectWorldConstructedRuntimeInput.h"
#include "ProjectWorldConstructedRuntimeMesh.h"
#include "ProjectWorldRuntimePartitionPolicy.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
    bool SameGeometry(const FProjectWorldStoredSurface&,UStaticMesh&,FAutomationTestBase&);
}
namespace ProjectWorldCoastalFixture
{
    bool LoadUpper(const FString& Path,const FString& Hash,const FProjectWorldCanonicalBundle& Frame,FUpper& Out,FString& Error)
    {
        Out=FUpper();ProjectWorldConstructedRuntimeTest::FInput Input;
        if (!ProjectWorldConstructedRuntimeTest::Load(Input,Error,Path,Hash) || !Input.Grouped) return false;
        if (Input.Frame.CanonicalCrs!=Frame.CanonicalCrs || Input.Frame.VerticalDatum!=Frame.VerticalDatum ||
            Input.Frame.EngineGeoreferenceOriginMeters!=Frame.EngineGeoreferenceOriginMeters || Input.Frame.HeightOriginMeters!=Frame.HeightOriginMeters)
        {Error=TEXT("Retained Upper and lower canonical frames disagree.");return false;}
        FUpper Candidate;Candidate.Frame=Input.Frame;int32 Omitted=0,Faces=0;
        for (int32 I=0;I<Input.Units.Num();++I)
        {
            if (Input.UnitGroups[I]==TEXT("land"))
            {if (Input.Units[I].Triangles.Num()!=1956) {Error=TEXT("Unexpected replaced Ground inventory.");return false;}++Omitted;continue;}
            Candidate.Groups.Add(Input.UnitGroups[I]);Candidate.SourceOrdinals.Add(Input.UnitSources[I]);
            Faces+=Input.Units[I].Triangles.Num();Candidate.Units.Add(MoveTemp(Input.Units[I]));
        }
        if (Omitted!=1 || Candidate.Units.Num()!=5 || Faces!=3169)
        {Error=TEXT("Coastal Upper projection requires the complete five retained native units.");return false;}
        Out=MoveTemp(Candidate);return true;
    }
    UStaticMesh* CompileUpper(const FProjectWorldStoredSurface& Unit,FAutomationTestBase& Test,UPackage& Package)
    {return ProjectWorldConstructedRuntimeTest::Compile(Unit,Test,&Package);}
    bool SameUpper(const FProjectWorldStoredSurface& Unit,UStaticMesh& Mesh,FAutomationTestBase& Test)
    {return ProjectWorldConstructedRuntimeTest::SameGeometry(Unit,Mesh,Test);}
    bool ConfigureGrid(UWorld& World,const FVector& Origin,FString& Error)
    {
        Error.Reset();
        if (!World.GetOutermost()->GetName().StartsWith(TEXT("/ProjectWorldBasinSections")) || Origin.ContainsNaN())
        {Error=TEXT("Diagnostic grid changes require the disposable coastal world.");return false;}
        FProjectWorldRuntimePartitionSettings Settings;Settings.PartitionCount=1;Settings.CellSizeCentimeters=3000;
        Settings.LoadingRangeCentimeters=60000;Settings.bIs2D=true;Settings.bBlockOnSlowStreaming=true;bool Changed=false;
        if (!ProjectWorldRuntimePartitionPolicy::Apply(&World,Settings,Changed,Error)) return false;
        auto* WP=World.GetWorldPartition();auto* Hash=WP ? Cast<UWorldPartitionRuntimeHashSet>(WP->RuntimeHash) : nullptr;
        auto* Property=Hash ? FindFProperty<FArrayProperty>(Hash->GetClass(),TEXT("RuntimePartitions")) : nullptr;
        if (!Property) {Error=TEXT("Missing diagnostic runtime partition array.");return false;}
        FScriptArrayHelper Partitions(Property,Property->ContainerPtrToValuePtr<void>(Hash));
        if (Partitions.Num()!=1) {Error=TEXT("Diagnostic grid requires one partition.");return false;}
        const auto* Desc=reinterpret_cast<const FRuntimePartitionDesc*>(Partitions.GetRawPtr(0));
        auto* Grid=Cast<URuntimePartitionLHGrid>(Desc->MainLayer.Get());
        auto* OriginProperty=Grid ? FindFProperty<FStructProperty>(Grid->GetClass(),TEXT("Origin")) : nullptr;
        if (!OriginProperty || OriginProperty->Struct!=TBaseStructure<FVector>::Get())
        {Error=TEXT("Unknown native LHGrid origin type.");return false;}
        Grid->Modify();*OriginProperty->ContainerPtrToValuePtr<FVector>(Grid)=Origin;Grid->MarkPackageDirty();
        const auto Info=Hash->GetFixedGridInfo(NAME_None);
        if (!Info.IsSet() || Info->CellSize!=3000 || Info->Origin!=Origin || !Info->bIs2D)
        {Error=TEXT("Native fixed grid readback differs from the diagnostic request.");return false;}
        return true;
    }
    bool AddOriginalWater(const FString& Path,const FString& Hash,FProjectWorldCanonicalBundle& Bundle,FString& Error)
    {
        FString Actual;FProjectWorldCanonicalBundle Source;FProjectWorldCanonicalValidation Validation;
        if (Hash.Len()!=64 || !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Hash ||
            !FProjectWorldCanonicalLoader::Load(Path,Source,Validation))
        {Error=TEXT("Original coastal Water source failed canonical authentication: ")+Validation.Message;return false;}
        const auto* Feature=Source.Features.Find(TEXT("alis:osm:relation:2389611"));
        if (!Feature || Bundle.Cells.Num()!=1 || !Bundle.Features.IsEmpty() || !Feature->WaterSurface.bValid ||
            Feature->WaterSurface.FunctionId!=TEXT("standing_polygon_quantile") || Feature->WaterSurface.LevelMeters!=0 ||
            Source.CanonicalCrs!=Bundle.CanonicalCrs || Source.VerticalDatum!=Bundle.VerticalDatum ||
            Source.HeightOriginMeters!=Bundle.HeightOriginMeters || Source.EngineGeoreferenceOriginMeters!=Bundle.EngineGeoreferenceOriginMeters)
        {Error=TEXT("Coastal fixture requires original ordinary Hudson Water and the same native frame.");return false;}
        // Only test ownership is projected; the complete source polygon and stage remain unchanged.
        auto Copy=*Feature;Copy.OwnerCellId=Bundle.Cells[0].CellId;
        Bundle.Cells[0].OwnedFeatureIds={Copy.FeatureId};Bundle.Features.Add(Copy.FeatureId,MoveTemp(Copy));
        return true;
    }
}
#endif
