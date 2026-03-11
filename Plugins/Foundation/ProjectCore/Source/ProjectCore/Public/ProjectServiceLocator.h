// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"

/**
 * Lightweight service locator providing opt-in dependency injection.
 *
 * Keeps module-level services decoupled by allowing registration and lookup
 * via template type keys. Intended for singleton-style services that need
 * to be discovered at runtime without hard references.
 *
 * Uses FName-based keys for DLL stability - each service interface must
 * define a static ServiceKey() method returning a unique FName.
 */
class PROJECTCORE_API FProjectServiceLocator final
{
public:
	/** Register or replace a service instance for the given type. */
	template <typename ServiceType>
	static void Register(const TSharedRef<ServiceType>& Service)
	{
		FScopeLock ScopeLock(&Mutex);
		Services.Add(GetTypeKey<ServiceType>(), Service);
	}

	/** Register or replace a service instance using a shared pointer. */
	template <typename ServiceType>
	static void Register(const TSharedPtr<ServiceType>& Service)
	{
		if (!Service.IsValid())
		{
			return;
		}

		FScopeLock ScopeLock(&Mutex);
		Services.Add(GetTypeKey<ServiceType>(), Service);
	}

	/** Unregister the service associated with the given type. */
	template <typename ServiceType>
	static void Unregister()
	{
		FScopeLock ScopeLock(&Mutex);
		Services.Remove(GetTypeKey<ServiceType>());
	}

	/** Resolve a registered service; returns null if not found. */
	template <typename ServiceType>
	static TSharedPtr<ServiceType> Resolve()
	{
		FScopeLock ScopeLock(&Mutex);
		if (const TSharedPtr<void>* Found = Services.Find(GetTypeKey<ServiceType>()))
		{
			// Additional null check before cast (defense-in-depth)
			// Protects against invalid service registration (should not happen)
			if (!Found->IsValid())
			{
				UE_LOG(LogCore, Warning, TEXT("ServiceLocator: Found service for type but pointer is invalid (null service registered)"));
				return nullptr;
			}

			return StaticCastSharedPtr<ServiceType>(*Found);
		}
		return nullptr;
	}

	/** True when a service is currently registered for the given type. */
	template <typename ServiceType>
	static bool IsRegistered()
	{
		FScopeLock ScopeLock(&Mutex);
		return Services.Contains(GetTypeKey<ServiceType>());
	}

	/** Clears all registered services. */
	static void Reset();

private:
	/**
	 * Get DLL-stable key via ServiceType::ServiceKey().
	 * FName is centralized in UE, so same string = same key across all DLLs.
	 */
	template <typename ServiceType>
	static FName GetTypeKey()
	{
		return ServiceType::ServiceKey();
	}

	static FCriticalSection Mutex;
	static TMap<FName, TSharedPtr<void>> Services;
};
