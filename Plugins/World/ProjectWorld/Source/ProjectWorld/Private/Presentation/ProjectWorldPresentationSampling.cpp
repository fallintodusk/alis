// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPresentationSampling.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace
{
	const FString RuntimeRolePrefix(TEXT("ProjectWorld.RuntimeRole="));
	const FString RuntimeProfilePrefix(TEXT("ProjectWorld.Runtime="));
	const FString RuntimeHashPrefix(TEXT("ProjectWorld.RuntimeHash="));

	FString TagValue(const AActor& Actor, const FString& Prefix)
	{
		for (const FName& Tag : Actor.Tags)
		{
			const FString Value = Tag.ToString();
			if (Value.StartsWith(Prefix))
			{
				return Value.RightChop(Prefix.Len());
			}
		}
		return FString();
	}

	bool HasTagValue(const AActor& Actor, const FString& Prefix, const FString& Expected)
	{
		return Actor.Tags.Contains(FName(*(Prefix + Expected)));
	}
}

namespace ProjectWorldPresentation
{
	FRuntimeRoleScan ScanRuntimeRoles(
		UWorld& World,
		const FString& RuntimeProfileId,
		const FString& RuntimeProfileHash)
	{
		FRuntimeRoleScan Scan;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			const FString Role = TagValue(**It, RuntimeRolePrefix);
			if (Role.IsEmpty())
			{
				continue;
			}
			if (!HasTagValue(**It, RuntimeProfilePrefix, RuntimeProfileId) ||
				!HasTagValue(**It, RuntimeHashPrefix, RuntimeProfileHash))
			{
				Scan.Error = FString::Printf(
					TEXT("A loaded actor carries runtime role '%s' with stale profile ownership."),
					*Role);
				return Scan;
			}
			if (Scan.LoadedRoles.Contains(Role))
			{
				Scan.Error = FString::Printf(
					TEXT("Runtime role '%s' is duplicated in one loaded state."),
					*Role);
				return Scan;
			}
			Scan.LoadedRoles.Add(Role);
		}
		Scan.bValid = true;
		return Scan;
	}

	bool IsValidSampleFrameMs(double FrameTimeMs)
	{
		return FMath::IsFinite(FrameTimeMs) && FrameTimeMs > 0.0;
	}

	TArray<FString> MissingRequiredRoles(const TSet<FString>& ObservedRoles)
	{
		TArray<FString> Missing;
		for (const FString& Required : {TEXT("RouteStart"), TEXT("RouteEnd"), TEXT("RouteNavigation")})
		{
			if (!ObservedRoles.Contains(Required))
			{
				Missing.Add(Required);
			}
		}
		return Missing;
	}
}
