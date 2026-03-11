// Copyright ALIS. All Rights Reserved.

#include "Validation/LoadRequestValidator.h"
#include "ProjectLoadingLog.h"

bool FLoadRequestValidator::Validate(const FLoadRequest& Request, TArray<FText>& OutErrors)
{
	// Delegate to FLoadRequest's built-in validation
	const bool bIsValid = Request.Validate(OutErrors);

	// Log validation result for observability
	if (bIsValid)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("LoadRequestValidator: Request validated successfully: %s"),
			*Request.ToString());
	}
	else
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("LoadRequestValidator: Validation failed with %d errors for request: %s"),
			OutErrors.Num(), *Request.ToString());

		for (int32 i = 0; i < OutErrors.Num(); ++i)
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("  [%d] %s"), i + 1, *OutErrors[i].ToString());
		}
	}

	return bIsValid;
}

bool FLoadRequestValidator::IsValid(const FLoadRequest& Request)
{
	return Request.IsValid();
}

FText FLoadRequestValidator::GetErrorSummary(const TArray<FText>& Errors)
{
	if (Errors.Num() == 0)
	{
		return FText::GetEmpty();
	}

	if (Errors.Num() == 1)
	{
		return Errors[0];
	}

	// Combine multiple errors into summary
	FString Combined = FString::Printf(TEXT("Validation failed with %d errors:"), Errors.Num());
	for (int32 i = 0; i < FMath::Min(Errors.Num(), 3); ++i)
	{
		Combined += FString::Printf(TEXT(" [%d] %s"), i + 1, *Errors[i].ToString());
	}

	if (Errors.Num() > 3)
	{
		Combined += FString::Printf(TEXT(" ... and %d more"), Errors.Num() - 3);
	}

	return FText::FromString(Combined);
}
