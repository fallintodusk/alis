// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IProjectActionReceiver.generated.h"

/**
 * Generic action receiver for capability communication.
 *
 * Capabilities on the same actor can communicate via namespaced action strings
 * without knowing about each other. Dispatchers (e.g. Dialogue) iterate all
 * components implementing this interface and call HandleAction().
 *
 * Action format: "namespace.command:argument"
 * Examples: "audio.play:katyusha", "audio.stop", "anim.open_lid"
 * Each receiver filters by its own namespace prefix.
 */
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class PROJECTCORE_API UProjectActionReceiver : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IProjectActionReceiver
{
	GENERATED_BODY()

public:
	/**
	 * Handle a generic action dispatched by another capability.
	 *
	 * @param Context Source context: node ID, "$conversation_end", or other system marker
	 * @param Action Namespaced action string, e.g. "audio.play:classical_01"
	 */
	virtual void HandleAction(const FString& Context, const FString& Action) = 0;
};
