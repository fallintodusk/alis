// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVerifyTestFlow.h"

// Mechanical consumer fixture only; source admission is exercised by RegisteredSourceOutput.
namespace ProjectWorldConstructedVerifyFixture
{
	inline const TCHAR* Material=TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	inline const ProjectWorldVerifyTestFlow::FSpec Spec{
		TEXT("Project.World.Realization.Verify.ConstructedSurface"),TEXT("ConstructedVerify"),TEXT("project_constructed_surface"),1};
	inline FProjectWorldRealizationLayer Layer()
	{
		FProjectWorldRealizationLayer Out; Out.LayerId=TEXT("structures"); Out.GeneratorId=Spec.GeneratorId; Out.GeneratorVersion=1;
		Out.DirtyGranularity=EProjectWorldDirtyGranularity::CanonicalCell; Out.CanonicalSelectors={TEXT("constructed_surfaces")};
		Out.SpatialOwnership=TEXT("cell_local"); Out.RuntimeMapping=TEXT("world_partition_spatial"); Out.ArtifactRoot=Spec.ArtifactRoot();
		Out.NormalizedSettings=TEXT("{\"maximum_join_step_m\":0.001,\"nanite\":true,\"collision\":\"complex_as_simple\",\"navigation\":\"physical_support\"}");
		return ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Out));
	}
	inline void Rectangle(FProjectWorldCanonicalConstructedSurface& Surface,double X,double Y,double W,double H,
		double GX,double GY,const TCHAR* Role)
	{
		const int32 First=Surface.Triangles.Num();
		const FVector P[4]={{X,Y,3},{X+W,Y,3+W*GX},{X+W,Y+H,3+W*GX+H*GY},{X,Y+H,3+H*GY}};
		for (const FIntVector I : {FIntVector(0,1,2),FIntVector(0,2,3)})
		{auto& Face=Surface.Triangles.AddDefaulted_GetRef(); Face.XYZ[0]=P[I.X]; Face.XYZ[1]=P[I.Y]; Face.XYZ[2]=P[I.Z];}
		auto& Region=Surface.FaceRegions.AddDefaulted_GetRef(); Region.Role=Role;
		Region.SourceIds={FString::Printf(TEXT("fixture:axis_%d"),First)}; Region.SourceTriangles={First,First+1};
		Surface.SourceProviderIds.Add(Region.SourceIds[0]);
		if (Region.Role==TEXT("gangway"))
		{Region.AxisSourceId=Region.SourceIds[0]; Region.AxisStart={X,Y}; Region.AxisEnd={X+(GX!=0 ? W : 0),Y+(GY!=0 ? H : 0)};}
	}
	inline FProjectWorldCanonicalBundle Bundle()
	{
		FProjectWorldCanonicalBundle Out; Out.GridId=TEXT("constructed_verify_grid"); Out.InputsHash=TEXT("mechanical_fixture");
		Out.CanonicalCrs=TEXT("EPSG:32618"); Out.VerticalDatum=TEXT("EPSG:3855");
		Out.CoordinateQuantizationMeters=.01; Out.HeightQuantizationMeters=.001;
		for (int32 I=0; I<2; ++I)
		{auto& Cell=Out.Cells.AddDefaulted_GetRef(); Cell.CellId=FString::Printf(TEXT("cell_%d"),I); Cell.CellX=I; Cell.Bounds=FVector4d(I*10.,0,I*10.+10,10);}
		for (const TCHAR* Id : {TEXT("fixture:fixed"),TEXT("fixture:gangways")})
		{
			FProjectWorldCanonicalFeature Feature; Feature.FeatureId=Id; Feature.FeatureClass=TEXT("constructed_surface");
			Feature.OwnerCellId=Out.Cells[0].CellId;
			FProjectWorldCanonicalConstructedSurface Surface; Surface.SourceFeatureId=Id;
			Surface.ModelHash=FString::ChrN(64,'a'); Surface.PolicyHash=FString::ChrN(64,'b'); Surface.AdmissionHash=FString::ChrN(64,'c');
			Surface.CanonicalCrs=Out.CanonicalCrs; Surface.VerticalDatum=Out.VerticalDatum;
			Surface.Basis=TEXT("identified_model"); Surface.Role=TEXT("identified_assembly");
			Surface.SourceSnapshots.AddDefaulted();
			FProjectWorldCanonicalConstructedPolicy Policy; Policy.MaximumTriangles=4096; Policy.MaximumOrdinaryGradient=.02;
			Policy.MaximumGangwayGrade=.125; Policy.MaximumCrossSlope=.02; Policy.MaximumNumericErrorMeters=1e-8;
			Surface.OperationalPolicy=Policy;
			Rectangle(Surface,Feature.FeatureId==TEXT("fixture:fixed") ? 8 : 1,1,4,1,
				Feature.FeatureId==TEXT("fixture:fixed") ? .01 : .1,0,
				Feature.FeatureId==TEXT("fixture:fixed") ? TEXT("fixed_pier") : TEXT("gangway"));
			if (Feature.FeatureId==TEXT("fixture:gangways")) Rectangle(Surface,1,5,1,4,0,.1,TEXT("gangway"));
			Feature.ConstructedSurface=MoveTemp(Surface);
			for (int32 I=0; I<(Feature.FeatureId==TEXT("fixture:fixed") ? 2 : 1); ++I)
			{
				auto& Representation=Feature.Representations.AddDefaulted_GetRef(); Representation.CellId=Out.Cells[I].CellId;
				Representation.Kind=I==0 ? TEXT("authoritative_constructed_surface") : TEXT("constructed_surface_reference");
				(I==0 ? Out.Cells[I].OwnedFeatureIds : Out.Cells[I].ReferencedFeatureIds).Add(Feature.FeatureId);
			}
			Out.Features.Add(Feature.FeatureId,MoveTemp(Feature));
		}
		return Out;
	}
}
