// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedBoundary.h"
#include <cmath>
#include <limits>

namespace ProjectWorldConstructedBoundary
{
	bool FConversion::Select(const FVector2f& Nearest,FVector2f& Out,FString& Error)
	{
		Error.Reset();
		Out=FVector2f(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN());
		if (!Domain || Nearest.ContainsNaN()) { Error=TEXT("Native boundary needs a finite node and qualified source domain."); return false; }
		if (const auto* Known=Nodes.Find(Nearest)) { Out=*Known; return true; }
		const float Infinity=std::numeric_limits<float>::infinity();
		const float X[3]={std::nextafter(Nearest.X,-Infinity),Nearest.X,std::nextafter(Nearest.X,Infinity)};
		const float Y[3]={std::nextafter(Nearest.Y,-Infinity),Nearest.Y,std::nextafter(Nearest.Y,Infinity)};
		bool Found=false; double Best=0; FVector2f Selected=FVector2f::ZeroVector;
		auto Consider = [&](const FVector2f& P)
		{
			if (P.ContainsNaN()) return true;
			FString DomainError; const auto Result=Domain(P,DomainError);
			if (Result==ProjectWorldConstructedSurface::ESampleResult::Invalid)
			{ Error=TEXT("Native boundary source query failed: ")+DomainError; return false; }
			if (Result==ProjectWorldConstructedSurface::ESampleResult::Outside) return true;
			const double Distance=(FVector2D(P)-FVector2D(Nearest)).SquaredLength();
			if (!Found || Distance<Best || (Distance==Best && (P.X<Selected.X || (P.X==Selected.X && P.Y<Selected.Y))))
			{ Found=true; Best=Distance; Selected=P; }
			return true;
		};
		for (const float PX : X) for (const float PY : Y) if (!Consider(FVector2f(PX,PY))) return false;
		if (!Found)
		{
			// Unequal axis spacings can leave a rotated corner beyond the adjacent grid.
			// Bound the additional candidates by one coarsest physical storage interval.
			double Spacing=0;
			for (const float V : X) if (FMath::IsFinite(V)) Spacing=FMath::Max(Spacing,FMath::Abs(static_cast<double>(V)-Nearest.X));
			for (const float V : Y) if (FMath::IsFinite(V)) Spacing=FMath::Max(Spacing,FMath::Abs(static_cast<double>(V)-Nearest.Y));
			const float Step=static_cast<float>(Spacing);
			if (!FMath::IsFinite(Step) || Step<=0) { Error=TEXT("Native boundary storage interval is unknown."); return false; }
			for (const float PX : {Nearest.X-Step,Nearest.X,Nearest.X+Step})
				for (const float PY : {Nearest.Y-Step,Nearest.Y,Nearest.Y+Step})
					if (FMath::Abs(static_cast<double>(PX)-Nearest.X)<=Spacing &&
						FMath::Abs(static_cast<double>(PY)-Nearest.Y)<=Spacing && !Consider(FVector2f(PX,PY))) return false;
		}
		if (!Found) { Error=TEXT("No source-supported boundary within the bounded float storage interval."); return false; }
		Nodes.Add(Nearest,Selected); Out=Selected; return true;
	}
}
