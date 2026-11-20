#include "ProjectTimeWorldContextComponent.h"
#include "ProjectTimeZone.h"

bool UProjectTimeWorldContextComponent::IsValidContext() const
{
	if (ProfileId.IsEmpty() || ProfileHash.Len() != 64) return false;
	for (TCHAR Char : ProfileHash)
		if (!FChar::IsHexDigit(Char)) return false;
	return ProjectTimeZone::IsValid(ZoneId);
}

FString UProjectTimeWorldContextComponent::IntegrityRecord() const
{
	return ProfileId + TEXT("|") + ProfileHash + TEXT("|") + ZoneId;
}
