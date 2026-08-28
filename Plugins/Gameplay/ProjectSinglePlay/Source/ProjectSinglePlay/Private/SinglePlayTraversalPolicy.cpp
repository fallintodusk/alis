// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "SinglePlayTraversalPolicy.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace ProjectSinglePlayTraversal
{
	const TCHAR* OptionName()
	{
		return TEXT("Traversal");
	}

	const TCHAR* PreviewFlightValue()
	{
		return TEXT("PreviewFlight");
	}

	FSinglePlayTraversalSelection Resolve(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return {};
		}
		if (Value.Equals(PreviewFlightValue(), ESearchCase::CaseSensitive))
		{
			return {
				ESinglePlayTraversalMode::PreviewFlight,
				ESinglePlayTraversalParseResult::Supported};
		}
		return {
			ESinglePlayTraversalMode::Default,
			ESinglePlayTraversalParseResult::Unknown};
	}

	bool Apply(APawn* Pawn, ESinglePlayTraversalMode Mode)
	{
		if (Mode == ESinglePlayTraversalMode::Default)
		{
			return true;
		}

		ACharacter* Character = Cast<ACharacter>(Pawn);
		UCharacterMovementComponent* Movement =
			Character == nullptr ? nullptr : Character->GetCharacterMovement();
		if (Movement == nullptr)
		{
			return false;
		}

		Character->UnCrouch();
		Movement->SetMovementMode(MOVE_Flying);
		return Movement->MovementMode == MOVE_Flying;
	}
}
