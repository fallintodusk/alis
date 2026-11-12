// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRuntimeInput.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	bool MeasureDeckWater(const FInput& Input,double WaterMeters,double MinimumDeckHeightMeters,TMap<FString,double>& Out,FString& Error)
	{
		Out.Reset(); Error.Reset(); TMap<FString,bool> Roles; TMap<FString,double> Measured;
		if (!Input.Grouped || Input.SourceGroups.IsEmpty() || Input.Units.IsEmpty() || Input.Units.Num()!=Input.UnitGroups.Num() ||
			!FMath::IsFinite(Input.Frame.HeightOriginMeters) || !FMath::IsFinite(WaterMeters) || !FMath::IsFinite(MinimumDeckHeightMeters) || MinimumDeckHeightMeters<=0)
		{ Error=TEXT("Deck/Water measurement requires known levels, positive minimum and complete grouped input."); return false; }
		for (const auto& Group : Input.SourceGroups)
		{
			const bool Structural=Group.Role==TEXT("constructed_body") || Group.Role==TEXT("inferred_structural_apron") || Group.Role==TEXT("inferred_gate_apron");
			if (Group.Id.IsEmpty() || Roles.Contains(Group.Id) || (!Structural && Group.Role!=TEXT("inferred_ground")))
			{ Error=TEXT("Deck/Water measurement has an unknown or repeated owner role."); return false; }
			Roles.Add(Group.Id,Structural);
		}
		for (int32 I=0; I<Input.Units.Num(); ++I)
		{
			const auto* Structural=Roles.Find(Input.UnitGroups[I]); const auto& Unit=Input.Units[I];
			if (!Structural || Unit.Origin.ContainsNaN() || Unit.Triangles.IsEmpty())
			{ Error=TEXT("Deck/Water measurement has missing ownership, frame or geometry."); return false; }
			if (!*Structural) continue;
			for (const auto& Face : Unit.Triangles) for (const auto& P : Face.Points)
			{
				const double Height=(P.Z+Unit.Origin.Z)*.01+Input.Frame.HeightOriginMeters-WaterMeters;
				if (P.ContainsNaN() || !FMath::IsFinite(Height) || Height<MinimumDeckHeightMeters)
				{ Error=TEXT("Actual stored deck violates representative Water separation: ")+Input.UnitGroups[I]; return false; }
				auto* Previous=Measured.Find(Input.UnitGroups[I]);
				if (Previous) *Previous=FMath::Min(*Previous,Height); else Measured.Add(Input.UnitGroups[I],Height);
			}
		}
		for (const auto& Role : Roles) if (Role.Value && !Measured.Contains(Role.Key))
		{ Error=TEXT("Deck/Water measurement omits a required structural owner."); return false; }
		if (Measured.IsEmpty()) { Error=TEXT("Deck/Water measurement contains no structural faces."); return false; }
		Out=MoveTemp(Measured); return true;
	}
}
#endif
