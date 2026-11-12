// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadFormation.h"
#include "ProjectWorldCanonicalBundle.h"

namespace ProjectWorldRoadFormation
{
	namespace
	{
		void Token(FString& Target,const FString& Value) {Target+=FString::Printf(TEXT("%d:"),Value.Len()); Target+=Value;}
		void Number(FString& Target,double Value) {Token(Target,FString::Printf(TEXT("%.17g"),Value));}
		FVector2D Tangent(const FProjectWorldCanonicalFeature& Feature,const FVector2D& Point,double Tolerance,FString& Context)
		{
			FVector2D Sum=FVector2D::ZeroVector,FirstDirection=FVector2D::ZeroVector;
			Token(Context,TEXT("tangent")); Number(Context,Point.X); Number(Context,Point.Y);
			auto Accumulate=[&](const TArray<FVector2D>& Part)
			{
				for (int32 Index=0; Index+1<Part.Num(); ++Index)
				{
					const FVector2D Segment=Part[Index+1]-Part[Index]; const double LengthSquared=Segment.SquaredLength();
					if (LengthSquared<=UE_DOUBLE_SMALL_NUMBER) continue;
					const double Alpha=FMath::Clamp(FVector2D::DotProduct(Point-Part[Index],Segment)/LengthSquared,0.,1.);
					const double DistanceSquared=(Point-(Part[Index]+Segment*Alpha)).SquaredLength();
					const FVector2D Direction=Segment.GetSafeNormal();
					if (DistanceSquared<=Tolerance*Tolerance)
					{
						if (FirstDirection.IsNearlyZero()) FirstDirection=Direction;
						Sum+=Direction; Number(Context,Part[Index].X); Number(Context,Part[Index].Y);
						Number(Context,Part[Index+1].X); Number(Context,Part[Index+1].Y);
					}
				}
			};
			if (Feature.GeometryParts.IsEmpty()) Accumulate(Feature.GeometryPoints);
			else for (const auto& Part : Feature.GeometryParts) Accumulate(Part);
			return Sum.IsNearlyZero() ? FirstDirection : Sum.GetSafeNormal();
		}
	}
	TStaticArray<FProjectWorldSurfaceTriangle,2> Faces(const FSegment& S,double Height)
	{
		TStaticArray<FProjectWorldSurfaceTriangle,2> Out;
		Out[0]={{FVector(S.A+S.SideA,Height),FVector(S.A-S.SideA,Height),FVector(S.B+S.SideB,Height)}};
		Out[1]={{FVector(S.A-S.SideA,Height),FVector(S.B-S.SideB,Height),FVector(S.B+S.SideB,Height)}};
		return Out;
	}
	bool ForCell(const FProjectWorldCanonicalFeature& Feature,const FString& CellId,double MaximumSegmentLength,
		double TangentTolerance,TArray<FSegment>& Out,FString& Consumed,FString& Error)
	{
		Out.Reset(); Consumed.Reset(); Error.Reset(); TArray<FSegment> Candidate; FString Identity;
		if (!FMath::IsFinite(Feature.WidthMeters) || Feature.WidthMeters<=0 || !FMath::IsFinite(MaximumSegmentLength) ||
			MaximumSegmentLength<=0 || !FMath::IsFinite(TangentTolerance) || TangentTolerance<0)
		{Error=TEXT("Road formation requires finite positive width and subdivision settings."); return false;}
		for (const auto& Part : Feature.GeometryParts) for (const auto& Point : Part) if (Point.ContainsNaN())
		{Error=TEXT("Road tangent context contains a nonfinite point."); return false;}
		for (const auto& Point : Feature.GeometryPoints) if (Point.ContainsNaN())
		{Error=TEXT("Road tangent context contains a nonfinite point."); return false;}
		const double HalfWidth=Feature.WidthMeters*.5;
		for (const auto& Representation : Feature.Representations)
		{
			if (Representation.CellId!=CellId || Representation.Kind!=TEXT("road_fragment")) continue;
			for (const auto& Part : Representation.Parts)
			{
				Token(Identity,FString::FromInt(Part.Num()));
				for (const auto& Point : Part)
				{
					if (Point.ContainsNaN()) {Error=TEXT("Road fragment contains a nonfinite point."); return false;}
					Number(Identity,Point.X); Number(Identity,Point.Y);
				}
				for (int32 Index=0; Index+1<Part.Num(); ++Index)
				{
					const FVector2D Start=Part[Index],End=Part[Index+1];
					const double Required=(End-Start).Length()/MaximumSegmentLength;
					if (!FMath::IsFinite(Required) || Required>65536 || Candidate.Num()+FMath::Max(1.,FMath::CeilToDouble(Required))>65536)
					{Error=TEXT("Road formation exceeds its finite 65536-segment cell-feature capacity."); return false;}
					const int32 Steps=FMath::Max(1,FMath::CeilToInt(Required));
					for (int32 Step=0; Step<Steps; ++Step)
					{
						const FVector2D A=FMath::Lerp(Start,End,static_cast<double>(Step)/Steps);
						const FVector2D B=FMath::Lerp(Start,End,static_cast<double>(Step+1)/Steps);
						const FVector2D TA=Tangent(Feature,A,TangentTolerance,Identity),TB=Tangent(Feature,B,TangentTolerance,Identity);
						if (TA.ContainsNaN() || TB.ContainsNaN() || TA.IsNearlyZero() || TB.IsNearlyZero())
						{Error=TEXT("Road fragment has no bounded canonical tangent context."); return false;}
						Candidate.Add({A,B,FVector2D(-TA.Y*HalfWidth,TA.X*HalfWidth),FVector2D(-TB.Y*HalfWidth,TB.X*HalfWidth)});
					}
				}
			}
		}
		Out=MoveTemp(Candidate); Consumed=MoveTemp(Identity); return true;
	}
}
