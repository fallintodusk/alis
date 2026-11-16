// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldVerticalProvenanceParsing.h"

#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"

namespace ProjectWorldVerticalProvenanceParsing
{
	bool Read(const TSharedPtr<FJsonObject>& Object, FProjectWorldCanonicalTerrain& OutTerrain,
		FProjectWorldCanonicalValidation& Validation)
	{
		FString Source, Datum, Confidence;
		double Accuracy = 0.0, Residual = 0.0;
		const TSharedPtr<FJsonValue>* Value = Object->Values.Find(TEXT("source_accuracy_m"));
		const bool bKnown = Value && Value->IsValid() && (*Value)->Type == EJson::Number;
		const bool bUnknown = Value && Value->IsValid() && (*Value)->Type == EJson::Null;
		if (!Object->TryGetStringField(TEXT("source_ref"), Source) || Source.IsEmpty() ||
			!Object->TryGetStringField(TEXT("vertical_datum"), Datum) || Datum.IsEmpty() ||
			!Object->TryGetStringField(TEXT("confidence"), Confidence) || Confidence.IsEmpty() ||
			(!bKnown && !bUnknown) ||
			(bKnown && (!(*Value)->TryGetNumber(Accuracy) || !FMath::IsFinite(Accuracy) || Accuracy < 0.0)) ||
			!Object->TryGetNumberField(TEXT("sampling_quantization_residual_m"), Residual) ||
			!FMath::IsFinite(Residual) || Residual < 0.0)
		{
			Validation.ErrorCode = TEXT("terrain-provenance");
			Validation.Message = TEXT("Terrain needs explicit source provenance and finite known or null unknown accuracy.");
			return false;
		}
		OutTerrain.VerticalProvenanceId = MoveTemp(Source);
		OutTerrain.VerticalDatum = MoveTemp(Datum);
		OutTerrain.VerticalConfidence = MoveTemp(Confidence);
		OutTerrain.bHasVerticalSourceAccuracy = bKnown;
		OutTerrain.VerticalSourceAccuracyMeters = Accuracy;
		OutTerrain.SamplingQuantizationResidualMeters = Residual;
		return true;
	}
}
