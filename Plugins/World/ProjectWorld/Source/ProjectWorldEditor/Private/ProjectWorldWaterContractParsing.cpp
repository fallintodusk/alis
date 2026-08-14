// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldWaterContractParsing.h"

#include "ProjectWorldCanonicalBundle.h"

#include "Dom/JsonObject.h"

namespace ProjectWorldWaterContractParsing
{
	namespace
	{
		bool Reject(
			FProjectWorldCanonicalValidation& Validation,
			const FString& Message,
			const FString& Detail)
		{
			Validation.ErrorCode = TEXT("water-contract");
			Validation.Message = Message;
			Validation.Detail = Detail;
			return false;
		}

		bool ReadRequiredString(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Field,
			FString& OutValue,
			FProjectWorldCanonicalValidation& Validation)
		{
			return Object->TryGetStringField(Field, OutValue) && !OutValue.IsEmpty()
				? true
				: Reject(Validation, TEXT("Canonical water string is missing."), Field);
		}

		bool ReadMembers(
			const TSharedPtr<FJsonObject>& Attributes,
			FProjectWorldCanonicalWaterSurface& OutSurface,
			FProjectWorldCanonicalValidation& Validation)
		{
			const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
			if (!Attributes->TryGetArrayField(TEXT("surface_group_members"), Members) ||
				Members == nullptr || Members->IsEmpty())
			{
				return Reject(Validation, TEXT("Canonical water group has no members."), TEXT("surface_group_members"));
			}
			TSet<FString> Unique;
			for (const TSharedPtr<FJsonValue>& Member : *Members)
			{
				FString Value;
				if (!Member->TryGetString(Value) || Value.IsEmpty() || Unique.Contains(Value))
				{
					return Reject(Validation, TEXT("Canonical water group member is invalid."), TEXT("surface_group_members"));
				}
				Unique.Add(Value);
				OutSurface.SurfaceGroupMembers.Add(MoveTemp(Value));
			}
			return true;
		}

		bool ReadFunction(
			const TSharedPtr<FJsonObject>& Attributes,
			FProjectWorldCanonicalWaterSurface& OutSurface,
			FProjectWorldCanonicalValidation& Validation)
		{
			const TSharedPtr<FJsonObject>* Function = nullptr;
			if (!Attributes->TryGetObjectField(TEXT("surface_function"), Function) ||
				Function == nullptr || !Function->IsValid() ||
				!ReadRequiredString(*Function, TEXT("function_id"), OutSurface.FunctionId, Validation) ||
				!(*Function)->TryGetNumberField(TEXT("function_version"), OutSurface.FunctionVersion))
			{
				return Reject(
					Validation,
					TEXT("Canonical water function identity is incomplete."),
					TEXT("surface_function"));
			}
			const bool bSupportedStanding = OutSurface.Behavior == TEXT("standing") &&
				OutSurface.FunctionId == TEXT("standing_polygon_quantile") &&
				OutSurface.FunctionVersion == 1;
			const bool bSupportedFlowing = OutSurface.Behavior == TEXT("flowing") &&
				OutSurface.FunctionId == TEXT("rolling_quantile_l1_isotonic") &&
				OutSurface.FunctionVersion == 1;
			if (!bSupportedStanding && !bSupportedFlowing)
			{
				return Reject(
					Validation,
					TEXT("Canonical water function identity is unsupported."),
					FString::Printf(TEXT("%s:v%d"), *OutSurface.FunctionId, OutSurface.FunctionVersion));
			}
			if (OutSurface.Behavior == TEXT("standing"))
			{
				return (*Function)->TryGetNumberField(TEXT("level_m"), OutSurface.LevelMeters) &&
					FMath::IsFinite(OutSurface.LevelMeters)
					? true
					: Reject(Validation, TEXT("Standing water has no finite level."), TEXT("level_m"));
			}

			const TArray<TSharedPtr<FJsonValue>>* Knots = nullptr;
			if (!(*Function)->TryGetArrayField(TEXT("knots"), Knots) || Knots == nullptr || Knots->Num() < 2)
			{
				return Reject(Validation, TEXT("Flowing water has fewer than two knots."), TEXT("knots"));
			}
			double PreviousZ = TNumericLimits<double>::Max();
			for (const TSharedPtr<FJsonValue>& KnotValue : *Knots)
			{
				const TArray<TSharedPtr<FJsonValue>>* Knot = nullptr;
				double Values[3] = {};
				if (!KnotValue->TryGetArray(Knot) || Knot == nullptr || Knot->Num() != 3 ||
					!(*Knot)[0]->TryGetNumber(Values[0]) ||
					!(*Knot)[1]->TryGetNumber(Values[1]) ||
					!(*Knot)[2]->TryGetNumber(Values[2]) ||
					!FMath::IsFinite(Values[0]) || !FMath::IsFinite(Values[1]) ||
					!FMath::IsFinite(Values[2]) || Values[2] > PreviousZ)
				{
					return Reject(Validation, TEXT("Flowing water knots are invalid or increase downstream."), TEXT("knots"));
				}
				OutSurface.Knots.Add(FVector(Values[0], Values[1], Values[2]));
				PreviousZ = Values[2];
			}
			return true;
		}
	}

	bool Read(
		const TSharedPtr<FJsonObject>& Attributes,
		FProjectWorldCanonicalFeature& OutFeature,
		FProjectWorldCanonicalValidation& OutValidation)
	{
		if (OutFeature.FeatureClass != TEXT("water"))
		{
			return true;
		}
		FProjectWorldCanonicalWaterSurface& Surface = OutFeature.WaterSurface;
		if (!ReadRequiredString(Attributes, TEXT("surface_group_id"), Surface.SurfaceGroupId, OutValidation) ||
			!ReadRequiredString(Attributes, TEXT("surface_geometry"), Surface.Geometry, OutValidation) ||
			!ReadRequiredString(Attributes, TEXT("surface_behavior"), Surface.Behavior, OutValidation) ||
			!ReadRequiredString(Attributes, TEXT("surface_role"), Surface.Role, OutValidation) ||
			!ReadMembers(Attributes, Surface, OutValidation))
		{
			return false;
		}
		if (!Surface.Geometry.Equals(TEXT("polygon")) && !Surface.Geometry.Equals(TEXT("ribbon")))
		{
			return Reject(OutValidation, TEXT("Canonical water geometry is unsupported."), Surface.Geometry);
		}
		if (!Surface.Behavior.Equals(TEXT("standing")) && !Surface.Behavior.Equals(TEXT("flowing")))
		{
			return Reject(OutValidation, TEXT("Canonical water behavior is unsupported."), Surface.Behavior);
		}
		if (!Surface.Role.Equals(TEXT("area")) && !Surface.Role.Equals(TEXT("flow_axis")))
		{
			return Reject(OutValidation, TEXT("Canonical water role is unsupported."), Surface.Role);
		}
		if (!ReadFunction(Attributes, Surface, OutValidation))
		{
			return false;
		}
		Surface.bValid = true;
		return true;
	}
}
