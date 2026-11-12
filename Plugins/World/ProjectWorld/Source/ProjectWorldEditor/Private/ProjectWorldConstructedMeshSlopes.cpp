// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedMesh.h"

namespace ProjectWorldConstructedMesh
{
	bool ValidateSlopes(const TArray<FProjectWorldSurfaceTriangle>& Faces,const FVector2D& AlongAxis,
		double MaximumGrade,double MaximumCrossSlope,FSlopeMetrics& Metrics,FString& Error)
	{
		Metrics=FSlopeMetrics(); Error.Reset();
		if (Faces.IsEmpty() || AlongAxis.ContainsNaN() || !FMath::IsFinite(AlongAxis.Length()) || AlongAxis.Length()==0 ||
			!FMath::IsFinite(MaximumGrade) || !FMath::IsFinite(MaximumCrossSlope) || MaximumGrade<0 || MaximumCrossSlope<0)
		{ Error=TEXT("Stored slope acceptance requires actual faces, a finite axis and explicit finite limits."); return false; }
		const auto Along=AlongAxis.GetSafeNormal(0.0); const FVector2D Across(-Along.Y,Along.X);
		for (int32 I=0; I<Faces.Num(); ++I)
		{
			const auto& T=Faces[I]; double Height;
			if (!ProjectWorldTerrainSurface::Height(T,FVector2D(T.Points[0]),Height,Error)) return false;
			const FVector U=T.Points[1]-T.Points[0],V=T.Points[2]-T.Points[0];
			const double D=U.X*V.Y-U.Y*V.X;
			const FVector2D Gradient((U.Z*V.Y-U.Y*V.Z)/D,(U.X*V.Z-U.Z*V.X)/D);
			const double Grade=FMath::Abs(FVector2D::DotProduct(Gradient,Along));
			const double Cross=FMath::Abs(FVector2D::DotProduct(Gradient,Across));
			if (!FMath::IsFinite(Grade) || !FMath::IsFinite(Cross))
			{ Error=TEXT("Stored surface has an unknown slope."); return false; }
			++Metrics.FaceCount;
			Metrics.MaximumGrade=FMath::Max(Metrics.MaximumGrade,Grade);
			Metrics.MaximumCrossSlope=FMath::Max(Metrics.MaximumCrossSlope,Cross);
			if ((Grade>MaximumGrade || Cross>MaximumCrossSlope) && Metrics.FirstFailingFace==INDEX_NONE) Metrics.FirstFailingFace=I;
		}
		if (Metrics.FirstFailingFace!=INDEX_NONE)
		{
			Error=FString::Printf(TEXT("Stored surface slope exceeds frozen limits: max_grade=%.17g limit=%.17g max_cross=%.17g limit=%.17g first_face=%d."),
				Metrics.MaximumGrade,MaximumGrade,Metrics.MaximumCrossSlope,MaximumCrossSlope,Metrics.FirstFailingFace);
			return false;
		}
		return true;
	}
}
