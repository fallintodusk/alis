// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedLayerProducer.h"
#include "ProjectWorldConstructedPlan.h"
#include "ProjectWorldConstructedOutput.h"

FProjectWorldConstructedLayerProducer& FProjectWorldConstructedLayerProducer::Get()
{ static FProjectWorldConstructedLayerProducer Instance; return Instance; }
FProjectWorldConstructedLayerProducer::FProjectWorldConstructedLayerProducer()
{
	Declaration.GeneratorId=TEXT("project_constructed_surface"); Declaration.GeneratorVersion=1;
	Declaration.DirtyGranularity=EProjectWorldDirtyGranularity::CanonicalCell;
	Declaration.CanonicalSelectors={TEXT("constructed_surfaces")}; Declaration.SpatialOwnership=TEXT("cell_local");
	Declaration.RuntimeMapping=TEXT("world_partition_spatial"); Declaration.DependencyHaloCells=0;
	Declaration.PermittedDependencies=TArray<FString>{}; Declaration.PresentationMaterialRole=TEXT("road");
	Declaration.OwnedActors.OwnedTags={FName(TEXT("ProjectWorld.ConstructedSurface.v1"))};
	Declaration.OwnedActors.CellTagPrefix=TEXT("ProjectWorld.ConstructedCell=");
}
bool FProjectWorldConstructedLayerProducer::ValidateSettings(const FString& Json,FString& Error) const
{ ProjectWorldConstructedRealization::FSettings Options; return ProjectWorldConstructedRealization::Settings(Json,Options,Error); }
bool FProjectWorldConstructedLayerProducer::HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
	FProjectWorldLayerUnitInputs& Units,FString& Error) const
{
	ProjectWorldConstructedRealization::FDependencyInputs Prepared; Units={};
	if (!ProjectWorldConstructedRealization::DependencyInputs(Inputs.Bundle,Inputs.Layer,Prepared,Error)) return false;
	Units.HashByUnit=MoveTemp(Prepared.CellHashes); return true;
}
bool FProjectWorldConstructedLayerProducer::Apply(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory,FProjectWorldRealizationResult& Result,FString& Error) const
{ return ProjectWorldConstructedRealization::ApplyOutput(Context,Inventory,Result,Error); }
bool FProjectWorldConstructedLayerProducer::CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory,FProjectWorldRealizationResult& Result,FString& Error) const
{ return ProjectWorldConstructedRealization::CaptureOutput(Context,Inventory,Result,Error); }
