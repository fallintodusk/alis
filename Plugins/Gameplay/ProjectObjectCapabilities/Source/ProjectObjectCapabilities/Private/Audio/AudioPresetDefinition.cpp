// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Audio/AudioPresetDefinition.h"

const FAudioTrack* UAudioPresetDefinition::FindTrack(const FString& TrackId) const
{
	for (const FAudioTrack& Track : Tracks)
	{
		if (Track.Id == TrackId)
		{
			return &Track;
		}
	}
	return nullptr;
}
