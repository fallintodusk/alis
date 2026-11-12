// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRuntimeRoutes.h"
#include "ProjectWorldRuntimeNavigation.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldRealizationService.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	bool BuildWalkingLanes(const FInput& Input,double Radius,TArray<FWalkLane>& Out,FString& Error)
	{
		Out.Reset(); Error.Reset(); TArray<FWalkLane> Candidate;
		if (!FMath::IsFinite(Radius) || Radius<=0 || Input.Routes.IsEmpty() || Input.Routes.Num()>32)
		{ Error=TEXT("Walking lanes require bounded explicit source routes and an actual capsule radius."); return false; }
		for (const auto& Route : Input.Routes)
		{
			const double Margin=Route.WidthMeters*50-Radius;
			if (!FMath::IsFinite(Margin) || Margin<0 || Route.Id.IsEmpty() || Route.Points.Num()<2 || Route.Points.Num()>256)
			{ Error=TEXT("The original source route must be finite, complete and wide enough for the actual capsule."); return false; }
			for (const double Offset : {0.0,-Margin,Margin})
			{
				FWalkLane Lane; Lane.RouteId=Route.Id; Lane.OffsetCentimeters=Offset;
				for (int32 I=0; I<Route.Points.Num(); ++I)
				{
					FVector2D Tangent=FVector2D::ZeroVector;
					if (I>0) Tangent+=FVector2D(Route.Points[I]-Route.Points[I-1]).GetSafeNormal();
					if (I+1<Route.Points.Num()) Tangent+=FVector2D(Route.Points[I+1]-Route.Points[I]).GetSafeNormal();
					if (FVector2D(Route.Points[0])==FVector2D(Route.Points.Last()) && Route.Points.Num()>2 && (I==0 || I==Route.Points.Num()-1))
						Tangent=FVector2D(Route.Points[1]-Route.Points[0]).GetSafeNormal()+FVector2D(Route.Points.Last()-Route.Points[Route.Points.Num()-2]).GetSafeNormal();
					if (Tangent.ContainsNaN() || Tangent.IsNearlyZero()) { Error=TEXT("A complete source section has no finite tangent."); return false; }
					Tangent.Normalize(); FVector P=Route.Points[I]+FVector(-Tangent.Y*Offset,Tangent.X*Offset,0); double Height;
					if (!ExpectedHeight(Input,P,Height)) { Error=Route.Id+TEXT(" full-width section has unknown model support."); return false; }
					P.Z=Height; Lane.Points.Add(P);
				}
				Candidate.Add(MoveTemp(Lane));
			}
		}
		Out=MoveTemp(Candidate); return true;
	}
	bool WalkRoutes(const FInput& Input,ACharacter* Character,TFunctionRef<void(float)> Tick,FAutomationTestBase& Test,FRouteMetrics& Out)
	{
		Out=FRouteMetrics(); auto* Movement=Character->GetCharacterMovement();
		if (!Test.TestTrue(TEXT("Grouped walking consumes complete explicit routes and actual local movement."),Input.Grouped && !Input.Routes.IsEmpty() && Character->IsLocallyControlled())) return false;
		TArray<FWalkLane> Lanes; FString Error;
		if (!BuildWalkingLanes(Input,Character->GetCapsuleComponent()->GetScaledCapsuleRadius(),Lanes,Error)) { Test.AddError(Error); return false; }
		for (const auto& Lane : Lanes)
		{
				const auto& Points=Lane.Points;
				Movement->StopMovementImmediately(); Character->SetActorLocation(Points[0]+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
				Movement->SetMovementMode(MOVE_Falling); for (int32 I=0; I<60; ++I) Tick(1.f/60);
				if (!Test.TestTrue(TEXT("The capsule settles on actual geometry at each route/lane start."),Movement->IsMovingOnGround())) { Test.AddError(Lane.RouteId); return false; }
				for (int32 Direction : {1,-1})
				{
					for (int32 Step=1; Step<Points.Num(); ++Step)
					{
						const int32 Index=Direction>0 ? Step : Points.Num()-1-Step; const FVector Target=Points[Index];
						const double Distance=FVector2D(Target-Character->GetActorLocation()).Length();
						const int32 Budget=FMath::CeilToInt(Distance/Movement->MaxWalkSpeed*60)+600; bool Reached=false;
						if (!Test.TestTrue(TEXT("Walking segment budget remains finite and bounded."),Budget>0 && Budget<=18000)) return false;
						for (int32 Frame=0; Frame<Budget; ++Frame)
						{
							FVector Delta=Target-Character->GetActorLocation(); Delta.Z=0;
							if (Delta.Length()<15) { Reached=true; break; }
							Character->AddMovementInput(Delta.GetSafeNormal(),1); Tick(1.f/60);
							Out.WalkingFrames+=Movement->IsMovingOnGround(); Out.FallingFrames+=Movement->IsFalling(); double Height;
							if (!ExpectedHeight(Input,Character->GetActorLocation(),Height)) ++Out.UnknownFrames;
							else Out.MaximumFootError=FMath::Max(Out.MaximumFootError,FMath::Abs(Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Height));
						}
						if (!Reached) { Test.AddError(FString::Printf(TEXT("Actual walking failed route=%s lane_cm=%.17g direction=%d node=%d location=%s target=%s"),*Lane.RouteId,Lane.OffsetCentimeters,Direction,Index,*Character->GetActorLocation().ToString(),*Target.ToString())); return false; }
						++Out.WalkingSegments;
					}
				}
		}
		Out.Routes=Lanes.Num()/3;
		Test.TestEqual(TEXT("All full source routes were physically walked in three capsule lanes both directions."),Out.Routes,Input.Routes.Num());
		Test.TestEqual(TEXT("Whole-neighborhood walking has no falling frames."),Out.FallingFrames,0);
		Test.TestEqual(TEXT("Whole-neighborhood walking never substitutes unknown model support."),Out.UnknownFrames,0);
		Test.TestTrue(TEXT("Every actual foot follows the original model within the existing 3 cm bound."),Out.MaximumFootError<=3);
		Movement->StopMovementImmediately(); Character->SetActorLocation(Input.Start+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
		Movement->SetMovementMode(MOVE_Falling); for (int32 I=0; I<60; ++I) Tick(1.f/60);
		return Movement->IsMovingOnGround() && Out.FallingFrames==0 && Out.UnknownFrames==0 && Out.MaximumFootError<=3;
	}
	bool NavRoutes(const FInput& Input,UWorld* World,ACharacter* Character,ANavMeshBoundsVolume* Bounds,
		ANavigationData*& NavData,FAutomationTestBase& Test,FRouteMetrics& Out)
	{
		FBox Box(ForceInit); for (const auto& Route : Input.Routes) for (const auto& P : Route.Points) Box+=P;
		if (!Test.TestTrue(TEXT("Complete source routes define a nonempty bounded navigation domain."),Box.IsValid!=0)) return false;
		const FVector2D A(Box.Min),Axis(FVector2D(Box.Max)-A); const double LengthSquared=Axis.SquaredLength(); double Padding=0;
		for (const auto& Route : Input.Routes) for (const auto& P : Route.Points)
		{
			const FVector2D Delta=FVector2D(P)-A; const double T=LengthSquared>0 ? FMath::Clamp(FVector2D::DotProduct(Delta,Axis)/LengthSquared,0.,1.) : 0;
			Padding=FMath::Max(Padding,(Delta-Axis*T).Length());
		}
		FProjectWorldRuntimeProfile Profile; Profile.NavigationPaddingMeters=Padding*.01+10; Profile.NavigationHeightMeters=10;
		FProjectWorldRealizationResult Receipt; FString Error;
		if (!ProjectWorldRuntimeNavigation::ConfigureRouteDomain(World,Character,Bounds,Box.Min,Box.Max,Profile,Receipt,NavData,Error)) { Test.AddError(Error); return false; }
		auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		for (const auto& Route : Input.Routes) for (int32 I=1; I<Route.Points.Num(); ++I) for (int32 Direction : {1,-1})
		{
			const FVector Start=Route.Points[Direction>0 ? I-1 : I],End=Route.Points[Direction>0 ? I : I-1]; FNavLocation From,To;
			if (!Navigation || !NavData || !Navigation->ProjectPointToNavigation(Start,From,FVector(50,50,30),NavData) ||
				!Navigation->ProjectPointToNavigation(End,To,FVector(50,50,30),NavData))
			{ Test.AddError(FString::Printf(TEXT("Native Nav projection failed route=%s segment=%d direction=%d"),*Route.Id,I,Direction)); return false; }
			FPathFindingQuery Query(Character,*NavData,From.Location,To.Location); Query.SetAllowPartialPaths(false); const auto Path=Navigation->FindPathSync(Query);
			if (!Path.IsSuccessful() || !Path.Path.IsValid() || Path.Path->IsPartial())
			{ Test.AddError(FString::Printf(TEXT("Native Nav route is incomplete route=%s segment=%d direction=%d"),*Route.Id,I,Direction)); return false; }
			for (const auto& P : Path.Path->GetPathPoints())
			{
				double Height;
				if (!ExpectedHeight(Input,P.Location,Height)) { Test.AddError(TEXT("Actual Nav path uses unknown grouped model support.")); return false; }
				Out.MaximumNavError=FMath::Max(Out.MaximumNavError,FMath::Abs(P.Location.Z-Height));
			}
			if (Out.MaximumNavError>30) { Test.AddError(TEXT("Actual Nav route exceeds the existing 30 cm model-height bound.")); return false; }
			++Out.NavSegments;
		}
		return true;
	}
}
#endif
