// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interfaces/AssemblyTypes.h"
#include "IAssemblyCapability.generated.h"

class UActorComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssemblyStateChangedNative, EAssemblyState /* NewState */);

/**
 * Interface for assembly lifecycle coordination.
 *
 * Implemented by USkeletalAssemblyComponent (and future assembly types).
 * ObjectSpawnUtility uses this interface to manage assembly lifecycle
 * without depending on concrete assembly plugin types.
 *
 * Lifecycle only. For assembly view data, see IAssemblyViewConfigSource.
 *
 * The spawn path:
 * 1. Creates and registers all capability components
 * 2. Resolves assembly lifecycle / view-config providers from those components
 * 3. Registers deferred managed capabilities via RegisterManagedCapability()
 * 4. Drives lifecycle via RequestAssembly() then CompleteAssembly()
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAssemblyCapability : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IAssemblyCapability
{
	GENERATED_BODY()

public:
	/**
	 * Register a capability for deferred activation.
	 * Called by spawn path when assembly is present.
	 * The capability is created with bAutoActivate=false.
	 */
	virtual void RegisterManagedCapability(UActorComponent* Capability) = 0;

	/** Start assembly lifecycle (Idle -> Assembling). */
	virtual bool RequestAssembly() = 0;

	/** Complete assembly (Assembling -> Ready), activates managed capabilities. */
	virtual bool CompleteAssembly() = 0;

	/** Teardown assembly, deactivates managed capabilities. */
	virtual bool RequestTeardown() = 0;

	// -------------------------------------------------------------------------
	// State query (added for DIP -- consumers read state without concrete dep)
	// -------------------------------------------------------------------------

	/** Current assembly state. */
	virtual EAssemblyState GetCurrentAssemblyState() const = 0;

	/** Subscribe to state changes. Returns handle for unsubscription. */
	virtual FDelegateHandle AddAssemblyStateChanged(
		const FOnAssemblyStateChangedNative::FDelegate& Callback) = 0;

	/** Unsubscribe from state changes. */
	virtual void RemoveAssemblyStateChanged(FDelegateHandle Handle) = 0;

};
